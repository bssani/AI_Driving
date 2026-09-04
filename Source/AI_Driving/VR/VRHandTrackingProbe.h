// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VRHandTrackingProbe.generated.h"

class UTextRenderComponent;
enum class EControllerHand : uint8;

/**
 *  Diagnostic actor that answers one question: do Quest hand joints actually reach Unreal
 *  over Quest Link? Meta's Unreal docs don't say, and the community reports disagree, so
 *  this measures it instead of guessing.
 *
 *  Spawn it from the console with "vr.HandProbe" while running in VR. It reports both routes
 *  into the runtime side by side:
 *
 *    - the engine route, OpenXR XR_EXT_hand_tracking via the OpenXRHandTracking plugin
 *    - the Meta route, OVRPlugin hand tracking via the Meta XR plugin
 *
 *  The readout distinguishes "extension missing" from "extension present but no joints ever
 *  arrived" from "joints arrived but the hands aren't visible right now", because those three
 *  have completely different causes.
 */
UCLASS()
class AVRHandTrackingProbe : public AActor
{
	GENERATED_BODY()

public:
	AVRHandTrackingProbe();

	// Begin Actor interface

	virtual void Tick(float DeltaTime) override;

	// End Actor interface

private:

	/** Status readout parked in front of the driver, so the result is readable without taking the headset off */
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Readout;

	/** Seconds until the next log line. The on-screen text updates every frame, the log doesn't need to */
	float LogCountdown = 0.0f;

	/** Keeps the readout in front of the player camera and facing it */
	void FollowCamera();

	/** Queries the engine's OpenXR hand tracker and draws its keypoints */
	FString PollEngineHandTracker();

	/** Queries the Meta XR plugin's hand tracking */
	FString PollMetaHandTracker();

	/** Draws a sphere at every reported joint, so a working hand is visible as 26 dots */
	void DrawKeypoints(EControllerHand Hand, const FColor& Color);
};
