// Copyright Epic Games, Inc. All Rights Reserved.

#include "VRSpectatorProbeWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "VRSpectatorUISubsystem.h"

/**
 *  vr.SpectatorProbe puts a screen-space UMG widget on the viewport and takes it away again.
 *
 *  Turn it on, look at the monitor, then look inside the headset. The expected answer is monitor
 *  yes, headset no; anything else means the spectator plan needs rethinking.
 *
 *  It goes up the same way the vehicle UI does - CreateWidget on the player controller, then
 *  AddToViewport - because a probe that took a different route would not be answering the question.
 */
namespace
{
	TWeakObjectPtr<UVRSpectatorProbeWidget> GSpectatorProbeWidget;

	void ToggleSpectatorProbe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (GSpectatorProbeWidget.IsValid())
		{
			GSpectatorProbeWidget->RemoveFromParent();
			GSpectatorProbeWidget.Reset();
			Ar.Log(TEXT("vr.SpectatorProbe: off."));
			return;
		}

		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;

		if (!Controller)
		{
			Ar.Log(TEXT("vr.SpectatorProbe: no player controller. Run this while playing."));
			return;
		}

		UVRSpectatorProbeWidget* Widget =
			CreateWidget<UVRSpectatorProbeWidget>(Controller, UVRSpectatorProbeWidget::StaticClass());

		if (!Widget)
		{
			Ar.Log(TEXT("vr.SpectatorProbe: could not create the widget."));
			return;
		}

		Widget->AddToViewport(1000);
		GSpectatorProbeWidget = Widget;

		Ar.Log(TEXT("vr.SpectatorProbe: on. A magenta bar is at the top of the screen and a blue one ")
			   TEXT("at the bottom. Look at the monitor, then look inside the headset."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GSpectatorProbeCommand(
		TEXT("vr.SpectatorProbe"),
		TEXT("Toggles a screen-space UMG widget. Shows whether viewport UI reaches the headset or only the spectator screen."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ToggleSpectatorProbe));

	void ToggleSpectatorUI(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UVRSpectatorUISubsystem* Spectator = World ? World->GetSubsystem<UVRSpectatorUISubsystem>() : nullptr;

		if (!Spectator)
		{
			Ar.Log(TEXT("vr.SpectatorUI: no world. Run this while playing."));
			return;
		}

		if (Spectator->IsShowingSpectatorWidget())
		{
			Spectator->HideSpectatorWidget();
			Ar.Log(TEXT("vr.SpectatorUI: off."));
			return;
		}

		if (Spectator->ShowSpectatorWidget(UVRSpectatorProbeWidget::StaticClass()))
		{
			Ar.Log(TEXT("vr.SpectatorUI: on. The same banners, drawn to the spectator screen instead ")
				   TEXT("of the viewport. Expected: monitor yes, headset no."));
		}
		else
		{
			Ar.Log(TEXT("vr.SpectatorUI: could not start. Needs a headset - there is no second screen without one."));
		}
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GSpectatorUICommand(
		TEXT("vr.SpectatorUI"),
		TEXT("Toggles the same banners drawn onto the spectator screen rather than the viewport. Compare with vr.SpectatorProbe."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ToggleSpectatorUI));
}
