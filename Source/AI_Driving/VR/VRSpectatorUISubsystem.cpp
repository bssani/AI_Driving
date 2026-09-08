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
}

void UVRSpectatorUISubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!SpectatorWidget || !RenderTarget || !WidgetRenderer.IsValid() || !SlateWidget.IsValid())
	{
		return;
	}

	// redrawn every frame so anything the widget shows can change - lap, position, speed
	WidgetRenderer->DrawWidget(RenderTarget, SlateWidget.ToSharedRef(),
		FVector2D(DrawSize.X, DrawSize.Y), DeltaTime);
}

void UVRSpectatorUISubsystem::Deinitialize()
{
	HideSpectatorWidget();

	Super::Deinitialize();
}
