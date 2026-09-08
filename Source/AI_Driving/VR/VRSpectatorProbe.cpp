// Copyright Epic Games, Inc. All Rights Reserved.

#include "VRSpectatorProbeWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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
}
