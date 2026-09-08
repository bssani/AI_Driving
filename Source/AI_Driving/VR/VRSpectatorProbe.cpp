// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

/**
 *  Settles by eye where a screen-space widget actually ends up in VR.
 *
 *  The engine draws Slate after the eye buffers have already gone to the headset compositor, into
 *  a swap chain it names StereoSpectatorSwapChainTexture. So a widget added to the viewport should
 *  reach the monitor and never the headset, which is exactly the split a demo wants: the driver
 *  sees the world, the room sees the readouts. That is worth confirming rather than trusting,
 *  because everything built on top of it depends on it.
 *
 *  vr.SpectatorProbe, then look in both places. Nothing to author and nothing to undo.
 */
namespace
{
	TSharedPtr<SWidget> GSpectatorProbeWidget;

	TSharedRef<SWidget> MakeBanner(const FString& Text, const FLinearColor& Colour, EVerticalAlignment VAlign)
	{
		return SNew(SBorder)
			.BorderBackgroundColor(Colour)
			.HAlign(HAlign_Center)
			.VAlign(VAlign)
			.Padding(FMargin(24.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text))
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				// the question this answers is "can you read this in the headset", so it has to be
				// readable at a glance through a lens rather than squinted at on a monitor
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 34))
			];
	}

	void ToggleSpectatorProbe(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;

		if (!Viewport)
		{
			Ar.Log(TEXT("vr.SpectatorProbe: no game viewport. Run this while playing."));
			return;
		}

		if (GSpectatorProbeWidget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(GSpectatorProbeWidget.ToSharedRef());
			GSpectatorProbeWidget.Reset();
			Ar.Log(TEXT("vr.SpectatorProbe: off."));
			return;
		}

		GSpectatorProbeWidget =
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeBanner(TEXT("SCREEN-SPACE UMG PROBE  -  this belongs on the monitor only"),
					FLinearColor(0.9f, 0.f, 0.6f), VAlign_Top)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeBanner(TEXT("If you can read this inside the headset, screen-space UI is reaching the HMD"),
					FLinearColor(0.f, 0.4f, 0.9f), VAlign_Bottom)
			];

		Viewport->AddViewportWidgetContent(GSpectatorProbeWidget.ToSharedRef(), 1000);

		Ar.Log(TEXT("vr.SpectatorProbe: on. Two banners are now on the viewport. Look at the monitor, ")
			   TEXT("then look inside the headset. Expected: monitor yes, headset no."));
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GSpectatorProbeCommand(
		TEXT("vr.SpectatorProbe"),
		TEXT("Toggles two screen-space banners. Shows whether viewport UI reaches the headset or only the spectator screen."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ToggleSpectatorProbe));
}
