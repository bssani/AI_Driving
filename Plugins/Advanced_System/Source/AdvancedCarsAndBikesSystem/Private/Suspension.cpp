// Copyright Cena Abachi - Youtube: Devlogerio - devloger.io@gmail.com - Publicated on 2025 - Last update 01/2026 - All Rights Reserved
#include "Suspension.h"
#include "Vehicle.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/World.h"

#include "Math/UnrealMathUtility.h"      // for FMath::Abs

#include "Components/AudioComponent.h"



// Sets default values for this component's properties
USuspension::USuspension()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;


}


void USuspension::BeginPlay()
{
	Super::BeginPlay();

	InitialLocalRotation = GetRelativeRotation();
	InitialLocalQuat = GetRelativeTransform().GetRotation(); // Stores fork orientation

	if (WheelMeshReference)
	{
		InitialWheelRotation = WheelMeshReference->GetRelativeRotation();
	}

	if (SkidSoundCue && Vehicle && Vehicle->VehicleMesh)
	{
		SkidAudio = UGameplayStatics::SpawnSoundAttached(
			SkidSoundCue,
			Vehicle->VehicleMesh,
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			true, // bStopWhenAttachedToDestroyed
			0.f,  // Start silent
			1.f   // Pitch
		);
	}


	if (!DriftCurve)
	{
		DriftCurve = NewObject<UCurveFloat>();
		DriftCurve->FloatCurve.AddKey(0.f, 0.25f);
		DriftCurve->FloatCurve.AddKey(1.f, 0.1f);
	}

	// ===============================
	// [1] Link to owning vehicle
	// ===============================
	Vehicle = Cast<AVehicle>(GetOwner());

	// ===============================
	// [2] Find the first StaticMeshComponent under this suspension
	//     and configure it as the visual wheel mesh
	// ===============================
	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);

	WheelMeshReference = nullptr;
	LeftSpringMeshReference = nullptr;
	RightSpringMeshReference = nullptr;

	for (USceneComponent* Child : Children)
	{
		UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Child);
		if (!Mesh) continue;

		// Apply common visual mesh settings
		Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetEnableGravity(false);
		Mesh->SetMobility(EComponentMobility::Movable);

		const FString MeshName = Mesh->GetName();

		// Classify Wheel
		if (!WheelMeshReference && MeshName.Contains(TEXT("Wheel")))
		{
			WheelMeshReference = Mesh;
		}
		else if (!LeftSpringMeshReference && MeshName.Contains(TEXT("LeftSpring")))
		{
			LeftSpringMeshReference = Mesh;
			LeftSpringLocalTransform = LeftSpringMeshReference->GetComponentTransform().GetRelativeTransform(Vehicle->GetActorTransform());
			LeftSpringInitialScale = LeftSpringMeshReference->GetComponentScale();
		}
		else if (!RightSpringMeshReference && MeshName.Contains(TEXT("RightSpring")))
		{
			RightSpringMeshReference = Mesh;
			RightSpringLocalTransform = RightSpringMeshReference->GetComponentTransform().GetRelativeTransform(Vehicle->GetActorTransform());
			RightSpringInitialScale = RightSpringMeshReference->GetComponentScale();
		}
		else if (!LeftBaseMeshReference && MeshName.Contains(TEXT("LeftBase")))
		{
			LeftBaseMeshReference = Mesh;
			LeftBaseLocalTransform = LeftBaseMeshReference->GetComponentTransform().GetRelativeTransform(Vehicle->GetActorTransform());
			LeftBaseInitialScale = LeftBaseMeshReference->GetComponentScale();
		}
		else if (!RightBaseMeshReference && MeshName.Contains(TEXT("RightBase")))
		{
			RightBaseMeshReference = Mesh;
			RightBaseLocalTransform = RightBaseMeshReference->GetComponentTransform().GetRelativeTransform(Vehicle->GetActorTransform());
			RightBaseInitialScale = RightBaseMeshReference->GetComponentScale();
		}
	}

}

void USuspension::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//if (!IsGameFocused())
	//{
	//	return; // Skip logic if alt-tabbed or not focused
	//}
	const float CurrentFPS = (DeltaTime > KINDA_SMALL_NUMBER) ? (1.f / DeltaTime) : 0.f;
	if (CurrentFPS < MinFunctionalFPS)
	{
		return;
	}


	// ===============================
	// [1] Safety check — ensure Vehicle and its mesh are valid
	// Prevents null pointer access during simulation
	// ===============================
	if (!bIsEnabled || !Vehicle || !Vehicle->VehicleMesh) return;

	// ===============================
	// [2] Update the wheel's cached forward speed (in km/h)
	// Used by multiple systems like grip, braking, and debug
	// ===============================
	UpdateCachedSpeed();

	// ===============================
	// [3] If this suspension supports steering, update its angle
	// Steering logic only applies to wheels flagged as steerable
	// ===============================
	if (bIsSteeringWheel)
	{
		UpdateSteering(DeltaTime);
	}

	// ===============================
	// [4] Run the suspension trace + spring simulation
	// Based on wheel type (capsule or sphere)
	// ===============================
	switch (SimulationMode)
	{
	case EWheelSimulationMode::SimpleLine:
		SimulateSuspensionSimple(DeltaTime);
		break;

	case EWheelSimulationMode::Sphere:
		SimulateSuspensionWithSphere(DeltaTime);
		if (bFallbackToLineTrace)
		{
			SimulateSuspensionSimple(DeltaTime); // fallback if sphere is invalid
		}
		break;

	case EWheelSimulationMode::SuperRealistic:
		SimulateSuspensionWithSphere(DeltaTime);
		if (bFallbackToLineTrace)
		{
			SimulateSuspensionSimple(DeltaTime); // Fallback if sphere hit was invalid
		}
		break;

	case EWheelSimulationMode::Motorcycle:
		SimulateSuspensionMotorcycle(DeltaTime);
		break;

	default:
		SimulateSuspensionSimple(DeltaTime);
		break;
	}

	// ===============================
	// [4.5] Moving platform follow (kinematic or simulating)
	// Makes raytrace suspension "stick" to moving ground
	// ===============================
	FollowMovingBase_Delta(DeltaTime);

	// ===============================
	// [5] Apply forward and side traction forces
	// Handles grip, damping, braking, and slope assistance
	// ===============================
	ApplyTraction(DeltaTime);
	UpdateDriftAndBrakingState(DeltaTime);
	UpdateWheelRotation(DeltaTime);
	//UpdateVisualWheel(VisualTargetLocation, VisualUpDirection, DeltaTime);
	SkidMark(DeltaTime);

	if (bShowDebugLines && WheelMeshReference)
	{
		const FVector WheelLocation = WheelMeshReference->GetComponentLocation();
		const FVector WheelRight = WheelMeshReference->GetRightVector();

		// Length of side lines — long and clear
		const float SideLineLength = 1000.f;

		FColor ThisColor = FColor::Purple;
		if (bIsSteeringWheel)
		{
			ThisColor = FColor::Green;
		}
		DrawDebugLine(
			GetWorld(),
			WheelLocation,
			WheelLocation + WheelRight * SideLineLength,
			ThisColor,
			false,
			0.f,
			0,
			1.f
		);

		DrawDebugLine(
			GetWorld(),
			WheelLocation,
			WheelLocation - WheelRight * SideLineLength,
			ThisColor,
			false,
			0.f,
			0,
			1.f
		);
	}
}


/*
void USuspension::FollowMovingBase_Delta(float DeltaTime)
{
	// =====================================================
	// WORKING moving-platform follow
	// NO new Vehicle members
	// Uses ONLY suspension trace + VehicleMesh
	// =====================================================

	if (!Vehicle || !Vehicle->VehicleMesh || !GetWorld())
		return;

	// ---------------------------------------------
	// Static per-vehicle state (safe, minimal)
	// ---------------------------------------------
	static TMap<AVehicle*, UPrimitiveComponent*> PrevBaseComp;
	static TMap<AVehicle*, FTransform> PrevBaseTransform;
	static TMap<AVehicle*, FTransform> RelativeToBase;
	static TMap<AVehicle*, float> UngroundedTime;

	// ---------------------------------------------
	// Grounding check (already computed by suspension)
	// ---------------------------------------------
	if (!bWheelTouchingGround || !CachedHitComponent)
	{
		float& T = UngroundedTime.FindOrAdd(Vehicle);
		T += DeltaTime;

		if (T >= 0.15f)
		{
			PrevBaseComp.Remove(Vehicle);
			PrevBaseTransform.Remove(Vehicle);
			RelativeToBase.Remove(Vehicle);
		}
		return;
	}

	UngroundedTime.FindOrAdd(Vehicle) = 0.f;

	// ---------------------------------------------
	// First contact or base change
	// ---------------------------------------------
	if (!PrevBaseComp.Contains(Vehicle) || PrevBaseComp[Vehicle] != CachedHitComponent)
	{
		const FTransform BaseNow = CachedHitComponent->GetComponentTransform();

		PrevBaseComp.Add(Vehicle, CachedHitComponent);
		PrevBaseTransform.Add(Vehicle, BaseNow);

		// store vehicle relative-to-base ONCE
		RelativeToBase.Add(
			Vehicle,
			Vehicle->VehicleMesh->GetComponentTransform().GetRelativeTransform(BaseNow)
		);
		return;
	}

	// ---------------------------------------------
	// Compute base delta (NothingPlane method)
	// ---------------------------------------------
	const FTransform BasePrev = PrevBaseTransform[Vehicle];
	const FTransform BaseNow = CachedHitComponent->GetComponentTransform();
	const FTransform Rel = RelativeToBase[Vehicle];

	const FVector PrevWorldPos =
		BasePrev.TransformPosition(Rel.GetLocation());
	const FQuat PrevWorldRot =
		BasePrev.GetRotation() * Rel.GetRotation();

	const FVector NewWorldPos =
		BaseNow.TransformPosition(Rel.GetLocation());
	const FQuat NewWorldRot =
		BaseNow.GetRotation() * Rel.GetRotation();

	const FVector PosDelta = NewWorldPos - PrevWorldPos;
	const FQuat   RotDelta = NewWorldRot * PrevWorldRot.Inverse();

	// ---------------------------------------------
	// Apply to PHYSICS ROOT ONLY
	// ---------------------------------------------
	const FTransform RootXf = Vehicle->VehicleMesh->GetComponentTransform();

	Vehicle->VehicleMesh->SetWorldLocationAndRotation(
		RootXf.GetLocation() + PosDelta,
		(RotDelta * RootXf.GetRotation()).Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	// ---------------------------------------------
	// History update ONLY
	// ---------------------------------------------
	PrevBaseTransform[Vehicle] = BaseNow;
}
*/

void USuspension::FollowMovingBase_Delta(float DeltaTime)
{
	// =====================================================
	// PERFECT moving-platform follow
	// Works while driving, braking, or idle
	// No drift, no locking, no accumulation
	// =====================================================

	if (!Vehicle || !Vehicle->VehicleMesh || !GetWorld())
		return;

	// ---------------------------------------------
	// Static per-vehicle history
	// ---------------------------------------------
	static TMap<AVehicle*, UPrimitiveComponent*> PrevBaseComp;
	static TMap<AVehicle*, FTransform> PrevBaseTransform;
	static TMap<AVehicle*, float> UngroundedTime;

	// ---------------------------------------------
	// Grounding check
	// ---------------------------------------------
	if (!bWheelTouchingGround || !IsValid(CachedHitComponent) || !CachedHitComponent)
	{
		float& T = UngroundedTime.FindOrAdd(Vehicle);
		T += DeltaTime;

		if (T > 0.15f)
		{
			PrevBaseComp.Remove(Vehicle);
			PrevBaseTransform.Remove(Vehicle);
		}
		return;
	}

	// ---------------------------------------------
	// Only follow MOVING tagged base
	// Tag can be "Moving" or Vehicle-defined override
	// ---------------------------------------------
	const FName DefaultMovingTag(TEXT("Moving"));
	const FName VehicleMovingTag =
		(Vehicle && !Vehicle->MovingPlatformTag.IsNone())
		? Vehicle->MovingPlatformTag
		: DefaultMovingTag;

	const bool bComponentMoving =
		CachedHitComponent->ComponentHasTag(DefaultMovingTag) ||
		CachedHitComponent->ComponentHasTag(VehicleMovingTag);

	const bool bActorMoving =
		(CachedHitComponent->GetOwner() &&
			(
				CachedHitComponent->GetOwner()->ActorHasTag(DefaultMovingTag) ||
				CachedHitComponent->GetOwner()->ActorHasTag(VehicleMovingTag)
				));

	if (!bComponentMoving && !bActorMoving)
	{
		PrevBaseComp.Remove(Vehicle);
		PrevBaseTransform.Remove(Vehicle);
		UngroundedTime.Remove(Vehicle);
		return;
	}


	UngroundedTime.FindOrAdd(Vehicle) = 0.f;

	// ---------------------------------------------
	// First contact or base changed
	// ---------------------------------------------
	if (!PrevBaseComp.Contains(Vehicle) || PrevBaseComp[Vehicle] != CachedHitComponent)
	{
		PrevBaseComp.Add(Vehicle, CachedHitComponent);
		PrevBaseTransform.Add(Vehicle, CachedHitComponent->GetComponentTransform());
		return;
	}

	// ---------------------------------------------
	// Base delta ONLY (this is the key)
	// ---------------------------------------------
	const FTransform BasePrev = PrevBaseTransform[Vehicle];
	const FTransform BaseNow = CachedHitComponent->GetComponentTransform();

	const FVector BasePosDelta =
		BaseNow.GetLocation() - BasePrev.GetLocation();

	const FQuat BaseRotDelta =
		BaseNow.GetRotation() * BasePrev.GetRotation().Inverse();

	// ---------------------------------------------
	// Apply delta to current vehicle transform
	// (DO NOT use stored relative offset)
	// ---------------------------------------------
	const FTransform VehXf = Vehicle->VehicleMesh->GetComponentTransform();

	const FVector NewLoc =
		BaseRotDelta.RotateVector(VehXf.GetLocation() - BasePrev.GetLocation())
		+ BasePrev.GetLocation()
		+ BasePosDelta;

	const FQuat NewRot =
		BaseRotDelta * VehXf.GetRotation();

	Vehicle->VehicleMesh->SetWorldLocationAndRotation(
		NewLoc,
		NewRot,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	// ---------------------------------------------
	// Update history
	// ---------------------------------------------
	PrevBaseTransform[Vehicle] = BaseNow;
}


void USuspension::ApplyAcceleration(const FVector& ContactPoint, float DeltaTime)
{
	if (!Vehicle || !Vehicle->VehicleMesh || !bWheelTouchingGround)
		return;

	const float ForwardSpeedCm = Vehicle->CachedSpeedKmh / 0.036f;
	FVector ForceVector = FVector::ZeroVector;




	// === 2. BRAKING IF REVERSING WHILE MOVING FORWARD ===
	const FVector Velocity = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(ContactPoint);
	const FVector Forward = GetForwardVector();
	const float SpeedDirDot = FVector::DotProduct(Forward, Velocity);
	const bool bShouldBrakeInsteadOfReverse = (Vehicle->CachedGearIndex == 0 && SpeedDirDot > 0.f);

	// === 3. NORMAL BRAKING ===
	if (Vehicle->bIsBraking && bCanBrake)
	{
		const float Speed = Velocity.Size(); // cm/s
		if (FMath::Abs(Speed) > KINDA_SMALL_NUMBER)
		{
			const float BrakeInput = FMath::Clamp(Vehicle->BrakePaddle, 0.f, 1.f);
			const FVector BrakeDir = -Velocity.GetSafeNormal(); // Opposite of motion

			// === Speed-based brake strength blend ===
			const float MaxSpeed = FMath::Max(1.f, Vehicle->DesiredMaxSpeedForFullThrottleKmh); // Avoid divide-by-zero
			const float SpeedRatio = FMath::Clamp((Speed * 0.036f) / MaxSpeed, 0.f, 1.f); // cm/s to km/h

			const float DynamicBrakingForce = FMath::Lerp(BrakingForce, BrakingForceOnMaxSpeed, SpeedRatio);

			const float BrakeForceMag = Speed * DynamicBrakingForce * BrakeInput;
			const FVector BrakeForce = BrakeDir * BrakeForceMag;

			Vehicle->VehicleMesh->AddForceAtLocation(BrakeForce, ContactPoint);

			if (bShowDebugLines)
			{
				DrawDebugLine(GetWorld(), ContactPoint + FVector(0, 0, 10.f),
					ContactPoint + FVector(0, 0, 10.f) + BrakeForce.GetSafeNormal() * 60.f,
					FColor::Cyan, false, -1, 0, 1.5f);

				DrawDebugString(GetWorld(), ContactPoint + FVector(0, 0, 30),
					TEXT("BRAKING"), nullptr, FColor::Cyan, 0.f, true);
			}
		}
		return;
	}
	else if (Vehicle->bIsBraking && !bCanBrake)
	{
		return;
	}


	if (bUseTankSteering && bIsDriveWheel && FMath::Abs(Vehicle->SteeringInput) > KINDA_SMALL_NUMBER)
	{
		// === Get steering input and clamp ===
		const float TankSteerInput = FMath::Clamp(Vehicle->SteeringInput, -1.f, 1.f);

		// === Use Gear 1 push force ===
		float TankBaseForce = 100000.f; // Default fallback
		if (Vehicle->GearPushForces.IsValidIndex(1))
		{
			TankBaseForce = Vehicle->GearPushForces[1];
		}

		// === Determine side multiplier ===
		const float TankSideMultiplier = bIsRightSideWheel ? -1.f : 1.f;


		// === Final force applied by this wheel ===
		const float TankFinalForce = TankSteerInput * TankSideMultiplier * TankBaseForce * TankSteeringForceMultiplier;

		const FVector TankForceVector = GetForwardVector() * TankFinalForce;

		// === Apply force to mesh ===
		Vehicle->VehicleMesh->AddForceAtLocation(TankForceVector, ContactPoint);

		// === Debug visuals ===
		if (bShowDebugLines)
		{
			DrawDebugLine(GetWorld(), ContactPoint + FVector(0, 0, 10.f),
				ContactPoint + FVector(0, 0, 10.f) + TankForceVector.GetSafeNormal() * 80.f,
				FColor::Red, false, -1, 0, 2.0f);

			DrawDebugString(GetWorld(), ContactPoint + FVector(0, 0, 30),
				FString::Printf(TEXT("TankForce: %.0f"), TankFinalForce),
				nullptr, FColor::Red, 0.f, true);
		}

		// === Fake Wheel Spin ===
		if (WheelMeshReference)
		{
			const float TankAngularVelocityRad = TankFinalForce / FMath::Max(VisualWheelRadius, 1.f);
			const float TankAngularDegrees = TankAngularVelocityRad * DeltaTime * (180.f / PI);
			WheelMeshReference->AddLocalRotation(FRotator(TankAngularDegrees, 0.f, 0.f));
			CachedWheelRPM = TankAngularVelocityRad * (60.f / (2.f * PI));
		}

		return;
	}



	// === Block Acceleration when Handbrake is active and no throttle, and speed is low ===
	//if (Vehicle->bIsHandBrake && Vehicle->CachedSpeedKmh < Vehicle->DesiredMaxSpeedForFullThrottleKmh)
	if (Vehicle->bIsHandBrake && bSupportsHandbrake && Vehicle->CachedSpeedKmh < Vehicle->DesiredMaxSpeedForFullThrottleKmh)

	{
		if (bShowDebugLines)
		{
			DrawDebugString(GetWorld(), ContactPoint + FVector(0, 0, 30), TEXT("ACCELERATION BLOCKED: Handbrake Hold"), nullptr, FColor::Orange, 0.f, true);
		}
		return;
	}


	// === 4. ACCELERATION ===
	if (bIsDriveWheel && FMath::Abs(Vehicle->CachedThrottleForce) > KINDA_SMALL_NUMBER)
	{
		// === Burnout Block ===
		if (Vehicle->bReadyForBurnout)
			return;

		float AccelCMPerSec2 = Vehicle->CachedThrottleForce;

		// === SPEED LIMIT ===
		const float SpeedLimitKmh = Vehicle->bIsReversing
			? (Vehicle->GearSpeedThresholdsKmh.IsValidIndex(0) ? Vehicle->GearSpeedThresholdsKmh[0] : 40.f)
			: (Vehicle->GearSpeedThresholdsKmh.IsValidIndex(Vehicle->CachedGearIndex)
				? Vehicle->GearSpeedThresholdsKmh[Vehicle->CachedGearIndex]
				: Vehicle->DesiredMaxSpeedForFullThrottleKmh);

		// === SPEED CURVE ===
		float CurveScale = 1.0f;
		if (SpeedBasedAccelCurve)
		{
			const float MaxVehicleSpeedKmh = FMath::Max(Vehicle->DesiredMaxSpeedForFullThrottleKmh, 1.f); // Avoid div by 0
			const float NormalizedGlobalSpeed = FMath::Clamp(FMath::Abs(Vehicle->CachedSpeedKmh) / MaxVehicleSpeedKmh, 0.f, 1.f);
			CurveScale = SpeedBasedAccelCurve->GetFloatValue(NormalizedGlobalSpeed);
		}



		// === SLOPE MULTIPLIER ===
		float SlopeMultiplier = 1.0f;
		if (bIsOnSlope && !Vehicle->bIsInAir && !bUseTankSteering)
		{
			const float Tilt = FMath::Abs(Vehicle->CachedGyroRotation.Pitch);
			const float MaxTilt = Vehicle->MaxHillGripAngle;
			const float TiltAlpha = FMath::Clamp(Tilt / MaxTilt, 0.f, 1.f);

			float BaseSlopeMultiplier = FMath::Lerp(1.0f, Vehicle->OnSlopeEnginePowerMultiplier, TiltAlpha);
			const float SpeedKmh = Vehicle->CachedSpeedKmh;
			const float MaxResistanceSpeed = 60.f;
			const float SpeedAlpha = FMath::Clamp(SpeedKmh / MaxResistanceSpeed, 0.f, 1.f);

			SlopeMultiplier = FMath::Lerp(BaseSlopeMultiplier, BaseSlopeMultiplier * 0.5f, SpeedAlpha);
		}

		// === FINAL FORCE ===
		const float Direction = (Vehicle->CachedGearIndex == 0) ? -1.f : 1.f;
		const float InputForce = AccelCMPerSec2 * CurveScale * SlopeMultiplier;
		const FVector AccelDirection = Vehicle->bIsMotorcycle
			? Vehicle->GetActorForwardVector()
			: GetForwardVector();

		ForceVector = AccelDirection * InputForce * Direction;


		const float SignedSpeed = FVector::DotProduct(Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(ContactPoint), GetForwardVector());
		// Skip speed cap for testing — always apply acceleration
		Vehicle->VehicleMesh->AddForceAtLocation(ForceVector, ContactPoint);



		DebugCurrentGear();
	}

	// === 5. DEBUG ===
	CachedThrottleForce = ForceVector.Size();

	if (bShowDebugLines && !ForceVector.IsNearlyZero())
	{
		DebugAccelerationForce(ContactPoint, ForceVector);
	}
}


void USuspension::SimulateSuspensionSimple(float DeltaTime)
{
	// ============================================
	// [1] Trace Setup — Line Trace Down from Suspension Start
	// ============================================
	const FVector UpDir = GetUpVector();
	float EffectiveStretch = SuspensionStretchMultiplier;

	if (ExtraStretchFadeMaxAngle > 0.f)
	{
		const float AngleToWorldUp = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(UpDir, FVector::UpVector)));
		const float AngleFadeRatio = FMath::Clamp(AngleToWorldUp / ExtraStretchFadeMaxAngle, 0.f, 1.f);
		const float StretchFadeFactor = FMath::Cos(FMath::DegreesToRadians(AngleFadeRatio * 90.f));
		EffectiveStretch = SuspensionStretchMultiplier * StretchFadeFactor;
	}

	const float ExtendedTraceLength = SuspensionHeight * (1.0f + EffectiveStretch);
	const FVector Start = GetComponentLocation();
	const FVector End = Start - UpDir * ExtendedTraceLength;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Vehicle);

	if (bShowDebugLines)
		DrawDebugLine(GetWorld(), Start - (GetForwardVector() * 5), End - (GetForwardVector() * 5), FColor::Cyan, false, -1, 0, 2.0f);

	// ============================================
	// [2] Suspension Hit Detected — Handle Ground Contact
	// ============================================
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const FVector HitOffset = Start - Hit.ImpactPoint;
		const float VerticalHitDistance = FVector::DotProduct(HitOffset, UpDir);

		const float MinAcceptableCompression = SuspensionHeight * 0.1f; // prevent 90%+ compression on side contact

		if (VerticalHitDistance < MinAcceptableCompression)
		{
			// Reject weird upward or side collisions
			bWheelTouchingGround = false;
			CachedHitActor = nullptr;
			CachedHitComponent = nullptr;
			bIsOnSlope = false;
			return;
		}

		bWheelTouchingGround = true;
		CachedGroundNormal = Hit.ImpactNormal;
		CachedHitActor = Hit.GetActor();
		CachedHitComponent = Hit.GetComponent();


		const float SlopeAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Hit.Normal, FVector::UpVector)));
		bIsOnSlope = SlopeAngleDeg > SlopeEnterAngle;

		const FVector Contact = Hit.Location;
		CachedSuspensionContactPoint = Contact;
		UpdateVisualWheel(Contact, UpDir, DeltaTime);

		// --- Spring Physics (Framerate-independent) ---
		const float SpringLen = (Start - Hit.Location).Size() - VisualWheelRadius;
		const float DesiredSpringLen = SuspensionHeight * FMath::Clamp(SuspensionRestRatio, 0.0f, 1.0f);
		const float Offset = DesiredSpringLen - SpringLen;
		SpringMotion(Offset);

		const FVector VelocityAtPoint = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(Start);
		const float VerticalVelocity = FVector::DotProduct(VelocityAtPoint, UpDir);

		// Calculate how "valid" the surface is for applying suspension force
		const float SurfaceAlignment = FVector::DotProduct(Hit.Normal, UpDir); // 1 = flat ground, 0 = 90°, < 0 = upside-down
		const float ClampedAlignment = FMath::Clamp(SurfaceAlignment, 0.0f, 1.0f); // Prevents sticking to walls/ceilings

		// Modulate spring force by alignment
		float SpringForceMag = (SuspensionPower * Offset - VerticalVelocity * SuspensionSmoothness) * ClampedAlignment;
		//SpringForceMag *= GetSuspensionForceMultiplier();

		// Apply it only if there's some alignment
		if (SpringForceMag > KINDA_SMALL_NUMBER)
		{
			const FVector SpringForce = UpDir * SpringForceMag;
			Vehicle->VehicleMesh->AddForceAtLocation(SpringForce, Contact);

			// --- Apply Drive Force ---
			ApplyAcceleration(Contact, DeltaTime);

			if (bShowDebugLines)
				DebugSuspensionForce(Contact, SpringForce, SpringForceMag, VerticalVelocity);
		}

	}
	else
	{
		// ============================================
		// [3] No Ground Contact
		// ============================================
		bWheelTouchingGround = false;
		CachedHitActor = nullptr;
		CachedHitComponent = nullptr;
		bIsOnSlope = false;

		if (WheelMeshReference)
		{
			const FVector FreeFallPos = End + UpDir * VisualWheelRadius;
			WheelMeshReference->SetWorldLocation(FreeFallPos);
		}


		// === Spring Visual (Match Wheel Stretch) ===
		const float SpringLen = (Start - End).Size() - VisualWheelRadius;
		const float DesiredSpringLen = SuspensionHeight * FMath::Clamp(SuspensionRestRatio, 0.0f, 1.0f);
		const float Offset = DesiredSpringLen - SpringLen;

		SpringMotion(Offset); // Use the same stretch logic as ground contact
	}
}

void USuspension::SimulateSuspensionMotorcycle(float DeltaTime)
{
	if (!Vehicle) return;

	const FVector UpDir = GetUpVector();
	const FVector ChassisUp = Vehicle->GetActorUpVector(); // Used instead of WorldUp

	// === Dynamic Stretch ===
	float EffectiveStretch = SuspensionStretchMultiplier;
	if (ExtraStretchFadeMaxAngle > 0.f)
	{
		const float AngleToChassisUp = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(UpDir, ChassisUp)));
		const float AngleFadeRatio = FMath::Clamp(AngleToChassisUp / ExtraStretchFadeMaxAngle, 0.f, 1.f);
		const float StretchFadeFactor = FMath::Cos(FMath::DegreesToRadians(AngleFadeRatio * 90.f));
		EffectiveStretch = SuspensionStretchMultiplier * StretchFadeFactor;
	}

	const float ExtendedTraceLength = SuspensionHeight * (1.0f + EffectiveStretch);
	const FVector Start = GetComponentLocation();
	const FVector End = Start - UpDir * ExtendedTraceLength;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Vehicle);

	if (bShowDebugLines)
		DrawDebugLine(GetWorld(), Start, End, FColor::Cyan, false, -1, 0, 2.0f);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const FVector SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
		const float VerticalHitDistance = FVector::DotProduct(Start - Hit.ImpactPoint, UpDir);

		const float UpDot = FVector::DotProduct(SurfaceNormal, UpDir);
		const float RightDot = FMath::Abs(FVector::DotProduct(SurfaceNormal, GetRightVector()));

		const float MinUpAlignmentDot = FMath::Cos(FMath::DegreesToRadians(SuperRealisticMaxGroundSlopeAngleDeg));
		const float MaxAllowedSideImpactDot = FMath::Cos(FMath::DegreesToRadians(SuperRealisticMaxSideImpactAngleDeg));

		if (SimulationMode == EWheelSimulationMode::SuperRealistic &&
			(UpDot < MinUpAlignmentDot || RightDot > MaxAllowedSideImpactDot || VerticalHitDistance < SuspensionHeight * 0.1f))
		{
			bWheelTouchingGround = false;
			CachedHitActor = nullptr;
			CachedHitComponent = nullptr;
			bIsOnSlope = false;
			return;
		}

		if (VerticalHitDistance < SuspensionHeight * 0.1f)
		{
			bWheelTouchingGround = false;
			CachedHitActor = nullptr;
			CachedHitComponent = nullptr;
			bIsOnSlope = false;
			return;
		}

		bWheelTouchingGround = true;
		CachedGroundNormal = SurfaceNormal;
		CachedHitActor = Hit.GetActor();
		CachedHitComponent = Hit.GetComponent();

		const float SlopeAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(SurfaceNormal, UpDir)));

		bIsOnSlope = SlopeAngleDeg > SlopeEnterAngle;

		const FVector Contact = Hit.Location;
		CachedSuspensionContactPoint = Contact;
		UpdateVisualWheel(Contact, UpDir, DeltaTime);

		const float SpringLen = VerticalHitDistance - VisualWheelRadius;
		const float DesiredSpringLen = SuspensionHeight * FMath::Clamp(SuspensionRestRatio, 0.f, 1.f);
		const float Offset = DesiredSpringLen - SpringLen;

		const FVector VelocityAtPoint = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(Start);
		float VerticalVelocity = FVector::DotProduct(VelocityAtPoint, UpDir);
		if (Vehicle->bIsMotorcycle)
			VerticalVelocity = FVector::DotProduct(VelocityAtPoint, ChassisUp); // ? Replaced

		const float SpeedKmh = FMath::Abs(Vehicle->CachedSpeedKmh);
		const bool bThrottleActive = FMath::Abs(Vehicle->GasPaddle) > 0.05f;

		// === Slope Lock Conditions ===
		const FVector VelocityAtWheel = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(Start);
		const float VerticalVelocity1 = FVector::DotProduct(VelocityAtWheel, ChassisUp); // ? Replaced
		const bool bVehicleMoving = SpeedKmh > 10.f || bThrottleActive || FMath::Abs(VerticalVelocity1) > 5.f;
		const bool bSlopeLockActive = bIsOnSlope && !bVehicleMoving;

		if (bSlopeLockActive)
		{
			if (bShowDebugLines)
				DrawDebugString(GetWorld(), CachedSuspensionContactPoint, TEXT("Slope Lock - Soft Hold"), nullptr, FColor::Orange, 0.f, true);

			// === Gently Damp Horizontal Motion ===
			const FVector CurrentVel = Vehicle->VehicleMesh->GetPhysicsLinearVelocity();
			const FVector HorizontalVel = FVector::VectorPlaneProject(CurrentVel, ChassisUp); // ? Replaced
			const FVector NewVel = CurrentVel - HorizontalVel * 0.5f;
			Vehicle->VehicleMesh->SetPhysicsLinearVelocity(NewVel);

			// === Gently Damp Angular Velocity ===
			const FVector AngularVel = Vehicle->VehicleMesh->GetPhysicsAngularVelocityInDegrees();
			Vehicle->VehicleMesh->SetPhysicsAngularVelocityInDegrees(AngularVel * 0.6f);

			// === Apply Soft Upward Hold Force ===
			const float HoldForceMag = Vehicle->VehicleMesh->GetMass() * 450.f;
			Vehicle->VehicleMesh->AddForceAtLocation(ChassisUp * HoldForceMag, CachedSuspensionContactPoint); // ? Replaced

			SpringMotion(0.f);
			return;
		}

		// === Aligned Spring Force ===
		const FVector ForceDirection = FMath::Lerp(ChassisUp, SurfaceNormal, 1.f - FMath::Clamp(UpDot, 0.f, 1.f)).GetSafeNormal();
		float SpringForceMag = (SuspensionPower * Offset - VerticalVelocity * SuspensionSmoothness) * FMath::Clamp(UpDot, 0.f, 1.f);

		SpringMotion(Offset);

		if (SpringForceMag > KINDA_SMALL_NUMBER)
		{
			const FVector SpringForce = ForceDirection * SpringForceMag;
			Vehicle->VehicleMesh->AddForceAtLocation(SpringForce, Contact);
			ApplyAcceleration(Contact, DeltaTime);

			if (bShowDebugLines)
				DebugSuspensionForce(Contact, SpringForce, SpringForceMag, VerticalVelocity);
		}
	}
	else
	{
		bWheelTouchingGround = false;
		CachedHitActor = nullptr;
		CachedHitComponent = nullptr;
		bIsOnSlope = false;

		if (WheelMeshReference)
		{
			const FVector FreeFallPos = End + UpDir * VisualWheelRadius;
			WheelMeshReference->SetWorldLocation(FreeFallPos);
		}

		const float SpringLen = (Start - End).Size() - VisualWheelRadius;
		const float DesiredSpringLen = SuspensionHeight * FMath::Clamp(SuspensionRestRatio, 0.0f, 1.0f);
		const float Offset = DesiredSpringLen - SpringLen;

		SpringMotion(Offset);
	}
}



void USuspension::SimulateSuspensionWithSphere(float DeltaTime)
{
	const FVector UpDir = GetUpVector();
	const FVector ForwardDir = GetForwardVector();
	const FVector RightDir = GetRightVector();
	const float Radius = VisualWheelRadius;

	// === Dynamic Stretch Fade Based on Suspension Angle ===
	float EffectiveStretch = SuspensionStretchMultiplier;
	if (ExtraStretchFadeMaxAngle > 0.f)
	{
		const float AngleToUp = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(UpDir, FVector::UpVector)));
		const float AngleFadeRatio = FMath::Clamp(AngleToUp / ExtraStretchFadeMaxAngle, 0.f, 1.f);
		const float StretchFadeFactor = FMath::Cos(FMath::DegreesToRadians(AngleFadeRatio * 90.f));
		EffectiveStretch = SuspensionStretchMultiplier * StretchFadeFactor;
	}

	const FVector Start = GetComponentLocation() + UpDir * Radius;
	const float TraceLength = SuspensionHeight * (1.0f + EffectiveStretch);
	const FVector End = Start - UpDir * TraceLength;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Vehicle);

	bool bHit = false;
	bHit = GetWorld()->SweepSingleByChannel(
		Hit, Start, End,
		WheelMeshReference ? WheelMeshReference->GetComponentQuat() : FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(Radius),
		Params
	);

	FVector WheelPosition = End;

	if (bHit)
	{
		const FVector HitOffset = Start - Hit.ImpactPoint;
		const float VerticalHitDistance = FVector::DotProduct(HitOffset, UpDir);
		const float MinValidCompression = SuspensionHeight * 0.1f;

		// === Directional Filtering ===
		const FVector SurfaceNormal = Hit.Normal.GetSafeNormal();
		const float UpDot = FVector::DotProduct(SurfaceNormal, UpDir);     // Good if near 1
		const float RightDot = FMath::Abs(FVector::DotProduct(SurfaceNormal, RightDir)); // Bad if high (side wall)

		const float MinUpAlignmentDot = FMath::Cos(FMath::DegreesToRadians(SuperRealisticMaxGroundSlopeAngleDeg));
		const float MaxAllowedSideImpactDot = FMath::Cos(FMath::DegreesToRadians(SuperRealisticMaxSideImpactAngleDeg));

		if (SimulationMode == EWheelSimulationMode::SuperRealistic && (UpDot < MinUpAlignmentDot || RightDot > MaxAllowedSideImpactDot || VerticalHitDistance < MinValidCompression))
		{
			bFallbackToLineTrace = true;
			bWheelTouchingGround = false;
			CachedHitActor = nullptr;
			CachedHitComponent = nullptr;
			bIsOnSlope = false;
			return;
		}
		else
		{
			bFallbackToLineTrace = false;
		}

		// === Valid Ground Hit ===
		bWheelTouchingGround = true;
		CachedHitActor = Hit.GetActor();
		CachedHitComponent = Hit.GetComponent();

		// Slope Check
		const float SlopeAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(SurfaceNormal, FVector::UpVector)));
		bIsOnSlope = SlopeAngleDeg > SlopeEnterAngle;

		// Visual
		const FVector Contact = Hit.Location - UpDir * Radius;
		CachedSuspensionContactPoint = Contact;
		UpdateVisualWheel(Contact, UpDir, DeltaTime);
		WheelPosition = Contact + UpDir * Radius;

		// Spring Force
		const float SpringLen = (Start - Hit.Location).Size() - Radius;
		const float DesiredSpringLen = SuspensionHeight * FMath::Clamp(SuspensionRestRatio, 0.f, 1.f);
		const float Offset = DesiredSpringLen - SpringLen;
		SpringMotion(Offset);

		const FVector VelocityAtPoint = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(Start);
		const float VerticalVelocity = FVector::DotProduct(VelocityAtPoint, UpDir);

		const float SurfaceAlignment = FMath::Clamp(UpDot, 0.0f, 1.0f);
		float SpringForceMag = (SuspensionPower * Offset - VerticalVelocity * SuspensionSmoothness) * SurfaceAlignment;
		//SpringForceMag *= GetSuspensionForceMultiplier();

		if (SpringForceMag > KINDA_SMALL_NUMBER)
		{
			const FVector SpringForce = SurfaceNormal * SpringForceMag;
			Vehicle->VehicleMesh->AddForceAtLocation(SpringForce, Contact);

			ApplyAcceleration(Contact, DeltaTime);

			if (bShowDebugLines)
				DebugSuspensionForce(Contact, SpringForce, SpringForceMag, VerticalVelocity);
		}
	}
	else
	{
		bWheelTouchingGround = false;
		CachedHitActor = nullptr;
		CachedHitComponent = nullptr;
		bIsOnSlope = false;

		if (WheelMeshReference)
		{
			const float MaxStretchLength = SuspensionHeight * (1.0f + SuspensionStretchMultiplier);
			const FVector MaxStretchPos = GetComponentLocation() - UpDir * (MaxStretchLength - VisualWheelRadius);
			WheelMeshReference->SetWorldLocation(End);

			const float SpringLen = (Start - End).Size() - VisualWheelRadius;
			const float DesiredSpringLen = SuspensionHeight * FMath::Clamp(SuspensionRestRatio, 0.0f, 1.0f);
			const float Offset = DesiredSpringLen - SpringLen;
			SpringMotion(Offset);
		}
	}

	if (bShowDebugLines)
	{
		DrawDebugSphere(GetWorld(), WheelPosition, Radius, 12, FColor::Orange, false, -1, 0, 1.0f);
	}
}


void USuspension::UpdateSteering(float DeltaTime)
{
	if (!bIsSteeringWheel || !Vehicle) return;

	// === 1. Compute Ackermann Steering Angle ===
	const float SteerInput = Vehicle->SteeringInput * (bNegateSteering ? -1.f : 1.f);
	const float AbsSteer = FMath::Abs(SteerInput);

	float TargetAngleDeg = 0.f;

	if (AbsSteer > KINDA_SMALL_NUMBER)
	{
		const bool bTurningRight = SteerInput > 0.f;
		const bool bIsInnerWheel = (bTurningRight && bIsOnRightSide) || (!bTurningRight && !bIsOnRightSide);

		const float SpeedFactor = FMath::Clamp(CachedSpeedKmh / FMath::Max(1.f, FullSpeedForSteering), 0.f, 1.f);
		const float MaxAngle = FMath::Lerp(SteerAngleAtZeroSpeed, SteerAngleAtFullSpeed, SpeedFactor);
		const float BaseRad = FMath::DegreesToRadians(MaxAngle * AbsSteer);

		const float SinBase = FMath::Sin(BaseRad);
		const float CosBase = FMath::Cos(BaseRad);

		const float IdealInner = FMath::Atan2(2.f * WheelBase * SinBase, 2.f * WheelBase * CosBase - TrackWidth * SinBase);
		const float IdealOuter = FMath::Atan2(2.f * WheelBase * SinBase, 2.f * WheelBase * CosBase + TrackWidth * SinBase);

		const float SteeredInner = AckermannPercent * IdealInner + (1 - AckermannPercent) * BaseRad;
		const float SteeredOuter = AckermannPercent * IdealOuter + (1 - AckermannPercent) * BaseRad;

		float ChosenRad = bIsInnerWheel ? SteeredInner : SteeredOuter;
		if (!bTurningRight) ChosenRad = -ChosenRad;

		TargetAngleDeg = FMath::RadiansToDegrees(ChosenRad);
	}

	// === 2. Apply to suspension fork (not mesh)
	CachedSteeringAngle = FMath::FInterpTo(CachedSteeringAngle, TargetAngleDeg, DeltaTime, SteeringSpeed);
	const FVector SteeringAxis = InitialLocalQuat.GetUpVector();
	const FQuat SteeringQuat = FQuat(SteeringAxis, FMath::DegreesToRadians(CachedSteeringAngle));
	SetRelativeRotation(SteeringQuat * InitialLocalQuat);

	// === 3. Compute Smoothed Countersteer Yaw (but don’t apply it yet)
	if (bVisualCounterSteering && bIsDriftWheel && bIsSteeringWheel && bIsDrifting)
	{
		const FVector Velocity = Vehicle->VehicleMesh->GetComponentVelocity();
		const FVector Forward = Vehicle->VehicleMesh->GetForwardVector();
		const FVector Right = Vehicle->VehicleMesh->GetRightVector();

		const float DriftAngleRad = FMath::Acos(FVector::DotProduct(Velocity.GetSafeNormal(), Forward));
		const float Sideways = FVector::DotProduct(Velocity, Right);
		const float DriftAngleDeg = FMath::Clamp(FMath::RadiansToDegrees(DriftAngleRad) * FMath::Sign(Sideways), -30.f, 30.f);

		CachedVisualYaw = FMath::FInterpTo(CachedVisualYaw, DriftAngleDeg, DeltaTime, SteeringSpeed);

	}
	else
	{
		// Reset yaw smoothly
		CachedVisualYaw = FMath::FInterpTo(CachedVisualYaw, 0.f, DeltaTime, SteeringSpeed);
	}
}


void USuspension::UpdateWheelRotation(float DeltaTime)
{
	if (!WheelMeshReference || !Vehicle || !Vehicle->VehicleMesh) return;

	// === Cache Initial Rotation Once ===
	if (!bHasCachedInitialRotation)
	{
		InitialWheelRotationQuat = WheelMeshReference->GetRelativeRotation().Quaternion();
		bHasCachedInitialRotation = true;
	}

	// === Calculate Delta Pitch ===
	float DeltaPitchDeg = 0.f;

	if (Vehicle->bReadyForBurnout && bWheelTouchingGround && bIsDriveWheel)
	{
		const float BurnoutRPM = 500.f;
		DeltaPitchDeg = -(BurnoutRPM / 60.f) * 360.f * DeltaTime;
		CachedWheelRPM = BurnoutRPM;
	}
	else if (bIsDriveWheel && bSupportsHandbrake && Vehicle->bIsHandBrake)
	{
		CachedWheelRPM = 0.f;
	}
	else if (!bWheelTouchingGround)
	{
		if (bIsDriveWheel && FMath::Abs(Vehicle->GasPaddle) > 0.05f)
		{
			const float AirRPM = Vehicle->GasPaddle * 300.f;
			DeltaPitchDeg = (AirRPM / 60.f) * 360.f * DeltaTime;
			CachedWheelRPM = FMath::Abs(AirRPM);
		}
		else
		{
			const float DecayRate = 20.f;
			CachedWheelRPM = FMath::Max(0.f, CachedWheelRPM - DecayRate * DeltaTime);
			DeltaPitchDeg = (CachedWheelRPM / 60.f) * 360.f * DeltaTime;
		}
	}
	else
	{
		const float Speed = Vehicle->CachedSpeedKmh / 0.036f;
		const float Circumference = 2.f * PI * FMath::Max(VisualWheelRadius, 1.f);
		float RPM = (Speed / Circumference) * 60.f;
		float DirectionSign = FMath::Sign(RPM);
		RPM = FMath::Abs(RPM);

		if (bIsDriveWheel && FMath::Abs(Vehicle->GasPaddle) > KINDA_SMALL_NUMBER && !Vehicle->bIsBraking)
		{
			if (!(Vehicle->bIsMotorcycle && Vehicle->CachedGearIndex == 0))
			{
				const float TargetRPM = Vehicle->GasPaddle * 100.f;
				const float Diff = FMath::Clamp(TargetRPM - RPM, 0.f, 300.f);
				RPM += Diff;

				if (FMath::Abs(Vehicle->CachedSpeedKmh) < Vehicle->BrakingSpeedBeforeReverse)
				{
					if (Vehicle->CachedGearIndex == 0) DirectionSign = -1.f;
					else if (Vehicle->CachedGearIndex > 0) DirectionSign = 1.f;
				}
			}
		}

		RPM *= DirectionSign;
		DeltaPitchDeg = (RPM / 60.f) * 360.f * DeltaTime;
		CachedWheelRPM = FMath::Abs(RPM);
	}

	// === Accumulate Pitch ===
	CachedWheelPitch += DeltaPitchDeg;


	// === Apply Rotation Around Correct Local Axes ===
	const FVector LocalRightAxis = InitialWheelRotationQuat.GetRightVector(); // wheel roll
	const FVector LocalUpAxis = InitialWheelRotationQuat.GetUpVector();       // visual yaw (countersteer)

	const FQuat LocalPitchQuat = FQuat(LocalRightAxis, FMath::DegreesToRadians(CachedWheelPitch));
	const FQuat LocalYawQuat = FQuat(LocalUpAxis, FMath::DegreesToRadians(CachedVisualYaw));

	const FQuat FinalQuat = LocalYawQuat * LocalPitchQuat * InitialWheelRotationQuat;
	WheelMeshReference->SetRelativeRotation(FinalQuat);


}


void USuspension::ApplyTraction(float DeltaTime)
{
	if (!Vehicle || !Vehicle->VehicleMesh || !bWheelTouchingGround || !WheelMeshReference)
		return;

	const float LowSpeedThreshold = 2.5f;
	const bool bTrulyStopped = FMath::Abs(Vehicle->CachedSpeedKmh) < LowSpeedThreshold;

	// === Instant Grip Recovery When Vehicle Is Stopped ===
	if (bTrulyStopped)
	{
		bLostGripFromHandbrake = false;
		bLostGripFromThrottle = false;
		CurrentGrip = GeneralGripStrength;
	}

	if (!bSupportsHandbrake)
	{
		// Reset any residual handbrake grip states
		bLostGripFromHandbrake = false;
		bPreviousHandbrake = false;
	}

	// === FULL STOP LOGIC (no forces if parked & handbraked) ===
	const bool bBlockAllTraction =
		bTrulyStopped &&
		(bSupportsHandbrake && Vehicle->bIsHandBrake) &&
		!Vehicle->bIsInAir &&
		Vehicle->GasPaddle > 0.f;

	if (bBlockAllTraction)
	{
		if (Vehicle->bShowDebugLines)
		{
			const FVector WheelPos = WheelMeshReference->GetComponentLocation();
			DrawDebugString(GetWorld(), WheelPos + FVector(0, 0, 40), TEXT("Traction Blocked"), nullptr, FColor::Red, 0.f, false);
		}
		return;
	}

	const FVector WheelPos = WheelMeshReference->GetComponentLocation();
	const FVector TireVelocity = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(WheelPos);
	const FVector ForwardDir = GetForwardVector();
	const FVector SideDir = bIsSteeringWheel ? GetRightVector() : Vehicle->GetActorRightVector();
	const float ForwardSpeed = FVector::DotProduct(ForwardDir, TireVelocity);
	const float LateralSpeed = FVector::DotProduct(SideDir, TireVelocity);

	const bool bVehicleIsMoving = Vehicle->CachedSpeedKmh > KINDA_SMALL_NUMBER;

	// === Drive Wheel Grip Snap on Throttle Reapply ===
	if (bIsDriveWheel)
	{
		if (!bDriftOnHandbrakeOnly)
		{
			const float Gas = Vehicle->GasPaddle;
			const bool bPressedNow = Gas > 0.1f;
			const bool bWasReleased = PreviousGas <= 0.1f;
			const bool bReappliedThrottle = bPressedNow && bWasReleased;

			if (bReappliedThrottle && bVehicleIsMoving)
			{
				bLostGripFromThrottle = true;
				CurrentGrip = GripLostOnThrottle;
			}

			PreviousGas = Gas;
		}
		else
		{
			bLostGripFromThrottle = false;
			PreviousGas = 0.f;
		}
	}


	// === Handbrake Grip Snap & Recovery ===
	if (bSupportsHandbrake)
	{
		const bool bHandbrakeNow = Vehicle->bIsHandBrake;
		const bool bHandbrakeReleased = bPreviousHandbrake && !bHandbrakeNow;

		if (bDriftOnHandbrakeOnly)
		{
			if (bHandbrakeNow && bVehicleIsMoving)
			{
				bLostGripFromHandbrake = true;
				CurrentGrip = GripLostOnThrottle;
			}
			else if (!bHandbrakeNow)
			{
				// Optional: let go of handbrake resets it (or keep drifting until recovery kicks in)
				// bLostGripFromHandbrake = false;
			}
		}
		else
		{
			if (bHandbrakeReleased && bVehicleIsMoving)
			{
				bLostGripFromHandbrake = true;
				CurrentGrip = GripLostOnThrottle;
			}
		}

		bPreviousHandbrake = bHandbrakeNow;
	}


	// === Global Grip Recovery (even if not handbrake-supported) ===
	const bool bShouldRecover = !bIsDrifting && (bLostGripFromThrottle || bLostGripFromHandbrake);
	if (bShouldRecover)
	{
		CurrentGrip = FMath::FInterpTo(CurrentGrip, GeneralGripStrength, DeltaTime, GripRecoverySpeed);

		if (CurrentGrip >= GeneralGripStrength)
		{
			bLostGripFromThrottle = false;
			bLostGripFromHandbrake = false;
		}
	}

	// === Burnout Mode ===
	if (Vehicle->bReadyForBurnout && (Vehicle->CachedSpeedKmh <= MaxSpeedThatBurnoutGripIsAllowd))
	{
		CurrentGrip = BurnoutGrip;
	}

	// === Base Grip with Optional Curve ===
	float GripAmount = CurrentGrip;

	if (BaseGripSpeedCurve && Vehicle->CachedSpeedKmh > KINDA_SMALL_NUMBER)
	{
		const float NormalizedSpeed = FMath::Clamp(Vehicle->CachedSpeedKmh / Vehicle->DesiredMaxSpeedForFullThrottleKmh, 0.f, 1.f);
		const float CurveScale = BaseGripSpeedCurve->GetFloatValue(NormalizedSpeed);
		GripAmount *= CurveScale;

		if (bShowDebugString)
		{
			DrawDebugString(GetWorld(), WheelPos + FVector(0, 0, 70),
				FString::Printf(TEXT("Base Grip Curve Scale: %.2f"), CurveScale),
				nullptr, FColor::Green, 0.f, true);
		}
	}


	// === Steering Grip Blend ===
	if (bIsDriveWheel && bIsSteeringWheel)
	{
		GripAmount = FMath::Lerp(SteeringWheelGripStrength, CurrentGrip, 0.6f);
	}
	else if (bIsSteeringWheel)
	{
		GripAmount = SteeringWheelGripStrength;
		if (bSupportsHandbrake)
		{
			GripAmount = CurrentGrip * SteeringWheelGripStrength;
		}
	}

	// === Drift Curve Adjustment ===
	const float ActualSpeedKmh = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(WheelPos).Size() * 0.036f;
	const bool bIsDonutMode =
		ActualSpeedKmh < Vehicle->DonutSpeedLimit &&
		FMath::Abs(Vehicle->SteeringInput) > 0.5f &&
		Vehicle->GasPaddle > 0.1f &&
		bIsSteeringWheel;

	if (!bIsDonutMode && bIsDriftWheel && DriftCurve && Vehicle->CachedSpeedKmh > KINDA_SMALL_NUMBER)
	{
		const float NormalizedSpeed = FMath::Clamp(Vehicle->CachedSpeedKmh / Vehicle->DesiredMaxSpeedForFullThrottleKmh, 0.f, 1.f);
		GripAmount *= DriftCurve->GetFloatValue(NormalizedSpeed);
	}


	// === Turning Grip Loss Multiplier ===
	if (FMath::Abs(Vehicle->SteeringInput) > KINDA_SMALL_NUMBER)
	{
		GripAmount *= GripWhileTurningMultiplier;
	}

	// === Handbrake Lock Force ===
	if (bSupportsHandbrake && Vehicle->bIsHandBrake)
	{
		if (!bIsSteeringWheel || !bIsDriveWheel) // rear wheels only
		{
			const float SpeedKmh = Vehicle->CachedSpeedKmh;
			const float SpeedAlpha = FMath::Clamp(SpeedKmh / MaxSpeedForHandbrakeFade, 0.f, 1.f);
			const float HB_Force = FMath::Lerp(BaseHandbrakeForce, HandbrakeForceAtMaxSpeed, SpeedAlpha);

			const FVector Velocity = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(WheelPos);

			// === Directional Braking Instead of Alignment ===
			if (bApplyDirectionalHandBrakingInsteadOfAligning)
			{
				if (!Velocity.IsNearlyZero())
				{
					const FVector BrakeDir = -Velocity.GetSafeNormal();
					FVector BrakeForce = BrakeDir * HB_Force * Vehicle->ActualMass;

					Vehicle->VehicleMesh->AddForceAtLocation(BrakeForce, WheelPos);

					if (bShowDebugLines)
					{
						DrawDebugLine(GetWorld(), WheelPos + FVector(0, 0, 10),
							WheelPos + FVector(0, 0, 10) + BrakeForce * 0.01f,
							FColor::Magenta, false, -1.f, 0, 2.5f);

						DrawDebugString(GetWorld(), WheelPos + FVector(0, 0, 30),
							FString::Printf(TEXT("[HB] Dir Force: %.0f"), BrakeForce.Size()),
							nullptr, FColor::Magenta, 0.f, true);
					}
				}
			}
			else
			{
				const FVector LocalWheelVelocity = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(WheelPos);
				if (!LocalWheelVelocity.IsNearlyZero())
				{
					const FVector VehicleRight = GetRightVector();
					const float LateralSlipSpeed = FVector::DotProduct(VehicleRight, LocalWheelVelocity); // Drift intensity
					const FVector LateralSlipDir = VehicleRight * FMath::Sign(LateralSlipSpeed); // Drift direction (sideways)

					// === Threshold check to avoid micro-corrections ===
					const bool bShouldCounterDrift = FMath::Abs(LateralSlipSpeed) > HandbrakeCounterDriftSpeedThreshold;

					if (bShouldCounterDrift)
					{
						const float SpeedFadeAlpha = FMath::Clamp(Vehicle->CachedSpeedKmh / MaxSpeedForHandbrakeFade, 0.f, 1.f);
						const float UprightForceStrength = FMath::Lerp(BaseHandbrakeForce, HandbrakeForceAtMaxSpeed, SpeedFadeAlpha) * 0.5f;

						const FVector UprightCorrectionForce =
							-LateralSlipDir *
							FMath::Abs(LateralSlipSpeed) *
							UprightForceStrength *
							HandbrakeCounterDriftUprightForceMultiplier *
							0.01f *
							Vehicle->ActualMass;

						Vehicle->VehicleMesh->AddForceAtLocation(UprightCorrectionForce, WheelPos);

						if (bShowDebugLines)
						{
							DrawDebugLine(GetWorld(), WheelPos + FVector(0, 0, 10),
								WheelPos + FVector(0, 0, 10) + UprightCorrectionForce * 0.01f,
								FColor::Cyan, false, -1.f, 0, 2.f);

							DrawDebugString(GetWorld(), WheelPos + FVector(0, 0, 30),
								FString::Printf(TEXT("[HB Upright] DriftSpeed: %.0f | Force: %.0f"),
									LateralSlipSpeed, UprightCorrectionForce.Size()),
								nullptr, FColor::Cyan, 0.f, true);
						}
					}

					// === Passive Handbrake Slowdown (when not counter-drifting) ===
					if (!bShouldCounterDrift)
					{
						const float PassiveFadeAlpha = FMath::Clamp(SpeedKmh / MaxSpeedForHandbrakeFade, 0.f, 1.f);
						const float PassiveHB_Force = FMath::Lerp(BaseHandbrakeForce, HandbrakeForceAtMaxSpeed, PassiveFadeAlpha) * PassiveHandbrakeSlowdownMultiplier;

						const FVector SlowdownForce = -LocalWheelVelocity.GetSafeNormal() *
							PassiveHB_Force *
							Vehicle->ActualMass;

						Vehicle->VehicleMesh->AddForceAtLocation(SlowdownForce, WheelPos);

						if (bShowDebugLines)
						{
							DrawDebugLine(GetWorld(), WheelPos + FVector(0, 0, 10),
								WheelPos + FVector(0, 0, 10) + SlowdownForce * 0.01f,
								FColor::Red, false, -1.f, 0, 2.f);

							DrawDebugString(GetWorld(), WheelPos + FVector(0, 0, 30),
								FString::Printf(TEXT("[HB Passive] SlowdownForce: %.0f"), SlowdownForce.Size()),
								nullptr, FColor::Red, 0.f, true);
						}
					}
				}
			}
		}
	}


	// === Debug Grip Readout ===
	bIsRecoveringGrip = bLostGripFromThrottle || bLostGripFromHandbrake;
	CachedCurrentGrip = GripAmount;

	if (bShowDebugString)
	{
		DrawDebugString(GetWorld(), WheelPos + FVector(0, 0, 50),
			FString::Printf(TEXT("Grip: %.2f | Recovering: %s"), GripAmount, bIsRecoveringGrip ? TEXT("YES") : TEXT("NO")),
			nullptr, FColor::White, 0.f, true);
	}

	// === Apply Lateral & Longitudinal Grip Forces ===
	ApplySideTraction(WheelPos, SideDir, LateralSpeed, GripAmount, DeltaTime);
	const FVector DragForwardDir = TireVelocity.GetSafeNormal();
	ApplyForwardDrag(WheelPos, DragForwardDir, ForwardSpeed, GripAmount);
}




void USuspension::ApplySideTraction(const FVector& WheelPos, const FVector& SideDir, float LateralSpeed, float GripMultiplier, float DeltaTime)
{
	if (FMath::Abs(LateralSpeed) < 5.f)
		return;

	const FVector LateralVelocity = SideDir * LateralSpeed;

	// Time-independent damping force
	FVector SideForce = -LateralVelocity * Vehicle->ActualMass * GripMultiplier;

	const float GravityZ = FMath::Abs(GetWorld()->GetGravityZ());
	const float MaxSideForce = Vehicle->ActualMass * GravityZ * GripMultiplier;
	SideForce = SideForce.GetClampedToMaxSize(MaxSideForce);

	Vehicle->VehicleMesh->AddForceAtLocation(SideForce, WheelPos);

	if (bShowDebugLines)
	{
		DrawDebugLine(GetWorld(), WheelPos + FVector(0, 0, 15),
			WheelPos + FVector(0, 0, 15) + (SideForce / Vehicle->ActualMass * 0.02f),
			FColor::Yellow, false, -1.f, 0, 2.0f);
	}
	if (bShowDebugString)
	{
		DrawDebugString(GetWorld(), WheelPos + FVector(0, 0, 20),
			FString::Printf(TEXT("LT: %f"), FMath::Abs(LateralSpeed)),
			nullptr, FColor::Yellow, 0.f, true);
	}
}


void USuspension::ApplyForwardDrag(const FVector& WheelPos, const FVector& ForwardDir, float ForwardSpeed, float GripMultiplier)
{
	// Calculate actual local velocity along the wheel's forward axis
	const FVector Velocity = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(WheelPos);
	const float SignedSpeed = FVector::DotProduct(ForwardDir, Velocity); // Keep sign

	// Only apply drag if there's actual motion
	if (FMath::Abs(SignedSpeed) > 0.f)
	{
		const FVector DampingForce = -ForwardDir * SignedSpeed * Vehicle->ActualMass * BaseDragForce * GripMultiplier;
		Vehicle->VehicleMesh->AddForceAtLocation(DampingForce, WheelPos);
	}
}




void USuspension::UpdateCachedSpeed()
{
	// 1. Safety Check
	if (!Vehicle || !Vehicle->VehicleMesh) return;

	// 2. Determine Direction & Velocity at Suspension Location
	const FVector ForwardDir = GetForwardVector();
	const FVector PointVelocity = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(GetComponentLocation());

	// 3. Project velocity onto forward axis and convert to km/h
	CachedSpeedKmh = FVector::DotProduct(ForwardDir, PointVelocity) * 0.036f;
}

void USuspension::DebugSuspensionForce(const FVector& Contact, const FVector& SpringForce, float SpringForceMag, float VerticalVelocity)
{
	// 1. Safety Check
	if (!Vehicle || !Vehicle->VehicleMesh) return;

	if (bShowDebugLines)
	{
		// 2. Calculate Scaled Force Vector for Debug Arrow
		const float Mass = FMath::Max(Vehicle->VehicleMesh->GetMass() * 2.f, 1.f);
		const FVector ScaledForce = SpringForce / Mass / 2;

		// 3. Draw Spring Force Line
		DrawDebugLine(GetWorld(), Contact + (GetForwardVector() * 5), Contact + ScaledForce + (GetForwardVector() * 5), FColor::Emerald, false, -1.f, 0, 2.5f);
	}


	if (bShowDebugString)
	{
		// 4. Display Force Magnitude and Vertical Velocity
		DrawDebugString(GetWorld(), Contact + FVector(0, 0, 0),
			FString::Printf(TEXT("SpringMag: %.0f | VerticalVel: %.2f"), SpringForceMag, VerticalVelocity),
			nullptr, FColor::White, 0.0f, true);

		// 5. Show Suspension Overextension
		DrawDebugString(GetWorld(), Contact + FVector(0, 0, 40),
			FString::Printf(TEXT("[Suspension] Overextend: %.2f"), SuspensionStretchMultiplier),
			nullptr, FColor::Green, 0.f, true);
	}
}

void USuspension::DebugAccelerationForce(const FVector& ContactPoint, const FVector& Force)
{
	// 1. Safety Check
	if (!Vehicle || !Vehicle->VehicleMesh) return;

	// 2. Debug Force Line Calculation
	const float SpeedKmh = CachedSpeedKmh;
	//const FColor Color = FColor(160, 32, 240); // Purple
	const FColor Color = FColor::Orange;
	const float LineLength = FMath::Clamp(Force.Size() / Vehicle->ActualMass, 5.f, 1000.f);

	// 3. Draw Acceleration Force Vector

	if (bShowDebugLines)
	{
		DrawDebugLine(GetWorld(), ContactPoint + FVector(0.f, 0.f, 10.f), ContactPoint + FVector(0.f, 0.f, 10.f) + Force.GetSafeNormal() * LineLength, Color, false, -1, 0, 2.5f);
	}

	if (bShowDebugString)
	{
		// 4. Annotate Force Details
		DrawDebugString(GetWorld(), ContactPoint + FVector(0, 0, 80),
			FString::Printf(TEXT("[Accel] Force: %.0f | Gas: %.2f | Speed: %.1f km/h"),
				Force.Size(), Vehicle->GasPaddle, SpeedKmh),
			nullptr, Color, 0.0f, true);
	}
}
void USuspension::DebugHandbrakeForce(float SpeedKmh, float BrakeMultiplier)
{
	if (!GetWorld() || !bShowDebugString) return;

	const FVector StartPos = GetComponentLocation() + FVector(0.f, 0.f, 100.f);

	if (bShowDebugString)
	{
		DrawDebugString(
			GetWorld(),
			StartPos,
			FString::Printf(TEXT("HB: %.1f km/h | x%.2f"), SpeedKmh, BrakeMultiplier),
			nullptr,
			FColor::Green,
			0.f,
			true
		);
	}
}

void USuspension::DebugCurrentGear()
{
	if (!bShowDebugString || !Vehicle || CurrentGearIndex < 0 || !Vehicle->GearPushForces.IsValidIndex(Vehicle->CachedGearIndex))
		return;

	if (bShowDebugString)
	{
		DrawDebugString(GetWorld(), GetComponentLocation() + FVector(0, 0, 120),
			FString::Printf(TEXT("Gear %d | PushForce %.1f"),
				CurrentGearIndex, Vehicle->GearPushForces[Vehicle->CachedGearIndex]),
			nullptr, FColor::Green, 0.f, true);
	}
}


void USuspension::UpdateDriftAndBrakingState(float DeltaTime)
{
	if (!bWheelTouchingGround || !Vehicle || !Vehicle->VehicleMesh || !WheelMeshReference)
	{
		bIsDrifting = false;
		DriftYawAngle = 0.f;
		return;
	}

	// === Smooth Nitro Override: Grip + Velocity Correction ===
	if (Vehicle->bNitroActive && bNitroEffectsTraction)
	{
		bIsDrifting = false;
		DriftYawAngle = 0.f;

		// Smooth grip recovery
		CurrentGrip = FMath::FInterpTo(CurrentGrip, GeneralGripStrength, DeltaTime, NitroGripRecoveryRate);

		// Smooth velocity correction
		const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
		const FVector CurrentVelocity = Vehicle->VehicleMesh->GetPhysicsLinearVelocity();
		const float ForwardSpeed = FVector::DotProduct(Forward, CurrentVelocity);
		const FVector TargetVelocity = (Forward * ForwardSpeed) + FVector(0.f, 0.f, CurrentVelocity.Z);
		const FVector SmoothedVelocity = FMath::VInterpTo(CurrentVelocity, TargetVelocity, DeltaTime, NitroVelocityLockStrength);

		Vehicle->VehicleMesh->SetPhysicsLinearVelocity(SmoothedVelocity);
		return;
	}

	const FVector WheelPos = WheelMeshReference->GetComponentLocation();
	FVector VelocityAtWheel = Vehicle->VehicleMesh->GetPhysicsLinearVelocityAtPoint(WheelPos);
	VelocityAtWheel.Z = 0.f;

	const FVector Forward = GetForwardVector().GetSafeNormal2D();
	const FVector Right = GetRightVector().GetSafeNormal2D();

	const float ForwardVel = FVector::DotProduct(Forward, VelocityAtWheel);
	const float LateralVel = FVector::DotProduct(Right, VelocityAtWheel);
	const float SpeedKmh = CachedSpeedKmh;

	const float DriftAngleDeg = FMath::RadiansToDegrees(FMath::Acos(
		FMath::Clamp(FVector::DotProduct(Forward, VelocityAtWheel.GetSafeNormal2D()), -1.f, 1.f)));

	const float LateralRatio = FMath::Abs(LateralVel) / (FMath::Abs(ForwardVel) + 1.f);
	const bool bAngleIsSliding = DriftAngleDeg > 25.f;
	const bool bLateralSlipStrong = LateralRatio > 0.25f;
	const bool bIsFastEnough = SpeedKmh > 10.f;
	const bool bHasStrongYawChange = FMath::Abs(Vehicle->SteeringInput) > 0.4f;

	const bool bShouldDrift = bIsFastEnough && (bLateralSlipStrong || bAngleIsSliding || bHasStrongYawChange);

	if (!bIsDrifting && bShouldDrift)
	{
		DriftEntryYaw = Vehicle->GetActorRotation().Yaw;
	}

	bIsDrifting = bShouldDrift;

	if (bIsDrifting)
	{
		const float CurrentYaw = Vehicle->GetActorRotation().Yaw;
		DriftYawAngle = FMath::FindDeltaAngleDegrees(CurrentYaw, DriftEntryYaw);
	}
	else
	{
		DriftYawAngle = 0.f;
	}

	// === DriftAssist — Disabled During Nitro ===
	if (bDriftAssist && !Vehicle->bNitroActive && bIsRecoveringGrip && Vehicle->GasPaddle != KINDA_SMALL_NUMBER)
	{
		const float AngleToForward = FMath::RadiansToDegrees(FMath::Acos(
			FVector::DotProduct(GetForwardVector(), Vehicle->VehicleMesh->GetPhysicsLinearVelocity().GetSafeNormal())
		));

		const float AlignmentMultiplier = FMath::GetMappedRangeValueClamped(
			FVector2D(DriftAssistMaxAngleForSlowRecovery, 0.f),
			FVector2D(DriftAssistMinRecoveryMultiplier, DriftAssistMaxRecoveryMultiplier),
			AngleToForward
		);

		const float AdjustedRecoveryRate = GripRecoverySpeed * AlignmentMultiplier;
		CurrentGrip = FMath::FInterpTo(CurrentGrip, GeneralGripStrength, DeltaTime, AdjustedRecoveryRate);
		CurrentGrip = FMath::Clamp(CurrentGrip, 0.f, GeneralGripStrength);
	}
}


void USuspension::UpdateVisualWheel(const FVector& ContactPoint, const FVector& UpDir, float DeltaTime)
{
	if (!WheelMeshReference || !Vehicle || !Vehicle->VehicleMesh) return;

	const FVector WheelPosition = ContactPoint + UpDir * VisualWheelRadius;
	WheelMeshReference->SetWorldLocation(WheelPosition);
}



void USuspension::SpringMotion(float Offset)
{
	if (!Vehicle) return;

	const bool bVehicleStopped =
		FMath::Abs(Vehicle->CachedSpeedKmh) < 1.5f; // physically stopped

	const bool bNoGas = FMath::Abs(Vehicle->GasPaddle) < 0.01f;

	const FRotator CurRot = GetRelativeRotation();
	const FRotator InitRot = InitialLocalRotation;

	// Ignore local Z (Yaw)
	const bool bRotated =
		FMath::Abs(CurRot.Pitch) > 0.f ||
		FMath::Abs(CurRot.Roll) > 0.f;

	if (bVehicleStopped && bNoGas && bRotated) return;

	const FTransform VehicleTransform = Vehicle->GetActorTransform();

	if (!bIsNormalSpring)
	{
		// === FOLLOW LOCAL POS/ROTATION ===
		if (LeftSpringMeshReference)
		{
			const FVector BasePos = VehicleTransform.TransformPosition(LeftSpringLocalTransform.GetLocation());
			const FQuat BaseRot = VehicleTransform.GetRotation() * LeftSpringLocalTransform.GetRotation();
			const FVector SlideDir = BaseRot.GetUpVector(); // Local Up of the base

			LeftSpringMeshReference->SetWorldLocation(BasePos + SlideDir * Offset);
			LeftSpringMeshReference->SetWorldRotation(BaseRot);
		}

		if (RightSpringMeshReference)
		{
			const FVector BasePos = VehicleTransform.TransformPosition(RightSpringLocalTransform.GetLocation());
			const FQuat BaseRot = VehicleTransform.GetRotation() * RightSpringLocalTransform.GetRotation();
			const FVector SlideDir = BaseRot.GetUpVector(); // Local Up of the base

			RightSpringMeshReference->SetWorldLocation(BasePos + SlideDir * Offset);
			RightSpringMeshReference->SetWorldRotation(BaseRot);
		}

		if (LeftBaseMeshReference)
		{
			const FVector WorldLocation = VehicleTransform.TransformPosition(LeftBaseLocalTransform.GetLocation());
			const FQuat WorldRotation = VehicleTransform.GetRotation() * LeftBaseLocalTransform.GetRotation();
			LeftBaseMeshReference->SetWorldLocation(WorldLocation);
			LeftBaseMeshReference->SetWorldRotation(WorldRotation);
		}

		if (RightBaseMeshReference)
		{
			const FVector WorldLocation = VehicleTransform.TransformPosition(RightBaseLocalTransform.GetLocation());
			const FQuat WorldRotation = VehicleTransform.GetRotation() * RightBaseLocalTransform.GetRotation();
			RightBaseMeshReference->SetWorldLocation(WorldLocation);
			RightBaseMeshReference->SetWorldRotation(WorldRotation);
		}
	}
	else
	{
		// === FIXED: NORMAL SPRING MODE (SCALE BASED) ===
		const float MaxCompression = SuspensionHeight;
		const float RawCompressionRatio = FMath::Clamp(Offset / MaxCompression, 0.f, 1.f);

		// Amplify visual compression
		const float SquishMultiplier = SpringSquishMultiplier;
		const float CompressionRatio = FMath::Clamp(RawCompressionRatio * SquishMultiplier, 0.f, 1.f);
		const float MinZScale = SpringMinVisualZScale;

		// LEFT SPRING
		if (LeftSpringMeshReference)
		{
			const float NewZ = FMath::Lerp(LeftSpringInitialScale.Z, MinZScale, CompressionRatio);
			const FVector NewScale = FVector(LeftSpringInitialScale.X, LeftSpringInitialScale.Y, NewZ);
			LeftSpringMeshReference->SetWorldScale3D(NewScale);
		}

		// RIGHT SPRING
		if (RightSpringMeshReference)
		{
			const float NewZ = FMath::Lerp(RightSpringInitialScale.Z, MinZScale, CompressionRatio);
			const FVector NewScale = FVector(RightSpringInitialScale.X, RightSpringInitialScale.Y, NewZ);
			RightSpringMeshReference->SetWorldScale3D(NewScale);
		}

		// LEFT BASE
		if (LeftBaseMeshReference)
		{
			const float NewZ = FMath::Lerp(LeftBaseInitialScale.Z, MinZScale, CompressionRatio);
			const FVector NewScale = FVector(LeftBaseInitialScale.X, LeftBaseInitialScale.Y, NewZ);
			LeftBaseMeshReference->SetWorldScale3D(NewScale);
		}

		// RIGHT BASE
		if (RightBaseMeshReference)
		{
			const float NewZ = FMath::Lerp(RightBaseInitialScale.Z, MinZScale, CompressionRatio);
			const FVector NewScale = FVector(RightBaseInitialScale.X, RightBaseInitialScale.Y, NewZ);
			RightBaseMeshReference->SetWorldScale3D(NewScale);
		}
	}
}

void USuspension::SkidMark(float DeltaTime)
{
	// =====================================================
	// Skip skid marks if ground under wheel is moving
	// (same component, but its transform changed)
	// =====================================================

	if (!SkidNiagaraSystem || !Vehicle || !bWheelTouchingGround)
		return;

	// -----------------------------------------------------
	// Track last ground component transform PER suspension
	// -----------------------------------------------------
	static TMap<const USuspension*, UPrimitiveComponent*> PrevGroundComp;
	static TMap<const USuspension*, FTransform> PrevGroundXf;

	if (CachedHitComponent)
	{
		const FTransform NowXf = CachedHitComponent->GetComponentTransform();

		if (PrevGroundComp.Contains(this) &&
			PrevGroundComp[this] == CachedHitComponent)
		{
			const FTransform& PrevXf = PrevGroundXf[this];

			const bool bMoved =
				!PrevXf.GetLocation().Equals(NowXf.GetLocation(), 0.01f) ||
				!PrevXf.GetRotation().Equals(NowXf.GetRotation(), 0.0001f);

			if (bMoved)
			{
				// Moving platform -> NO skid
				if (ActiveSkidComponent)
				{
					ActiveSkidComponent->Deactivate();
					ActiveSkidComponent = nullptr;
				}
				if (SkidAudio && SkidAudio->IsPlaying())
				{
					SkidAudio->Stop();
					SkidAudio = nullptr;
				}

				PrevGroundXf[this] = NowXf;
				return;
			}
		}

		PrevGroundComp.FindOrAdd(this) = CachedHitComponent;
		PrevGroundXf.FindOrAdd(this) = NowXf;
	}
	else
	{
		PrevGroundComp.Remove(this);
		PrevGroundXf.Remove(this);
		return;
	}

	// =====================================================
	// ORIGINAL SKID LOGIC (unchanged)
	// =====================================================

	const float RPM = Vehicle->CachedRPM;
	const float Speed = CachedSpeedKmh;
	const float Gas = FMath::Abs(Vehicle->GasPaddle);
	const int32 GearIndex = Vehicle->CachedGearIndex;
	const bool bClutchEngaged = !Vehicle->bClutchEnabled || Vehicle->Clutch >= 0.9f;
	const bool bInNeutral = GearIndex < 0;
	const bool bIsHandbrake = Vehicle->bIsHandBrake;

	const bool bVisualDrift =
		FMath::Abs(DriftYawAngle) > DriftVisualAngleThresholdDeg &&
		Speed > DriftVisualMinForwardSpeed;

	const bool bRecoveringDrift = bIsRecoveringGrip && bIsDrifting;

	bool bHighRPM = false;
	if (GearIndex >= 0 && Vehicle->GearSpeedThresholdsKmh.IsValidIndex(GearIndex))
	{
		const float MaxSpeed = FMath::Max(Vehicle->GearSpeedThresholdsKmh[GearIndex], 1.f);
		const float SpeedRatio = FMath::Clamp(Speed / MaxSpeed, 0.f, 1.f);
		const float ExpectedRPM = SpeedRatio * Vehicle->MaxRPM;

		if (bIsDriveWheel)
		{
			bHighRPM = (RPM > ExpectedRPM + 500.f) && (Gas > 0.4f) && bClutchEngaged;
		}
	}

	const bool bBurnoutInPlace =
		bIsDriveWheel && !bInNeutral && Gas > 0.6f && RPM > 3500.f && Speed < 10.f;

	const bool bTankSkid =
		bUseTankSteering && bIsDriveWheel &&
		FMath::Abs(Vehicle->SteeringInput) > 0.1f &&
		Gas > 0.4f && Speed < 8.f;

	const bool bHandbrakeSkid =
		bSupportsHandbrake && bIsHandbrake && Speed > 5.f;

	const bool bSteeringSkid =
		bIsSteeringWheel &&
		FMath::Abs(Vehicle->SteeringInput) > 0.4f &&
		Speed < 15.f && Gas > 0.5f;

	const bool bForceDriftSkid =
		bIsDriftWheel && bIsDriveWheel && Vehicle->bIsDrifting;

	const bool bShouldSkid =
		bForceDriftSkid ||
		bRecoveringDrift ||
		bVisualDrift ||
		bHighRPM ||
		bBurnoutInPlace ||
		bTankSkid ||
		bHandbrakeSkid ||
		bSteeringSkid;

	if (!bShouldSkid)
	{
		if (ActiveSkidComponent)
		{
			ActiveSkidComponent->Deactivate();
			ActiveSkidComponent = nullptr;
		}
		if (SkidAudio && SkidAudio->IsPlaying())
		{
			SkidAudio->Stop();
			SkidAudio = nullptr;
		}
		return;
	}

	const FVector SpawnLocation = CachedSuspensionContactPoint;

	if (!ActiveSkidComponent || !ActiveSkidComponent->IsActive())
	{
		ActiveSkidComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			SkidNiagaraSystem,
			SpawnLocation,
			FRotator::ZeroRotator,
			FVector(1.f),
			true,
			true,
			ENCPoolMethod::AutoRelease
		);

		if (SkidSoundCue && !SkidAudio)
		{
			SkidAudio = UGameplayStatics::SpawnSoundAttached(
				SkidSoundCue,
				Vehicle->VehicleMesh,
				NAME_None,
				FVector::ZeroVector,
				EAttachLocation::KeepRelativeOffset,
				true,
				0.5f,
				1.0f,
				0.0f,
				SkidAttenuationSettings
			);
		}
	}
	else
	{
		ActiveSkidComponent->SetWorldLocation(SpawnLocation);
	}
}
