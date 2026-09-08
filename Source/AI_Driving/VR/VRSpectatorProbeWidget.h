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

	/** Names the route that drew this, so a banner seen in the headset says which one put it there.
	 *  Two routes drawing the same banner cannot be told apart, which is how the first test failed
	 *  to answer anything. Set before the widget is built */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Spectator")
	FString RouteName = TEXT("GREEN  =  SPECTATOR TEXTURE  (render target)");

	/** Colour of both bars. One colour per route.
	 *
	 *  Defaults to the spectator route because that one is created by a subsystem which has no
	 *  business knowing about this widget; the viewport command sets red over the top of these. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Spectator")
	FLinearColor RouteColour = FLinearColor(0.f, 0.7f, 0.2f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
