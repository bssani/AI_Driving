// Copyright Cena Abachi - Youtube: Devlogerio - devloger.io@gmail.com - Publicated on 2025 - Last update 01/2026 - All Rights Reserved
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Vehicle.h"
#include "Suspension.generated.h"

UENUM(BlueprintType)
enum class EWheelSimulationMode : uint8
{
	SimpleLine        UMETA(DisplayName = "Simple Line Trace"),
	Sphere            UMETA(DisplayName = "Sphere Trace"),
	SuperRealistic    UMETA(DisplayName = "Super Realistic"),
	Motorcycle        UMETA(DisplayName = "Motorcycle")
};

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class ADVANCEDCARSANDBIKESSYSTEM_API USuspension : public USceneComponent
{
	GENERATED_BODY()

public:
	USuspension();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties")
	float MinFunctionalFPS = 10.f;

	// ==========================
	// Properties | Suspension
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	bool bCanSlopeLock = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float SuspensionHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float SuspensionPower = 40000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float SuspensionSmoothness = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float SuspensionRestRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float SuspensionStretchMultiplier = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float VisualWheelRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	EWheelSimulationMode SimulationMode = EWheelSimulationMode::SuperRealistic;

	UPROPERTY(EditAnywhere, Category = "Properties|Suspension", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float SuperRealisticMaxSideImpactAngleDeg = 66.0f;

	UPROPERTY(EditAnywhere, Category = "Properties|Suspension", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float SuperRealisticMaxGroundSlopeAngleDeg = 72.0f;

	UPROPERTY(EditAnywhere, Category = "Properties|Suspension")
	bool bRotateWithWheel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float ExtraStretchFadeMaxAngle = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	bool bIsNormalSpring = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float SpringMinVisualZScale = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Suspension")
	float SpringSquishMultiplier = 2.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Suspension")
	FVector CachedSuspensionContactPoint = FVector::ZeroVector;

	// ==========================
	// Properties | FX
	// ==========================
	UPROPERTY(EditAnywhere, Category = "Properties|FX")
	class UAudioComponent* SkidAudio;

	UPROPERTY(EditAnywhere, Category = "Properties|FX")
	class UNiagaraSystem* SkidNiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX")
	class USoundCue* SkidSoundCue = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX")
	class USoundAttenuation* SkidAttenuationSettings;

	class UNiagaraComponent* ActiveSkidComponent = nullptr;

	// ==========================
	// Properties | Drive & Brake
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Drive & Brake")
	bool bIsDriveWheel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Drive & Brake")
	bool bCanBrake = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Drive & Brake")
	float BrakingForce = 400.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Drive & Brake")
	float BrakingForceOnMaxSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Drive & Brake")
	UCurveFloat* SpeedBasedAccelCurve = nullptr;

	// ==========================
	// Properties | Steering
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	bool bIsSteeringWheel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	bool bIsOnRightSide = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	bool bNegateSteering = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float SteeringSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float SteeringResetSpeed = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float FullSpeedForSteering = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float SteerAngleAtZeroSpeed = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float SteerAngleAtFullSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float WheelBase = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float TrackWidth = 155.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float AckermannPercent = 1.0f;

	// ==========================
	// Properties | Traction
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	float GeneralGripStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	float BurnoutGrip = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	float MaxSpeedThatBurnoutGripIsAllowd = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	float GripWhileTurningMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	float DriftVisualAngleThresholdDeg = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	float DriftVisualMinForwardSpeed = 400.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Traction")
	float GripRecoverySpeed = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	bool bIsDriftWheel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	UCurveFloat* DriftCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	bool bDriftAssist = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Traction")
	UCurveFloat* BaseGripSpeedCurve = nullptr;

	// ==========================
	// Properties | Drift Assist
	// ==========================
	UPROPERTY(EditAnywhere, Category = "Properties|Drift Assist")
	bool bVisualCounterSteering = false;

	UPROPERTY(EditAnywhere, Category = "Properties|Drift Assist")
	float DriftAssistMaxAngleForSlowRecovery = 90.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Drift Assist")
	float DriftAssistMinRecoveryMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Properties|Drift Assist")
	float DriftAssistMaxRecoveryMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Properties|Drift Assist")
	float TireSlipperyWhileDrifting = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Properties|Drift Assist")
	float LowSpeedDriftThresholdKmh = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	float CachedWheelPitch = 0.f;

	FRotator InitialWheelRotation;
	FQuat InitialWheelRotationQuat;
	bool bHasCachedInitialRotation = false;

	// ==========================
	// Properties | Grip
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Grip")
	bool bDriftOnHandbrakeOnly = true;

	UPROPERTY(EditAnywhere, Category = "Properties|Grip")
	float SteeringWheelGripStrength = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Properties|Grip")
	float GripLostOnThrottle = 0.5f;

	UPROPERTY(VisibleAnywhere, Category = "Properties|Grip")
	float CachedCurrentGrip = 1.f;

	// ==========================
	// Properties | Nitro
	// ==========================
	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	bool bNitroEffectsTraction = false;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float NitroGripRecoveryRate = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float NitroVelocityLockStrength = 1.5f;

	// ==========================
	// Properties | Handbrake
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Handbrake")
	bool bSupportsHandbrake = false;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	bool bApplyDirectionalHandBrakingInsteadOfAligning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Handbrake")
	float MinHandbrakeGrip = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Handbrake")
	float MaxHandbrakeGrip = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Handbrake")
	float HandbrakeFadeSpeed = 90.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	float BaseHandbrakeForce = 4500.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	float HandbrakeForceAtMaxSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	float MaxSpeedForHandbrakeFade = 150.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	float HandbrakeUprightCorrectionMultiplier = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	float PassiveHandbrakeSlowdownMultiplier = 3.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	float HandbrakeCounterDriftUprightForceMultiplier = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Properties|Handbrake")
	float HandbrakeCounterDriftSpeedThreshold = 50.f;

	// ==========================
	// Properties | Tank Steering
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Tank Steering")
	bool bUseTankSteering = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Tank Steering")
	bool bIsRightSideWheel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Tank Steering")
	float TankSteeringForceMultiplier = 1.0f;

	// ==========================
	// Properties | Slope Assist
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Slope Assist")
	float SlopeEnterAngle = 1.f;

	// ==========================
	// Properties | Damping
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Damping")
	float BaseDragForce = 0.0096f;

	// ==========================
	// Properties | Runtime
	// ==========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	bool bWheelTouchingGround = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	bool bIsOnSlope = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	float CachedSpeedKmh = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	float CachedThrottleForce = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	float CachedSteeringAngle = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	float CachedWheelRPM = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	int32 CurrentGearIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	bool bIsDrifting = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	float DriftYawAngle = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime")
	float DriftEntryYaw = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime|Grip")
	bool bIsRecoveringGrip = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime|Grip")
	bool bPreviousHandbrake = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime|Grip")
	bool bLostGripFromHandbrake = false;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	float CachedVisualYaw = 0.f;

	// ==========================
	// Properties | Info
	// ==========================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties|Info")
	FName SuspensionName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties|Info")
	bool bNoThrottleWheelSpinOnReverse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Info")
	bool bShowDebugLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Info")
	bool bShowDebugString = false;

	// ==========================
	// Internal (Non-UPROPERTY)
	// ==========================
	FRotator InitialLocalRotation;
	FQuat InitialLocalQuat;
	FVector CachedGroundNormal = FVector::ZeroVector;

	bool bLostGripFromThrottle = false;
	float PreviousGas = 0.f;
	float CurrentGrip = 1.f;

	class AVehicle* Vehicle = nullptr;
	UStaticMeshComponent* WheelMeshReference = nullptr;
	UStaticMeshComponent* LeftSpringMeshReference = nullptr;
	UStaticMeshComponent* RightSpringMeshReference = nullptr;
	UStaticMeshComponent* LeftBaseMeshReference = nullptr;
	UStaticMeshComponent* RightBaseMeshReference = nullptr;
	FTransform LeftSpringLocalTransform;
	FTransform RightSpringLocalTransform;
	FTransform LeftBaseLocalTransform;
	FTransform RightBaseLocalTransform;
	FVector LeftSpringInitialScale;
	FVector RightSpringInitialScale;
	FVector LeftBaseInitialScale;
	FVector RightBaseInitialScale;

	bool bFallbackToLineTrace = false;
	FVector VisualTargetLocation = FVector::ZeroVector;
	FVector VisualUpDirection = FVector::UpVector;
	// Moving base follow
	UPrimitiveComponent* CachedMovingBase = nullptr;
	FTransform CachedRelativeToBase;
	bool bHasCachedBase = false;
	UPrimitiveComponent* CachedSkidBase = nullptr;
	FTransform LastBaseTransform;
	AActor* CachedHitActor = nullptr;
	UPrimitiveComponent* CachedHitComponent = nullptr;

	// ==========================
	// Public Methods
	// ==========================
	void UpdateCachedSpeed();
	FVector ComputeDampingForce();
	void FollowMovingBase_Delta(float DeltaTime);
	void ApplyAcceleration(const FVector& ContactPoint, float DeltaTime);

	// These were previously declared correctly, keep them:
	void UpdateSteering(float DeltaTime);
	void SimulateSuspensionSimple(float DeltaTime);
	void SimulateSuspensionMotorcycle(float DeltaTime);
	void SimulateSuspensionWithSphere(float DeltaTime);
	void ApplyTraction(float DeltaTime);
	void UpdateDriftAndBrakingState(float DeltaTime);
	void UpdateWheelRotation(float DeltaTime);
	void SkidMark(float DeltaTime);
	// Core Overrides
	void ApplySideTraction(const FVector& WheelPos, const FVector& SideDir, float LateralSpeed, float GripMultiplier, float DeltaTime);
	void ApplyForwardDrag(const FVector& WheelPos, const FVector& ForwardDir, float ForwardSpeed, float GripMultiplier);
	void DebugSuspensionForce(const FVector& Contact, const FVector& SpringForce, float SpringForceMag, float VerticalVelocity);
	void DebugAccelerationForce(const FVector& ContactPoint, const FVector& Force);
	void DebugHandbrakeForce(float SpeedKmh, float BrakeMultiplier);
	void DebugCurrentGear();
	void UpdateVisualWheel(const FVector& ContactPoint, const FVector& UpDir, float DeltaTime);
	void SpringMotion(float Offset);

};
