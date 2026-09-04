// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "VRHandPresenceComponent.generated.h"

class USceneComponent;
class IHandTracker;

UENUM(BlueprintType)
enum class EVRHandGrip : uint8
{
	/** The driver's hand is off the wheel, so the tracked hand is what they see */
	Released	UMETA(DisplayName = "Off the wheel"),

	/** The driver's hand is on the wheel, so the wheel-mounted hand mesh takes over */
	Gripped		UMETA(DisplayName = "On the wheel")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVRHandGripChanged, EControllerHand, Hand, EVRHandGrip, Grip);

/**
 *  Swaps each of the driver's hands between two representations: a hand tracked by the headset
 *  while it's off the wheel, and a mesh parented to the steering wheel while it's on.
 *
 *  The design point worth knowing about is what happens when hand tracking drops out. A fist
 *  wrapped around a rim hides most of the hand from the headset cameras, so losing tracking
 *  mid-grip is the normal case rather than a fault. This component therefore treats a tracking
 *  loss while gripping as "still gripping" and holds the mesh hand in place, instead of falling
 *  back to a tracked hand that would simply disappear. The same rule means the component works
 *  with no hand tracking at all: the hands just stay on the wheel, which for a driver sitting at
 *  a physical wheel is true nearly all of the time.
 *
 *  This only decides which representation is visible. Posing the tracked hand is left to whatever
 *  component draws it, and placing the mesh hands on the rim is left to the Blueprint - getting a
 *  hand to sit convincingly on a rim is an authoring job, not a formula. Turn on bDrawDebug to
 *  see the rim and the grip thresholds while positioning them.
 */
UCLASS(ClassGroup=(VR), meta=(BlueprintSpawnableComponent))
class UVRHandPresenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRHandPresenceComponent();

	// Begin ActorComponent interface

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// End ActorComponent interface

	/** Fires when either hand moves on or off the wheel */
	UPROPERTY(BlueprintAssignable, Category="VR Hands")
	FVRHandGripChanged OnGripChanged;

	/** Returns whether the given hand is currently on the wheel */
	UFUNCTION(BlueprintPure, Category="VR Hands")
	EVRHandGrip GetGrip(EControllerHand Hand) const;

	/** Returns false if the runtime never offered hand tracking, in which case both hands stay
	 *  on the wheel for the whole session */
	UFUNCTION(BlueprintPure, Category="VR Hands")
	bool IsHandTrackingAvailable() const { return bHandTrackingAvailable; }

	/** Returns the registered OpenXR hand tracker, or null if nothing provides the feature.
	 *  Shared so the diagnostic probe reads the same source this component acts on */
	static IHandTracker* FindHandTracker();

	/** Points the component at the wheel and the two mesh hands. Meant for an owner that builds
	 *  those in C++; a Blueprint can just fill the properties in instead */
	void SetWheelAndGripHands(USceneComponent* InWheel, USceneComponent* InLeftGrip, USceneComponent* InRightGrip);

	/** Assigns whatever draws the tracked hands. Usually done from a Blueprint, since posing a
	 *  tracked hand is the job of a component the Meta XR plugin provides */
	UFUNCTION(BlueprintCallable, Category="VR Hands")
	void SetTrackedHands(USceneComponent* InLeft, USceneComponent* InRight);

protected:

	/** Wheel the grip is measured against. The mesh hands should be parented under this so they
	 *  turn with it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands")
	TObjectPtr<USceneComponent> SteeringWheel;

	/** Rim radius in world centimetres, measured from the hub. Matched to SteeringMesh, whose
	 *  bounds put the outer edge at 20.9, and BP_SteeringBase grips it at 18 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands", meta = (Units = "cm"))
	float WheelRadius = 18.0f;

	/** Which of the wheel component's local axes it turns around. SteeringMesh is a disc lying in
	 *  its own XY plane, so it spins around Z */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands")
	TEnumAsByte<EAxis::Type> WheelSpinAxis = EAxis::Z;

	/** Hand shown while that hand is off the wheel. Typically whatever component poses a tracked hand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Visuals")
	TObjectPtr<USceneComponent> LeftTrackedHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Visuals")
	TObjectPtr<USceneComponent> RightTrackedHand;

	/** Hand shown while that hand is on the wheel. Parent these under SteeringWheel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Visuals")
	TObjectPtr<USceneComponent> LeftGripHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Visuals")
	TObjectPtr<USceneComponent> RightGripHand;

	/** A palm this close to the rim counts as taking hold of it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Grip", meta = (Units = "cm"))
	float GripEnterDistance = 10.0f;

	/** How far the palm has to get before it counts as letting go. Deliberately larger than the
	 *  enter distance, so a hand resting near the threshold doesn't flicker between the two hands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Grip", meta = (Units = "cm"))
	float GripReleaseDistance = 18.0f;

	/** Turns the wheel from the vehicle's steering input. Switch this off wherever a physical
	 *  wheel already drives the model */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Wheel")
	bool bDriveWheelFromSteering = true;

	/** Wheel rotation at full lock. A 900 degree wheel is 450 each way */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Wheel", meta = (Units = "deg", EditCondition = "bDriveWheelFromSteering"))
	float WheelLockDegrees = 450.0f;

	/** Correction applied to the tracked hands' orientation. The wrist joint's axes don't
	 *  necessarily line up with however the hand mesh was authored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Visuals")
	FRotator TrackedHandRotationOffset = FRotator::ZeroRotator;

	/** Draws the rim, both grip thresholds and the tracked palms. Leave this on while positioning
	 *  the hands, turn it off for the demo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR Hands|Debug")
	bool bDrawDebug = false;

private:

	/** Current state of each hand. Both start on the wheel, which is also where they stay if the
	 *  runtime turns out to have no hand tracking */
	EVRHandGrip LeftGrip = EVRHandGrip::Gripped;
	EVRHandGrip RightGrip = EVRHandGrip::Gripped;

	/** Whether the runtime offered hand tracking at all, checked once on BeginPlay */
	bool bHandTrackingAvailable = false;

	/** Seconds until the next debug line, so bDrawDebug doesn't spam the log every frame */
	float DebugLogCountdown = 0.0f;

	/** Reused between frames so polling the tracker doesn't reallocate every tick */
	TArray<FVector> KeypointPositions;
	TArray<FQuat> KeypointRotations;
	TArray<float> KeypointRadii;

	/** Wheel orientation as authored, so driving the wheel adds to it rather than replacing it */
	FRotator WheelRestRotation = FRotator::ZeroRotator;

	/** Works out where one hand should be this frame. Reports the measured distance so the
	 *  debug output can explain why a hand did or didn't take hold */
	EVRHandGrip UpdateHandGrip(EControllerHand Hand, EVRHandGrip Current, float& OutDistance);

	/** Distance from a world point to the nearest point on the rim */
	float DistanceToRim(const FVector& WorldPoint) const;

	/** Shows the representation matching each hand's state and hides the other */
	void ApplyVisuals() const;

	/** Turns the wheel model to match the steering input */
	void DriveWheel() const;

	/** Moves a tracked hand onto the real hand. Meta's hand component poses the fingers but
	 *  leaves the component where it is, so the wrist has to be driven from the tracker */
	void PlaceTrackedHand(EControllerHand Hand, USceneComponent* Visual);

	/** Draws the rim and the grip thresholds */
	void DrawDebugRim() const;
};
