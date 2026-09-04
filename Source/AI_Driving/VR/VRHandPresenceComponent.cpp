// Copyright Epic Games, Inc. All Rights Reserved.

#include "VRHandPresenceComponent.h"
#include "AI_Driving.h"
#include "AI_DrivingPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Features/IModularFeatures.h"
#include "HeadMountedDisplayTypes.h"
#include "IHandTracker.h"

namespace
{
	/** Shows one of the two representations of a hand and hides the other */
	void ApplyHandVisibility(USceneComponent* Tracked, USceneComponent* Grip, EVRHandGrip State)
	{
		const bool bGripped = State == EVRHandGrip::Gripped;

		if (Tracked)
		{
			Tracked->SetVisibility(!bGripped, true);
		}

		if (Grip)
		{
			Grip->SetVisibility(bGripped, true);
		}
	}
}

IHandTracker* UVRHandPresenceComponent::FindHandTracker()
{
	const TArray<IHandTracker*> Trackers =
		IModularFeatures::Get().GetModularFeatureImplementations<IHandTracker>(IHandTracker::GetModularFeatureName());

	return Trackers.Num() > 0 ? Trackers[0] : nullptr;
}

UVRHandPresenceComponent::UVRHandPresenceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVRHandPresenceComponent::BeginPlay()
{
	Super::BeginPlay();

	const IHandTracker* Tracker = UVRHandPresenceComponent::FindHandTracker();
	bHandTrackingAvailable = Tracker && Tracker->IsHandTrackingStateValid();

	if (!bHandTrackingAvailable)
	{
		// not an error. A driver at a physical wheel has their hands on it nearly the whole time,
		// so the mesh hands on their own are already a complete experience
		UE_LOG(LogAI_Driving, Log, TEXT("No hand tracking available. The driver's hands will stay on the wheel."));
	}

	if (SteeringWheel)
	{
		WheelRestRotation = SteeringWheel->GetRelativeRotation();

		// the mesh hands only turn with the wheel if they hang off it
		if (LeftGripHand && !LeftGripHand->IsAttachedTo(SteeringWheel))
		{
			UE_LOG(LogAI_Driving, Warning, TEXT("LeftGripHand is not attached to the steering wheel, so it will not turn with it."));
		}

		if (RightGripHand && !RightGripHand->IsAttachedTo(SteeringWheel))
		{
			UE_LOG(LogAI_Driving, Warning, TEXT("RightGripHand is not attached to the steering wheel, so it will not turn with it."));
		}
	}
	else
	{
		UE_LOG(LogAI_Driving, Warning, TEXT("VRHandPresenceComponent has no steering wheel assigned. Both hands will stay gripped."));
	}

	// Meta's hand component writes its bone poses on its own tick, so ours has to come after it
	// or our transform would be the one that gets overwritten
	if (LeftTrackedHand)
	{
		AddTickPrerequisiteComponent(LeftTrackedHand);
	}

	if (RightTrackedHand)
	{
		AddTickPrerequisiteComponent(RightTrackedHand);
	}

	ApplyVisuals();
}

void UVRHandPresenceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DriveWheel();

	float LeftDistance = -1.0f;
	float RightDistance = -1.0f;

	const EVRHandGrip NewLeft = UpdateHandGrip(EControllerHand::Left, LeftGrip, LeftDistance);
	const EVRHandGrip NewRight = UpdateHandGrip(EControllerHand::Right, RightGrip, RightDistance);

	// a hand changing state is rare and worth a line: without it, a hand that never lets go is
	// indistinguishable from one whose detection never ran
	if (NewLeft != LeftGrip)
	{
		LeftGrip = NewLeft;
		UE_LOG(LogAI_Driving, Log, TEXT("Left hand %s the wheel (palm %.1fcm from the rim)."),
			LeftGrip == EVRHandGrip::Gripped ? TEXT("took hold of") : TEXT("let go of"), LeftDistance);
		OnGripChanged.Broadcast(EControllerHand::Left, LeftGrip);
	}

	if (NewRight != RightGrip)
	{
		RightGrip = NewRight;
		UE_LOG(LogAI_Driving, Log, TEXT("Right hand %s the wheel (palm %.1fcm from the rim)."),
			RightGrip == EVRHandGrip::Gripped ? TEXT("took hold of") : TEXT("let go of"), RightDistance);
		OnGripChanged.Broadcast(EControllerHand::Right, RightGrip);
	}

	ApplyVisuals();

	PlaceTrackedHand(EControllerHand::Left, LeftTrackedHand);
	PlaceTrackedHand(EControllerHand::Right, RightTrackedHand);

	if (bDrawDebug)
	{
		DrawDebugRim();

		DebugLogCountdown -= DeltaTime;

		if (DebugLogCountdown <= 0.0f)
		{
			DebugLogCountdown = 1.0f;
			UE_LOG(LogAI_Driving, Log, TEXT("[Hands] L %s %.1fcm | R %s %.1fcm | enter %.0f release %.0f"),
				LeftGrip == EVRHandGrip::Gripped ? TEXT("grip") : TEXT("free"), LeftDistance,
				RightGrip == EVRHandGrip::Gripped ? TEXT("grip") : TEXT("free"), RightDistance,
				GripEnterDistance, GripReleaseDistance);
		}
	}
}

void UVRHandPresenceComponent::SetWheelAndGripHands(USceneComponent* InWheel, USceneComponent* InLeftGrip, USceneComponent* InRightGrip)
{
	SteeringWheel = InWheel;
	LeftGripHand = InLeftGrip;
	RightGripHand = InRightGrip;
}

void UVRHandPresenceComponent::SetTrackedHands(USceneComponent* InLeft, USceneComponent* InRight)
{
	LeftTrackedHand = InLeft;
	RightTrackedHand = InRight;

	ApplyVisuals();
}

EVRHandGrip UVRHandPresenceComponent::GetGrip(EControllerHand Hand) const
{
	return Hand == EControllerHand::Left ? LeftGrip : RightGrip;
}

EVRHandGrip UVRHandPresenceComponent::UpdateHandGrip(EControllerHand Hand, EVRHandGrip Current, float& OutDistance)
{
	OutDistance = -1.0f;

	IHandTracker* Tracker = UVRHandPresenceComponent::FindHandTracker();

	if (!SteeringWheel || !Tracker || !Tracker->IsHandTrackingStateValid())
	{
		// nothing to measure against, so assume the hands are where a driver's hands normally are
		return EVRHandGrip::Gripped;
	}

	bool bIsTracked = false;

	if (!Tracker->GetAllKeypointStates(Hand, KeypointPositions, KeypointRotations, KeypointRadii, bIsTracked) || !bIsTracked)
	{
		// a hand closed around a rim is mostly hidden from the headset cameras, so losing tracking
		// mid grip is expected rather than a fault. Letting go here would make the hand vanish at
		// exactly the moment it is on the wheel, so hold the grip until tracking says otherwise
		return Current == EVRHandGrip::Gripped ? EVRHandGrip::Gripped : EVRHandGrip::Released;
	}

	const int32 PalmIndex = static_cast<int32>(EHandKeypoint::Palm);

	if (!KeypointPositions.IsValidIndex(PalmIndex))
	{
		return Current;
	}

	const float Distance = DistanceToRim(KeypointPositions[PalmIndex]);
	OutDistance = Distance;

	// the two thresholds differ so a hand hovering at the boundary does not flicker between states
	if (Current == EVRHandGrip::Gripped)
	{
		return Distance > GripReleaseDistance ? EVRHandGrip::Released : EVRHandGrip::Gripped;
	}

	return Distance < GripEnterDistance ? EVRHandGrip::Gripped : EVRHandGrip::Released;
}

float UVRHandPresenceComponent::DistanceToRim(const FVector& WorldPoint) const
{
	if (!SteeringWheel)
	{
		return TNumericLimits<float>::Max();
	}

	// scale is left out so the radius and the thresholds stay in real centimetres, which is what
	// gets measured off the physical wheel
	const FVector Local = SteeringWheel->GetComponentTransform().InverseTransformPositionNoScale(WorldPoint);

	// split the point into distance along the spin axis and distance out from the hub, so this
	// measures against the rim circle rather than against the hub
	float Axial = 0.0f;
	FVector2D Radial = FVector2D::ZeroVector;

	switch (WheelSpinAxis)
	{
	case EAxis::Y:
		Axial = Local.Y;
		Radial = FVector2D(Local.X, Local.Z);
		break;

	case EAxis::Z:
		Axial = Local.Z;
		Radial = FVector2D(Local.X, Local.Y);
		break;

	default:
		Axial = Local.X;
		Radial = FVector2D(Local.Y, Local.Z);
		break;
	}

	const float FromRim = Radial.Size() - WheelRadius;

	return FMath::Sqrt(FromRim * FromRim + Axial * Axial);
}

void UVRHandPresenceComponent::ApplyVisuals() const
{
	ApplyHandVisibility(LeftTrackedHand, LeftGripHand, LeftGrip);
	ApplyHandVisibility(RightTrackedHand, RightGripHand, RightGrip);
}

void UVRHandPresenceComponent::DriveWheel() const
{
	if (!bDriveWheelFromSteering || !SteeringWheel)
	{
		return;
	}

	const AAI_DrivingPawn* Pawn = Cast<AAI_DrivingPawn>(GetOwner());

	if (!Pawn)
	{
		return;
	}

	// the raw input rather than the rate limited one, so the wheel follows what the driver did
	const float Steering = Pawn->GetChaosVehicleMovement()->GetSteeringInput();
	const float Angle = Steering * WheelLockDegrees;

	FRotator Spin = FRotator::ZeroRotator;

	switch (WheelSpinAxis)
	{
	case EAxis::Y:	Spin.Pitch = Angle; break;
	case EAxis::Z:	Spin.Yaw = Angle; break;
	default:		Spin.Roll = Angle; break;
	}

	// composed onto the authored orientation rather than replacing it
	SteeringWheel->SetRelativeRotation(WheelRestRotation.Quaternion() * Spin.Quaternion());
}

void UVRHandPresenceComponent::PlaceTrackedHand(EControllerHand Hand, USceneComponent* Visual)
{
	if (!Visual || !Visual->IsVisible())
	{
		return;
	}

	IHandTracker* Tracker = UVRHandPresenceComponent::FindHandTracker();

	if (!Tracker || !Tracker->IsHandTrackingStateValid())
	{
		return;
	}

	FTransform Wrist;
	float Radius = 0.0f;

	if (!Tracker->GetKeypointState(Hand, EHandKeypoint::Wrist, Wrist, Radius))
	{
		return;
	}

	// keypoints already come back in world space
	Visual->SetWorldLocationAndRotation(Wrist.GetLocation(),
		Wrist.GetRotation() * TrackedHandRotationOffset.Quaternion());
}

void UVRHandPresenceComponent::DrawDebugRim() const
{
	if (!SteeringWheel)
	{
		return;
	}

	const FTransform WheelTransform = SteeringWheel->GetComponentTransform();
	const FVector Centre = WheelTransform.GetLocation();

	// the rim lies in the plane the spin axis is normal to, so the two in-plane axes are whichever
	// pair is left over
	FVector PlaneX = FVector::ZeroVector;
	FVector PlaneY = FVector::ZeroVector;

	switch (WheelSpinAxis)
	{
	case EAxis::Y:
		PlaneX = WheelTransform.GetUnitAxis(EAxis::X);
		PlaneY = WheelTransform.GetUnitAxis(EAxis::Z);
		break;

	case EAxis::Z:
		PlaneX = WheelTransform.GetUnitAxis(EAxis::X);
		PlaneY = WheelTransform.GetUnitAxis(EAxis::Y);
		break;

	default:
		PlaneX = WheelTransform.GetUnitAxis(EAxis::Y);
		PlaneY = WheelTransform.GetUnitAxis(EAxis::Z);
		break;
	}

	// the rim, then the distance a palm has to reach to take hold and the distance it has to
	// leave by to let go
	DrawDebugCircle(GetWorld(), Centre, WheelRadius, 48, FColor::White, false, -1.0f, 0, 0.3f, PlaneX, PlaneY, false);
	DrawDebugCircle(GetWorld(), Centre, WheelRadius + GripEnterDistance, 32, FColor::Green, false, -1.0f, 0, 0.15f, PlaneX, PlaneY, false);
	DrawDebugCircle(GetWorld(), Centre, WheelRadius + GripReleaseDistance, 32, FColor::Orange, false, -1.0f, 0, 0.15f, PlaneX, PlaneY, false);

	// and where each palm actually is, so a hand that is not triggering the grip can be explained
	IHandTracker* Tracker = UVRHandPresenceComponent::FindHandTracker();

	if (!Tracker || !Tracker->IsHandTrackingStateValid())
	{
		return;
	}

	const EControllerHand Hands[] = { EControllerHand::Left, EControllerHand::Right };
	const FColor Colours[] = { FColor::Cyan, FColor::Green };

	for (int32 Index = 0; Index < 2; ++Index)
	{
		TArray<FVector> Positions;
		TArray<FQuat> Rotations;
		TArray<float> Radii;
		bool bIsTracked = false;

		if (!Tracker->GetAllKeypointStates(Hands[Index], Positions, Rotations, Radii, bIsTracked) || !bIsTracked)
		{
			continue;
		}

		const int32 PalmIndex = static_cast<int32>(EHandKeypoint::Palm);

		if (Positions.IsValidIndex(PalmIndex))
		{
			DrawDebugSphere(GetWorld(), Positions[PalmIndex], 2.0f, 8, Colours[Index], false, -1.0f, 0, 0.2f);
			DrawDebugLine(GetWorld(), Positions[PalmIndex], Centre, Colours[Index], false, -1.0f, 0, 0.1f);
		}
	}
}
