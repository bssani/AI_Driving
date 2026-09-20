// Copyright Epic Games, Inc. All Rights Reserved.

#include "VRSpectatorUISubsystem.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HeadMountedDisplayTypes.h"
#include "IHeadMountedDisplay.h"
#include "ISpectatorScreenController.h"
#include "IXRTrackingSystem.h"
#include "Slate/WidgetRenderer.h"
#include "VRSpectatorUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRSpectatorUI, Log, All);

namespace
{
	/** How often a pending request is retried. Often enough to come up with the headset, rarely
	 *  enough that failing costs nothing */
	constexpr float PendingRetryInterval = 0.5f;

	/** How long to keep trying before deciding there is no headset coming. Generous: a VR preview
	 *  can take several seconds to bring the spectator screen up */
	constexpr float PendingGiveUpAfter = 30.0f;
}

namespace
{
	ISpectatorScreenController* GetSpectatorScreen()
	{
		if (!GEngine || !GEngine->XRSystem.IsValid())
		{
			return nullptr;
		}

		IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice();

		return HMD ? HMD->GetSpectatorScreenController() : nullptr;
	}
}

bool UVRSpectatorUISubsystem::ShowSpectatorWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	// clears any pending request too: an explicit Show replaces whatever was being waited for
	HideSpectatorWidget();

	if (!WidgetClass)
	{
		return false;
	}

	ISpectatorScreenController* Screen = GetSpectatorScreen();

	if (!Screen)
	{
		// no headset means no second screen to serve, and the widget would have nowhere to go
		return false;
	}

	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;

	if (!Controller)
	{
		return false;
	}

	SpectatorWidget = CreateWidget<UUserWidget>(Controller, WidgetClass);

	if (!SpectatorWidget)
	{
		return false;
	}

	// deliberately not AddToViewport: that is the path that reaches the headset
	SlateWidget = SpectatorWidget->TakeWidget();

	WidgetRenderer = MakeShared<FWidgetRenderer>(true);
	RenderTarget = FWidgetRenderer::CreateTargetFor(FVector2D(DrawSize.X, DrawSize.Y), TF_Bilinear, false);

	if (!RenderTarget.Get())
	{
		HideSpectatorWidget();
		return false;
	}

	// eye image underneath filling the screen, UI over the top of it with its alpha respected
	Screen->SetSpectatorScreenModeTexturePlusEyeLayout(FSpectatorScreenModeTexturePlusEyeLayout(
		FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
		/*bDrawEyeFirst*/ true, /*bClearBlack*/ false, /*bUseAlpha*/ true));

	Screen->SetSpectatorScreenTexture(RenderTarget);
	Screen->SetSpectatorScreenMode(ESpectatorScreenMode::TexturePlusEye);

	return true;
}

void UVRSpectatorUISubsystem::HideSpectatorWidget()
{
	if (ISpectatorScreenController* Screen = GetSpectatorScreen())
	{
		Screen->SetSpectatorScreenTexture(nullptr);
		Screen->SetSpectatorScreenMode(ESpectatorScreenMode::SingleEyeCroppedToFill);
	}

	SlateWidget.Reset();
	WidgetRenderer.Reset();
	SpectatorWidget = nullptr;
	RenderTarget = nullptr;
	RedrawCountdown = 0.f;
	PendingWidgetClass = nullptr;
}

void UVRSpectatorUISubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UVRSpectatorUISettings* Settings = GetDefault<UVRSpectatorUISettings>();

	if (Settings->SpectatorWidgetClass.IsNull())
	{
		return;
	}

	// loaded here rather than held as a hard reference: a renamed or deleted widget should cost
	// an empty monitor and a log line, not a map that will not open
	UClass* WidgetClass = Settings->SpectatorWidgetClass.LoadSynchronous();

	if (!WidgetClass)
	{
		UE_LOG(LogVRSpectatorUI, Warning,
			TEXT("Spectator widget '%s' could not be loaded. The monitor will show the plain eye image."),
			*Settings->SpectatorWidgetClass.ToString());
		return;
	}

	RequestSpectatorWidget(WidgetClass);
}

void UVRSpectatorUISubsystem::RequestSpectatorWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	if (ShowSpectatorWidget(WidgetClass))
	{
		return;
	}

	// too early rather than wrong: at BeginPlay there is usually neither a headset nor a player
	// controller, and both arrive on their own a moment later
	PendingWidgetClass = WidgetClass;
	PendingRetryCountdown = PendingRetryInterval;
	PendingElapsed = 0.f;
}

void UVRSpectatorUISubsystem::TickPendingRequest(float DeltaTime)
{
	PendingElapsed += DeltaTime;
	PendingRetryCountdown -= DeltaTime;

	if (PendingRetryCountdown > 0.f)
	{
		return;
	}

	PendingRetryCountdown = PendingRetryInterval;

	// ShowSpectatorWidget clears the pending class through HideSpectatorWidget, so hold it
	const TSubclassOf<UUserWidget> Wanted = PendingWidgetClass;

	if (ShowSpectatorWidget(Wanted))
	{
		UE_LOG(LogVRSpectatorUI, Log, TEXT("Spectator overlay '%s' up after %.1fs."),
			*Wanted->GetName(), PendingElapsed);
		return;
	}

	if (PendingElapsed >= PendingGiveUpAfter)
	{
		UE_LOG(LogVRSpectatorUI, Warning,
			TEXT("Gave up putting '%s' on the spectator screen after %.0fs. There is no headset, ")
			TEXT("so there is no second screen to draw it on. Outside VR the monitor is the ")
			TEXT("driver's view and an overlay would be in their way."),
			*Wanted->GetName(), PendingGiveUpAfter);

		PendingWidgetClass = nullptr;
		return;
	}

	PendingWidgetClass = Wanted;
}

void UVRSpectatorUISubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PendingWidgetClass)
	{
		TickPendingRequest(DeltaTime);
	}

	if (!SpectatorWidget || !RenderTarget || !WidgetRenderer.IsValid() || !SlateWidget.IsValid())
	{
		return;
	}

	RedrawCountdown -= DeltaTime;

	if (RedrawCountdown > 0.f)
	{
		return;
	}

	const float Interval = 1.f / FMath::Max(RedrawsPerSecond, 1.f);

	// counted from now rather than added to what is owed, so a hitch does not leave the overlay
	// owing a burst of redraws it would then do back to back
	RedrawCountdown = Interval;

	// the accumulated interval is what the widget has actually lived through, so animations and
	// timers inside it advance at the right rate despite being drawn less often
	WidgetRenderer->DrawWidget(RenderTarget, SlateWidget.ToSharedRef(),
		FVector2D(DrawSize.X, DrawSize.Y), Interval);
}

void UVRSpectatorUISubsystem::Deinitialize()
{
	HideSpectatorWidget();

	Super::Deinitialize();
}
