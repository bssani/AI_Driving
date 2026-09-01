// Copyright Cena Abachi - Youtube: Devlogerio - devloger.io@gmail.com - Publicated on 2025 - Last update 01/2026 - All Rights Reserved
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Engine/PostProcessVolume.h" // Needed for FPostProcessSettings
#include "Vehicle.generated.h"

struct FInputActionValue;

UENUM(BlueprintType)
enum class ESteeringVisualMode : uint8
{
	CarArcade UMETA(DisplayName = "Car Arcade"),
	CarRealistic UMETA(DisplayName = "Car Realistic"),
	MotorcycleArcade UMETA(DisplayName = "Motorcycle Arcade"),
	MotorcycleRealistic UMETA(DisplayName = "Motorcycle Realistic")
};

UENUM(BlueprintType)
enum class EAIReverseFollowMode : uint8
{
	NeverReverseOutsideNav UMETA(DisplayName = "Never Reverse Outside of NavMesh"),
	ReverseTowardTarget    UMETA(DisplayName = "Can Drive in Reverse Toward Target"),
	SuperRealisticReverse  UMETA(DisplayName = "Super Realistic Reverse Behavior")
};

UENUM(BlueprintType)
enum class EAIModes : uint8
{
	FollowSpline      UMETA(DisplayName = "Follow Spline"),
	ConstantChase     UMETA(DisplayName = "Constant Chase"),
	ChaseByDistance   UMETA(DisplayName = "Chase by Distance")
};


USTRUCT(BlueprintType)
struct FDownforceElement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bIsFunctional = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bIsFunctionalInAir = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FVector LocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float Force = 100000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float MaxEffectivenessSpeed = 120.f;
};


USTRUCT(BlueprintType)
struct FCameraModeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FName ModeName = "Default";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bEnabled = true; // Camera mode is available for use

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bIsActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bCanReverseLook = false; // Camera mode is available for use

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bCanFreeLook = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float ArmLength = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FVector ArmLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FRotator ArmRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bEnableCameraLag = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float CameraLagSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bEnableCameraRotationLag = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float CameraRotationLagSpeed = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bInheritPitch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bInheritRoll = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bInheritYaw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FVector SocketOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bUsePawnControlRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bDoCollisionTest = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	TEnumAsByte<ECollisionChannel> ProbeChannel = ECC_Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float ProbeSize = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float TargetOffsetZ = 0.f; // Optional for vertical offset

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bUseFieldOfViewOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float CameraFOV = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	bool bUsePostProcessSettings = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	FPostProcessSettings PostProcessSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties")
	float PostProcessBlendWeight = 0.5f;
};

struct FInputActionInstance;

UCLASS()
class ADVANCEDCARSANDBIKESSYSTEM_API AVehicle : public APawn
{
	GENERATED_BODY()

public:
	// =========================
	// Core Overrides
	// =========================
	AVehicle();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties")
	float MinFunctionalFPS = 10.f;


	// =========================
	// Properties | Mesh & Components
	// =========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Mesh")
	UStaticMeshComponent* VehicleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|CameraRig", meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* SpringArm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|CameraRig", meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Interior", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SteeringWheel;

	// =========================
	// Properties | Engine | State
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	bool bEngineStartsOnBeginPlay = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Engine")
	bool bEngineRunning = false;

	// =========================
	// Properties | Engine | Delay
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float EngineStartDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float EngineStopDelay = 0.f;

	// =========================
	// Internal engine delay state
	// =========================
	bool bEnginePending = false;
	bool bPendingEngineState = false;
	float EngineDelayTimer = 0.f;


	// =========================
	// Properties | AI
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bIsAI = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bAIActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bActivateAIOnHit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	EAIModes AIMode = EAIModes::ConstantChase;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|AI")
	bool bAIChasing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	EAIReverseFollowMode AIReverseFollowMode = EAIReverseFollowMode::SuperRealisticReverse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float ReverseFollowDistanceM = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	FName AITargetTag = "Chased";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	FName ObstacleTag = "Avoidable";

	// === AI Obstacle Speed Control ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bAIObstacleAffectsThrottle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float ChaseStartDistanceM = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float ChaseEndDistanceM = 30000.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float ChaseReachedDistanceM = 800.f;

	// Locked chase target (do not swap once chase starts)
	UPROPERTY(Transient)
	AActor* AIChaseLockedTarget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI", meta = (ClampMin = "0.0"))
	float StartDelay = 0.f;

	// Internal delay tracking
	float AIDelayTimer = 0.f;
	bool bHasPassedStartDelay = false;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bUseAngleDampenedSteering = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIAngleDampedSteeringStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	AActor* AISplineActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIDesiredMaxSpeedKmh = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AISteeringMultiplier = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIBrakeAggressiveness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIGasAggressiveness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float BaseSteerLookAhead = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float SteerLookSpeedFactor = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float BaseBrakeLookAhead = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float BrakeLookSpeedFactor = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	int32 AIObstacleTraceCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIObstacleTraceSpread = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIObstacleTraceForwardOffset = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIObstacleTraceZOffset = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIObstacleTraceLengthMin = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIObstacleTraceLengthMax = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIAvoidanceSteeringMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bShouldStopBehindObstacles = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bAILoop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AISplineOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIResetBalanceDelayForLostBalanceOnImpact = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bResetToRoad = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI", meta = (ClampMin = "0.1"))
	float ResetToRoadDelay = 5.0f;
	
	// In your class definition:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bShouldStopWhenTooCloseToTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bAICanUseNitro = false;

	// === AI Randomization ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bRandomizeRoadOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float RandomRoadOffsetRange = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bRandomizeSpeedOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float RandomSpeedOffsetRange = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	bool bEnableRandomSteerOnReverse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float TrailerResetForwardOffset = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIStuckTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float UnstuckingMinDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float UnstuckingMaxDurationSeconds = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float RespawnImpulsePower = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI")
	float AIStuckRotationDegree = 90.f;

	FTimerHandle TimerHandle_ResetToRoad;
	float TimeInAir = 0.f;
	bool bPendingResetToRoad = false;
	bool bReachedSplineEnd = false;
	float InAirTime = 0.f;
	bool bDisableResetToSpline = false;
	bool bDisableResetToSplineCuzOfTowed = false;
	float TimeSinceLastReset;
	float TimeStuck = 0.f;
	float TimeStuckForSplineReset = 0.f;
	float TimeReversing = 0.f;
	bool bIsReversingFromStuck = false;
	float ReverseTimeDuration = 0.f; // Holds the random reverse time
	float ReverseSteerDirection = 0.f; // -1.f to 1.f

	// -------------------------
	// AI | NavMesh
	// -------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	bool bUseNavMeshSystem = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	bool bAINav_DrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	bool bInsideNavCanFollowInReverse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AI_RerouteMinDistance = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_RepathIntervalSec = 0.35f;

	// NAV FOLLOW TUNING (treat nav path like spline)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_ProjectExtentXY = 300.f; // helps when our own collision blocks projection

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_ProjectExtentZ = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_StartForwardOffsetCm = 150.f; // project start a bit ahead of our body

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_ArriveRadiusCm = 200.f; // you already have something like this, keep using yours if it exists

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_LookAheadCm = 900.f; // you already have this, keep using yours if it exists

	// "Spline-like" speed control
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_MinThrottle = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_TurnThrottleDrop = 0.70f; // how much throttle drops at full steer

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_BrakeWhenOverSpeedKmh = 6.f; // start braking if current speed is over desired by this

	// Wall / obstacle avoidance (nav walls usually have collision anyway)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	bool bAINav_AvoidWalls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_WallTraceLenCm = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_WallTraceSideOffsetCm = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_WallMinClearCm = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_WallSteerStrength = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	FVector AI_NavTargetLocation = FVector::ZeroVector;

	// Stop radius for Nav arrival (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|AI|NavMesh")
	float AINav_StopRadiusCm = 150.f;

	UPROPERTY()
	FVector AINav_FinalTarget = FVector::ZeroVector;

	// Cached nav steer point (optional but useful)
	UPROPERTY()
	FVector AINav_SteerPoint = FVector::ZeroVector;

	// Nav validity flag
	UPROPERTY()
	bool bAINavHasPath = false;


	UPROPERTY(Transient)
	TObjectPtr<class UNavigationPath> AINavPath = nullptr;

	UPROPERTY(Transient)
	int32 AINavPathIndex = 0;

	UPROPERTY(Transient)
	float AINav_RepathTimer = 0.f;
	float AI_DirectionFatigueTime = 0.f;

	float AI_ObstacleHandbrakeTime = 0.f;
	// Obstacle override of nav-steer (seconds)
	float AINav_AvoidOverrideSeconds = 0.45f;
	float AINav_AvoidOverrideRemain = 0.f;

	// Small brake/handbrake pulse when very close to an obstacle (seconds)
	float AINav_ObstacleBrakePulseSeconds = 0.18f;
	float AINav_ObstacleBrakePulseRemain = 0.f;
	// Vehicle.h (AVehicle)
	bool  bAI_ObstacleAvoidActive = false;
	float AI_ObstacleAvoidTime = 0.f;
	int32 AI_ObstacleLastDirSign = 0;     // +1 right, -1 left
	FVector AI_ObstacleLastAvoidDirWS = FVector::ZeroVector;
	bool bAINavExitedMesh = false;

	// Helpers
	bool AI_NavIsUsable() const;
	bool AI_NavBuildPathTo(const FVector& WorldTarget);
	float AI_NavWallSteerBias(const FVector& MyLoc) const;
	FVector AI_NavGetSteerPoint(const FVector& MyLoc) const;
	void AI_NavDriveToward(const FVector& SteerPoint, float DeltaTime);


	// =========================
	// Properties | Extra
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Extra")
	FName MovingPlatformTag = FName("Moving");

	// =========================
	// Properties | FX | Exhaust
	// =========================
	UPROPERTY(VisibleAnywhere, Category = "Properties|FX|Exhaust")
	USceneComponent* LeftExhaust;

	UPROPERTY(VisibleAnywhere, Category = "Properties|FX|Exhaust")
	USceneComponent* RightExhaust;

	// =========================
	// Properties | FX | Collision
	// =========================

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Collision")
	class USoundCue* CollisionSoundCue = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Collision")
	class USoundAttenuation* CollisionSoundAttenuation = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Collision")
	float CollisionSoundThreshold = 100000.f;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Collision")
	class UAudioComponent* CollisionAudio = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Collision")
	float CollisionSoundCooldown = 0.1f;

	float LastCollisionSoundTime = -100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|FX|Collision")
	class UNiagaraSystem* CollisionFX;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|FX|Collision")
	float MinCollisionFXDistance = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	FVector LastCollisionFXLocation = FVector::ZeroVector;

	// =========================
	// Properties | FX | Horn
	// =========================
	UPROPERTY(EditAnywhere, Category = "Properties|FX|Horn")
	USoundCue* HornSoundCue = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Horn")
	class USoundAttenuation* HornAttenuationSettings = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	class UAudioComponent* HornAudio = nullptr;

	// =========================
	// Properties | FX | Engine
	// =========================
	UPROPERTY(EditAnywhere, Category = "Properties|FX|Engine")
	USoundCue* EngineSoundCue;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Engine")
	USoundAttenuation* EngineAttenuationSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	UAudioComponent* EngineAudio;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Engine")
	USoundCue* EngineStartSoundCue = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Engine")
	USoundCue* EngineStopSoundCue = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Engine")
	USoundAttenuation* EngineStartStopAttenuation = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Gear")
	USoundCue* GearShiftSoundCue;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Gear")
	USoundAttenuation* GearAttenuationSettings;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Gear")
	float GearShiftVolume = 1.f;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Gear")
	float GearShiftPitch = 1.f;

	// =========================
	// Properties | FX | Nitro
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|FX|Nitro")
	USoundCue* NitroSoundCue = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|FX|Nitro")
	USoundAttenuation* NitroAttenuationSettings = nullptr;

	UPROPERTY(EditAnywhere, Category = "Properties|FX|Nitro")
	UNiagaraSystem* NitroFX = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	class UNiagaraComponent* LeftNitroFX;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	UNiagaraComponent* RightNitroFX;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	UAudioComponent* NitroAudio = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|FX|Nitro")
	float HandbrakeRevLimitAlpha = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|FX|Nitro")
	float LaunchBoostStrength = 1000000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|FX|Nitro")
	float LaunchBoostMaxSpeedKmh = 5.f;

	bool bWasRevvingWithHandbrake = false;

	// =========================
	// Properties | Engine
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float MinRPM = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float MaxRPM = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float RedLineRPM = 3900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float IdealDownshiftRPM = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float RevBoostMinRPM = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Engine")
	float RevBoostMaxRPM = 2500.f;


	float TargetRPM = 0.f;

	// =========================
	// Properties | Transmission
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Transmission")
	bool bAutomaticTransmission = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Transmission")
	bool bClutchEnabled = false;

	UPROPERTY(BlueprintReadWrite, Category = "Properties|Transmission")
	bool bClutchActive = true;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	int32 PreviousGearIndex = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	float DownshiftTargetSpeedCm = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Transmission")
	TArray<float> GearSpeedThresholdsKmh = { 60.f, 40.f, 70.f, 95.f, 110.f, 140.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Transmission")
	TArray<float> GearPushForces = { 5000.f, 5000.f, 4000.f, 3500.f, 3000.f, 2500.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Transmission")
	float MaxOverspeed = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Transmission")
	float GearOverSpeedPercent = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Transmission")
	float GearOverSpeedTorqueDropPercent = 90.f;

	// =========================
	// Properties | Drivetrain
	// =========================
	UPROPERTY(EditAnywhere, Category = "Properties|Drivetrain")
	float DownshiftBrakeStrength = 5.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Drivetrain")
	float DownshiftKickStrengthPerGear = 0.25f;

	// =========================
	// Properties | Driving
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Driving")
	float DesiredMaxSpeedForFullThrottleKmh = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Driving")
	float DonutSpeedLimit = 30.f;

	// =========================
	// Properties | Nitro System
	// =========================
	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	bool bCanNitro = true;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float NitroForce = 35000.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float MaxNitroFuel = 5.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float NitroConsumptionRate = 1.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float NitroRefillDuration = 10.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float NitroRefillDelay = 2.f;

	float NitroRefillDelayTimer = 0.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	bool bNitroAutoRefill = true;

	UPROPERTY(EditAnywhere, Category = "Properties|Nitro")
	float CurrentNitroFuel = 5.f;

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Nitro")
	bool bNitroActive = false;

	UPROPERTY(BlueprintReadWrite, Category = "Properties|Nitro")
	bool bNitroButtonHeld = false;


	// =========================
	// Properties | Steering
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	ESteeringVisualMode SteeringVisualMode = ESteeringVisualMode::CarArcade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float MaxVisualSteeringAngle = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float SteeringWheelLerpSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float RealisticSteeringVisualMultiplier = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float MotorcycleMaxSteeringAngle = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float MotorcycleSteeringLerpSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Steering")
	float RealisticMotorcycleSteeringVisualMultiplier = 2.f;

	float CurrentSteeringVisualAngle = 0.f;
	FRotator SteeringWheelInitialRotation = FRotator::ZeroRotator;

	// =========================
	// Properties | Slope Assist
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|SlopeAssist")
	bool bIsTank = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|SlopeAssist")
	bool bCanHillGrip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|SlopeAssist")
	float MaxHillGripAngle = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|SlopeAssist")
	float MaxHillGripSpeedKmh = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|SlopeAssist")
	float OnSlopeEnginePowerMultiplier = 1.f;

	// =========================
	// Properties | Balance
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bEnableFullAutoBalance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bIsMotorcycle = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bIdleLockDisablesAutoBalance = false;

	// --- AutoBalance Spawn Protection ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float AutoBalanceSpawnProtectionSeconds = 3.0f;
	float AutoBalanceSpawnTimer = 0.f;
	bool bAutoBalanceSpawnProtected = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bEnableNoseBalance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bEnableAutoBalanceLean = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bBalanceToHorizon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bRealisticHillClimb = false;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bLoseBalanceOnImpact = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float LoseBalanceImpactPower = 100000.0f; // Tune as needed

	bool bEnableFullAutoBalance_Original = true;
	bool bEnableNoseBalance_Original = true;
	bool bEnableAutoBalanceLean_Original = true;
	bool bBalanceToHorizon_Original = true;
	bool bRealisticHillClimb_Original = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float RealisticHillClimbHorizonBalanceEnterAngle = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float RealisticHillClimbHorizonBalanceExitAngle = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bShouldManualAirControls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bInvertAirRoll = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bInvertAirPitch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bShouldTiltWhileGrounded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float RollingTorqueOnGround = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float RollingTorqueOnGroundEntrySpeedKm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float RollingTorque = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float TiltingTorque = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float RollingTorqueDamping = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float TiltingTorqueDamping = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float AutoBalanceTraceLength = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float UprightCorrectionTorque = 15000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float UprightCorrectionTorqueDamping = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float LeanTorqueStrength = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float LeanSmoothingSpeed = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float SteerDeadzone = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float LookDeadzone = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	float LookSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bInvertLookY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Balance")
	bool bInvertLookX = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Balance")
	float SmoothedLeanStrength = 0.f;

	// =========================
	// Properties | Camera
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	TArray<FCameraModeConfig> CameraModes;

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Camera")
	int32 CurrentCameraIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float ReverseCameraStartingSpeed = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float InAirCameraYawFollowMultiplier = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraSensitivityYawMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraSensitivityPitchMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraMinPitch = -89.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraMaxPitch = 89.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraMinPitchInDrive = -45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraMaxPitchInDrive = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraMinYawInDrive = -45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraMaxYawInDrive = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraRotationLagSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float ArmLengthMaxLagSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float ArmLengthLagStretch = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Camera")
	float CameraResetDelay = 3.0f;

	float ReverseCameraYawOffset = 0.f;
	float ReverseCameraYawTarget = 0.f;
	float ReverseCameraInterpSpeed = 2.f;
	bool bReverseCameraActive = false;

	// =========================
	// Properties | Input | Mapping
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	class UInputMappingContext* IMC_Vehicle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	class UInputAction* IA_MoveForward = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Reverse = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Steer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Clutch = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Look = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Brake = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_HandBrake = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Nitro = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Tilt = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_CameraChange = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Reset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_GearUp = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_GearDown = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_N = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Horn = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_Hook = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Input|Mapping")
	UInputAction* IA_EngineToggle = nullptr;

	// =========================
	// Properties | Input | State
	// =========================
	UPROPERTY(BlueprintReadWrite, Category = "Properties|Input|State")
	float GasPaddle = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Properties|Input|State")
	float ForwardPaddle = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Properties|Input|State")
	float BrakePaddle = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Properties|Transmission")
	float Clutch = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Input|State")
	float SteeringInput = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Input|State")
	float TiltingInput = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Input|State")
	bool bIsHandBrake = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Input|State")
	bool bBrakeButtonPressed = false;

	// =========================
	// Properties | Stats
	// =========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Stats")
	bool bIsBraking = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Stats")
	bool bIsReversing = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Stats")
	bool bIsIdleLocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Properties|Stats")
	bool bThrottling = false;

	UPROPERTY(EditAnywhere, Category = "Properties|Stats")
	float BrakingSpeedBeforeReverse = 15.f;

	bool bIsReverseOrBraking = false;
	bool bReadyForBurnout = false;

	// =========================
	// Properties | Idle Lock
	// =========================
	UPROPERTY(EditAnywhere, Category = "Properties|IdleLock")
	float IdleConditionDelay = 1.f;

	float IdleConditionTimer = 0.f;


	// =========================
	// Properties | Physics | Mass
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Physics|Mass")
	bool DynamicCenterOfMass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Physics|Mass")
	float ActualMass = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Physics|Mass")
	FVector NormalCenterOfMass = FVector(0, 0, -0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Physics|Mass")
	FVector InAirCenterOfMass = FVector(0, 0, -50.0f);

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Physics|Mass")
	FVector CurrentCenterOfMassOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Physics|Mass")
	bool bIsInAir = false;

	// =========================
	// Properties | Trailer
	// =========================
	UPROPERTY(EditAnywhere, Category = "Properties|Trailer")
	bool CanTow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Trailer")
	AActor* TrailerReference = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Properties|Trailer")
	USceneComponent* TrailerHitchPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Trailer")
	class UPhysicsConstraintComponent* TrailerConstraint;

	UPROPERTY(VisibleAnywhere, Category = "Properties|Trailer")
	bool bTrailerAttached = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Trailer")
	bool bBeingTowed = false;

	UPROPERTY(EditAnywhere, Category = "Properties|Trailer")
	float MaxHookAttachDistance = 10.f;

	UPROPERTY(EditAnywhere, Category = "Properties|Trailer")
	float HookDetectionRadius = 50.f;

	float HookValidationDistance = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Properties|Others")
	AActor* PendingAttachActor = nullptr;

	FVector PendingAttachFrom;
	FVector PendingAttachTo;

	float AttachInterpDuration = 0.25f;
	float AttachInterpElapsed = 0.f;
	bool bIsInterpolatingAttach = false;

	// =========================
	// Properties | Downforce
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Downforce")
	TArray<FDownforceElement> DownforcePoints;

	// =========================
	// Properties | Debug
	// =========================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Debug")
	bool bShowWheelLocationOnConstruction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Debug")
	bool bShowDebugLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Debug")
	bool bShowDebugHUD = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Properties|Debug|HUD", meta = (MultiLine = "true"))
	FString KeybindingsText =
		TEXT("KEYBINDINGS     WASD: Drive  |  Space: Handbrake  |  Alt: Nitro  |  Shift/Ctrl: Air Control  |  C: Camera  |  Mouse: Look\n")
		TEXT("                Q/E: Gear Up/Down  |  T: Engine Start/Stop  |  TAB: Trailer Hook  |  M: Toggle Manual Trans  |  H: Horn\n")
		TEXT("                R: Upright Vehicle  |  Esc/0: Restart Game  |  1-9: Select Cars  |  F11: FullScreen");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	float CachedSpeedKmh = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	float CachedRPM = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	float CachedSteeringDeg = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	float CachedThrottlePercent = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	float CachedGasPaddlePercent = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	FRotator CachedGyroRotation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	float CachedSlopeAngleDeg;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	bool bIsDrifting = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	bool bIsHillGripped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Debug")
	int32 CachedGearIndex = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Properties|Debug")
	int32 CachedDriveWheelCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime|Suspension")
	float CachedDriftGrip = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime|Suspension")
	float CachedThrottleForce = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Properties|Runtime|Suspension")
	bool bCachedDriftGripIsRecovering = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties|Debug")
	int32 bCachedWheelsDriftGripRecovering = -1;

	// =========================
	// Internal Variables (no UPROPERTY)
	// =========================
	float CurrentArmLength = 0.f;
	float CameraYaw = 0.0f;
	float CameraPitch = -10.0f;
	float InitialSpringArmLength = 800.f;
	FVector InitialSpringArmLocation = FVector::ZeroVector;
	float SecondTargetArmLength = 800.f;
	FVector SecondTargetArmLocation = FVector::ZeroVector;
	float LastCameraInputTime = 0.f;
	FRotator InitialSpringArmRotation = FRotator::ZeroRotator;
	float CameraPitchInterpSpeed = 6.0f;
	bool bVehicleIsOnSlope = false;
	FVector InitialCenterOfMass;
	TArray<class USuspension*> CachedSuspensions;
	float SmoothedSteeringWheelYaw = 0.f;
	bool bHasStartedChasing = false;
	bool bIsRecoveringFromStuck = false;

	UFUNCTION(Category = "Properties|Others")
	void OnVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category = "Properties|Camera")
	void OnCameraChange();

	UFUNCTION(BlueprintCallable, Category = "Properties|Camera")
	void SetCameraModeByName(FName ModeName);

	UFUNCTION(BlueprintCallable, Category = "Properties|Camera")
	FCameraModeConfig GetActiveCameraMode();

	UFUNCTION(BlueprintCallable, Category = "Properties|Input")
	void ManualUpshift();

	UFUNCTION(BlueprintCallable, Category = "Properties|Input")
	void ManualDownshift();

	UFUNCTION(BlueprintCallable, Category = "Properties|Input")
	void Neutral();

	UFUNCTION(Category = "Properties|Others")
	void OnClutch(const FInputActionInstance& Instance);

	UFUNCTION(BlueprintCallable, Category = "Properties|Transmission")
	void StartHorn();

	UFUNCTION(BlueprintCallable, Category = "Properties|Transmission")
	void StopHorn();

	UFUNCTION(BlueprintCallable, Category = "Properties|Transmission")
	void ForceSetGear(int32 NewGearIndex);

	void ApplyDownforces();
	void PlayGearShiftSound();

	void HandleNitroSystem(float DeltaTime);
	void RefillNitro(float Amount);
	void StartNitro();
	void StopNitro();
	void UpdateEngineAudio();

	UFUNCTION(BlueprintCallable, Category = "Properties|Engine")
	void StartEngine();

	UFUNCTION(BlueprintCallable, Category = "Properties|Engine")
	void StopEngine();

	void OnEngineToggle();

	UFUNCTION(BlueprintCallable, Category = "Properties|Engine")
	void SetEngineRunning(bool bRun);

private:
	// =========================
	// Input Events
	// =========================
	void MoveForward(const FInputActionValue& Value);
	void Reverse(const FInputActionValue& Value);
	void Steer(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Tilt(const FInputActionValue& Value);
	void OnHandBrakePressed();
	void OnHandBrakeReleased();
	void OnBrakePressed();
	void OnBrakeReleased();
	//
	void CheckIfInAir();
	void UpdateCenterOfMass(float DeltaTime);
	void UpdateVehicleStats(float DeltaTime);
	void PerformVehicleAutoBalance(float DeltaTime);
	void PerformVehicleAutoBalanceBlend(float DeltaTime);
	void UpdateDriftState(float DeltaTime);
	void DrawVehicleHUD();
	void ResetBalanceFunction();
	void ResetVehiclePosition();
	void ResetVehiclePositionToSpline();
	void UpdateCameraRotation(float DeltaTime);
	void ApplyAntiSlopeSlideForce();
	void UpdateSteeringWheelVisual(float DeltaTime);
	bool IsPlayerControlledVehicle() const;
	void UpdateGForceStats(float DeltaTime);
	void DrawGForceDial();
	void CheckAndDoIdleLock(float DeltaTime);

	void ResetAIDelay();

	void ResetCameraToInitial(float DeltaTime);
	bool HasRecentCameraInput(float CurrentTime) const;

	FVector2D CombinedGVector2D = FVector2D::ZeroVector;
	FVector LastVelocityG = FVector::ZeroVector;
	float LateralG = 0.f;
	float LongitudinalG = 0.f;
	float VerticalG = 0.f;
	float LastIdleLockUpdateTime = 0.f;
	FVector LastLocationBeforeGettingLocked = FVector::ZeroVector;
	FRotator LastRotationBeforeGettingLocked = FRotator::ZeroRotator;
	float AIStuckAccum = 0.f;
	float AIUnstuckTimer = 0.f;
	float AIUnstuckSteerDir = 0.f;
	float SmoothedVolume = 1.f;
	int32 RespawnYawStep = 0;
	FVector UnstuckStartLocation = FVector::ZeroVector;
	float   UnstuckMaxDistSq = 0.f;
	bool    bForwardPhase = false;
	bool bSuppressNextEngineSound = false;
	bool bDidBeginPlayEngineStart = false;
	FRotator BalanceLockSavedRotation;
	bool bBalanceLockWasActive = false;
	FVector LastLinearVelBeforeFreeze;
	FVector LastAngularVelBeforeFreeze;

	void SimulateDrivetrain(float DeltaTime);
	virtual void OnConstruction(const FTransform& Transform) override;

	void ToggleTrailer();

	void AttachTrailer(AActor* HitActor, const FVector& FallbackImpact);

	bool HasXYZTag(AActor* Actor);

	bool ExtractXYZWorldLocation(AActor* Actor, FVector& OutWorldLocation);

	class USplineComponent* GetAISpline() const;

	//float GetMaxSteeringAngle() const;

	void UpdateAIDriving(float DeltaTime);

	void UpdateAIChasing(float DeltaTime);
	float Clamp01(float v);
	void AI_ApplyInputs(float SteerCmd, float Throttle01, float Brake01);
	float AI_ComputeSteerToPoint(const FVector& WorldPoint) const;
};
