// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VRSpectatorProbeWidget.generated.h"

/**
 *  A screen-space widget that says where it is, so you can find out where it went.
 *
 *  The engine hands the eye buffers to the headset compositor and only then draws Slate, into a
 *  swap chain it calls StereoSpectatorSwapChainTexture. A widget added to the viewport should
 *  therefore reach the monitor and never the headset - which is the split this demo wants, the
 *  driver seeing the world and the room seeing the readouts. Everything planned for the spectator
 *  screen rests on that, so it is worth seeing rather than believing.
 *
 *  Built in C++ rather than authored, because the point is to test the path, not the layout: it
 *  goes up through CreateWidget and AddToViewport exactly as AI_DrivingUI does.
 */
UCLASS()
class UVRSpectatorProbeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UVRSpectatorProbeWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
