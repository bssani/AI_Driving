// Copyright Cena Abachi - Youtube: Devlogerio - devloger.io@gmail.com - Publicated on 2025 - Last update 01/2026 - All Rights Reserved
#include "Vehicle.h"
#include "Suspension.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Scene.h" // FPostProcessSettings lives here
#include "Engine/Engine.h"
#include "Curves/CurveFloat.h"
#include "Engine/GameViewportClient.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/RecastNavMesh.h"

#include "Engine/LocalPlayer.h" // Correct for ULocalPlayer
#include "EnhancedInputSubsystems.h" // UEnhancedInputLocalPlayerSubsyste


#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"


#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"              // FOverlapResult, FHitResult, etc.
#include "Components/PrimitiveComponent.h"   // Needed for overlap actors
#include "Engine/World.h"
#include "CollisionQueryParams.h"

#include "Kismet/GameplayStatics.h"      // if you're using GetWorld()
#include "Math/UnrealMathUtility.h"      // for FMath::Abs

#include "PhysicsEngine/PhysicsConstraintComponent.h"


AVehicle::AVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;


	// === VEHICLE MESH ===
	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
	RootComponent = VehicleMesh;
	VehicleMesh->SetSimulatePhysics(true);
	VehicleMesh->SetGenerateOverlapEvents(true);
	VehicleMesh->SetEnableGravity(true);
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleMesh->SetNotifyRigidBodyCollision(true);


	// === SPRING ARM ===
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(VehicleMesh);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	SpringArm->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
	SpringArm->SocketOffset = FVector::ZeroVector;
	SpringArm->TargetArmLength = 800.f;
	SpringArm->bEnableCameraLag = false;
	SpringArm->CameraLagSpeed = 5.f;
	SpringArm->bEnableCameraRotationLag = false;
	SpringArm->CameraRotationLagSpeed = 7.f;
	SpringArm->bDoCollisionTest = true;
	SpringArm->bUsePawnControlRotation = false;

	// Disable spring arm rotating with the vehicle
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = false; // or true if you want camera behind the car


	// === CAMERA ===
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	SteeringWheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SteeringWheel"));
	SteeringWheel->SetupAttachment(VehicleMesh); // Or attach to VehicleMesh or wherever appropriate

	// Visual-only: no physics or collision
	SteeringWheel->SetSimulatePhysics(false);
	SteeringWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SteeringWheel->SetEnableGravity(false);
	SteeringWheel->SetMobility(EComponentMobility::Movable);

	// Position it roughly in front of driver (adjust as needed)
	SteeringWheel->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	SteeringWheel->SetRelativeRotation(FRotator(0.f, 0.f, 0.f)); // Make it flat like a wheel


	LeftExhaust = CreateDefaultSubobject<USceneComponent>(TEXT("LeftExhaust"));
	LeftExhaust->SetupAttachment(VehicleMesh);
	LeftExhaust->SetRelativeLocation(FVector(-120.f, -30.f, 30.f)); // Adjust as needed

	RightExhaust = CreateDefaultSubobject<USceneComponent>(TEXT("RightExhaust"));
	RightExhaust->SetupAttachment(VehicleMesh);
	RightExhaust->SetRelativeLocation(FVector(-120.f, 30.f, 30.f)); // Adjust as needed


	TrailerHitchPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TrailerHitch"));
	TrailerHitchPoint->SetupAttachment(RootComponent);
	TrailerHitchPoint->SetRelativeLocation(FVector(-200.f, 0.f, 50.f)); // Adjust as needed

	TrailerConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("TrailerConstraint"));
	TrailerConstraint->SetupAttachment(TrailerHitchPoint);


	// === Third Person View ===
	CameraModes.Add({
		"ThirdPerson",           // ModeName
		true,                    // bIsEnable
		true,                    // bIsActive
		true,                    // bCanReverseLook
		true,                    // bCanFreeLook
		800.f,                   // ArmLength
		FVector(0.f, 0.f, -30.f),// ArmLocation
		FRotator(-15.f, 0.f, 0.f),// ArmRotation
		false,                    // bEnableCameraLag
		5.f,                     // CameraLagSpeed
		false,                   // bEnableCameraRotationLag
		7.f,                     // CameraRotationLagSpeed
		false,                   // bInheritPitch
		false,                   // bInheritRoll
		false,                   // bInheritYaw
		FVector::ZeroVector,     // SocketOffset
		false,                   // bUsePawnControlRotation
		true,                    // bDoCollisionTest
		ECC_Camera,              // ProbeChannel
		12.f,                    // ProbeSize
		0.f,                     // TargetOffsetZ
		false,                   // bUseFieldOfViewOverride
		90.f                     // CameraFOV
		});

	// === Close Follow View ===
	CameraModes.Add({
		"CloseFollow",           // ModeName
		true,                    // bIsActive
		false,                   // bIsActive
		true,                    // bCanReverseLook
		true,                    // bCanFreeLook
		650.f,       // ArmLength
		FVector(-40.f, -50.f, 70.f),     // ArmLocation
		FRotator(-5.f, 0.f, 0.f),              // ArmRotation
		false,                   // bEnableCameraLag
		5.f,                     // CameraLagSpeed
		true,                    // bEnableCameraRotationLag
		7.f,                     // CameraRotationLagSpeed
		false,                   // bInheritPitch
		false,                   // bInheritRoll
		false,                   // bInheritYaw
		FVector::ZeroVector,     // SocketOffset
		false,                   // bUsePawnControlRotation
		true,                    // bDoCollisionTest
		ECC_Camera,              // ProbeChannel
		12.f,                    // ProbeSize
		0.f,                     // TargetOffsetZ
		false,                   // bUseFieldOfViewOverride
		90.f                     // CameraFOV
		});

	// === Driver Seat View ===
	CameraModes.Add({
		"Driver",                // ModeName
		true,                    // bIsActive
		false,                   // bIsActive
		false,                    // bCanReverseLook
		true,                    // bCanFreeLook
		0.f,                     // ArmLength
		FVector(-40.f, -50.f, 70.f),       // ArmLocation
		FRotator::ZeroRotator,  // ArmRotation
		false,                   // bEnableCameraLag
		10.f,                    // CameraLagSpeed
		true,                    // bEnableCameraRotationLag
		12.f,                    // CameraRotationLagSpeed
		true,                    // bInheritPitch
		true,                    // bInheritRoll
		true,                    // bInheritYaw
		FVector::ZeroVector,     // SocketOffset
		false,                   // bUsePawnControlRotation
		true,                    // bDoCollisionTest
		ECC_Camera,              // ProbeChannel
		12.f,                    // ProbeSize
		0.f,                     // TargetOffsetZ
		false,                   // bUseFieldOfViewOverride
		90.f                     // CameraFOV
		});

	// === Hood View ===
	CameraModes.Add({
		"Hood",                  // ModeName
		true,                    // bIsActive
		false,                   // bIsActive
		false,                    // bCanReverseLook
		false,                   // bCanFreeLook
		0.f,                     // ArmLength
		FVector(50.f, 0.f, 70.f),// ArmLocation
		FRotator::ZeroRotator,  // ArmRotation
		false,                   // bEnableCameraLag
		15.f,                    // CameraLagSpeed
		true,                    // bEnableCameraRotationLag
		12.f,                    // CameraRotationLagSpeed
		true,                    // bInheritPitch
		true,                    // bInheritRoll
		true,                    // bInheritYaw
		FVector::ZeroVector,     // SocketOffset
		false,                   // bUsePawnControlRotation
		true,                    // bDoCollisionTest
		ECC_Camera,              // ProbeChannel
		12.f,                    // ProbeSize
		0.f,                     // TargetOffsetZ
		false,                   // bUseFieldOfViewOverride
		90.f                     // CameraFOV
		});

	// === Front Bumper View ===
	CameraModes.Add({
		"Front",                 // ModeName
		true,                    // bIsActive
		false,                   // bIsActive
		false,                    // bCanReverseLook
		false,                   // bCanFreeLook
		0.f,                     // ArmLength
		FVector(200.f, 0.f, -20.f),        // ArmLocation
		FRotator::ZeroRotator,  // ArmRotation
		false,                   // bEnableCameraLag
		15.f,                    // CameraLagSpeed
		false,                   // bEnableCameraRotationLag
		12.f,                    // CameraRotationLagSpeed
		true,                    // bInheritPitch
		true,                    // bInheritRoll
		true,                    // bInheritYaw
		FVector::ZeroVector,     // SocketOffset
		false,                   // bUsePawnControlRotation
		true,                    // bDoCollisionTest
		ECC_Camera,              // ProbeChannel
		12.f,                    // ProbeSize
		0.f,                     // TargetOffsetZ
		false,                   // bUseFieldOfViewOverride
		90.f                     // CameraFOV
		});

	DownforcePoints.Add({
	TEXT("Center"),
	false,
	true,
	FVector::ZeroVector,
	100000.f,
	120.f
		});

}

void AVehicle::BeginPlay()
{
	Super::BeginPlay();

	if (SteeringWheel)
	{
		SteeringWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SteeringWheel->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	// === Ensure Transmission Arrays Are Same Length ===
	const int32 SpeedCount = GearSpeedThresholdsKmh.Num();
	const int32 ForceCount = GearPushForces.Num();
	AutoBalanceSpawnTimer = 0.f;
	bAutoBalanceSpawnProtected = (AutoBalanceSpawnProtectionSeconds > 0.f);



	// === Randomize AI Spline Offset ===
	if (bRandomizeRoadOffset)
	{
		AISplineOffset += FMath::RandRange(-RandomRoadOffsetRange, RandomRoadOffsetRange);
	}

	// === Randomize AI Max Speed ===
	if (bRandomizeSpeedOffset)
	{
		AIDesiredMaxSpeedKmh += FMath::RandRange(-RandomSpeedOffsetRange, RandomSpeedOffsetRange);
		AIDesiredMaxSpeedKmh = FMath::Max(10.f, AIDesiredMaxSpeedKmh); // Safety cap
	}

	if (SpeedCount != ForceCount)
	{
		if (SpeedCount > ForceCount)
		{
			// Extend GearPushForces to match
			for (int32 i = ForceCount; i < SpeedCount; ++i)
			{
				// Use last known force or fallback value
				const float LastForce = ForceCount > 0 ? GearPushForces.Last() : 1000.f;
				GearPushForces.Add(LastForce);
			}
		}
		else
		{
			// Extend GearSpeedThresholdsKmh to match
			for (int32 i = SpeedCount; i < ForceCount; ++i)
			{
				// Use last known speed or fallback value
				const float LastSpeed = SpeedCount > 0 ? GearSpeedThresholdsKmh.Last() : 80.f;
				GearSpeedThresholdsKmh.Add(LastSpeed);
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("Transmission arrays mismatched. Auto-corrected to match size (%d entries)."), GearSpeedThresholdsKmh.Num());
	}

	if (EngineSoundCue)
	{
		EngineAudio = NewObject<UAudioComponent>(this, UAudioComponent::StaticClass(), TEXT("EngineAudio"));
		EngineAudio->bAutoActivate = false;
		EngineAudio->SetSound(EngineSoundCue);
		EngineAudio->SetupAttachment(VehicleMesh);
		EngineAudio->SetRelativeLocation(FVector::ZeroVector);
		EngineAudio->bAllowSpatialization = true;
		EngineAudio->bIsUISound = false;
		EngineAudio->AttenuationSettings = EngineAttenuationSettings;
		EngineAudio->RegisterComponent();
		// DO NOT PLAY HERE
	}




	if (HornSoundCue)
	{
		HornAudio = NewObject<UAudioComponent>(this, UAudioComponent::StaticClass(), TEXT("HornAudio"));
		HornAudio->SetupAttachment(VehicleMesh);
		HornAudio->bAutoActivate = false;
		HornAudio->bIsUISound = false;
		HornAudio->bAllowSpatialization = true;
		HornAudio->SetSound(HornSoundCue);
		HornAudio->SetVolumeMultiplier(1.0f);  // Optional tuning
		HornAudio->SetPitchMultiplier(1.0f);   // Optional tuning
		HornAudio->AttenuationSettings = HornAttenuationSettings;
		HornAudio->RegisterComponent();
	}

	if (CollisionSoundCue)
	{
		CollisionAudio = NewObject<UAudioComponent>(this, UAudioComponent::StaticClass(), TEXT("CollisionAudio"));
		CollisionAudio->SetupAttachment(VehicleMesh);
		CollisionAudio->bAutoActivate = false;
		CollisionAudio->bAllowSpatialization = true;
		CollisionAudio->SetSound(CollisionSoundCue);
		if (CollisionSoundAttenuation)
		{
			CollisionAudio->AttenuationSettings = CollisionSoundAttenuation;
		}
		CollisionAudio->RegisterComponent();
	}

	// === Nitro Sound Setup ===
	if (NitroSoundCue)
	{
		NitroAudio = NewObject<UAudioComponent>(this, UAudioComponent::StaticClass(), TEXT("NitroAudio"));
		NitroAudio->SetupAttachment(VehicleMesh);
		NitroAudio->bAutoActivate = false;
		NitroAudio->SetSound(NitroSoundCue);
		NitroAudio->bAllowSpatialization = true;
		NitroAudio->AttenuationSettings = NitroAttenuationSettings;
		NitroAudio->SetVolumeMultiplier(1.0f);
		NitroAudio->RegisterComponent();
	}

	// === Nitro Particle Setup ===
	if (NitroFX)
	{
		if (LeftExhaust->IsVisible())
		{
			LeftNitroFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				NitroFX,
				LeftExhaust,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				false,
				true
			);
			LeftNitroFX->Deactivate();
		}

		if (RightExhaust->IsVisible())
		{
			RightNitroFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				NitroFX,
				RightExhaust,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				false,
				true
			);
			RightNitroFX->Deactivate();
		}
	}




	if (VehicleMesh)
	{
		VehicleMesh->OnComponentHit.AddDynamic(this, &AVehicle::OnVehicleHit);
	}

	// === [1] Cache Suspension Components ===
	GetComponents<USuspension>(CachedSuspensions);

	LastLocationBeforeGettingLocked = GetActorLocation();
	LastRotationBeforeGettingLocked = GetActorRotation();

	if (GearPushForces.Num() > 0)
	{
		for (int32 i = 0; i < GearPushForces.Num(); ++i)
		{
			GearPushForces[i] = GearPushForces[i] * 100000.f / 3600.f; // Convert from km/h² to cm/s²
		}
	}



	// === [3] Cache Initial Steering Wheel Rotation (if exists) ===
	if (SteeringWheel)
	{
		SteeringWheelInitialRotation = SteeringWheel->GetRelativeRotation();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vehicle] SteeringWheel is NULL in BeginPlay! Vehicle: %s"), *GetName());
	}

	// === [4] Initialize Camera Input Time ===
	if (GetWorld())
	{
		LastCameraInputTime = GetWorld()->GetTimeSeconds();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vehicle] GetWorld() is NULL in BeginPlay! Vehicle: %s"), *GetName());
	}

	// === [5] Input Mapping (Only for Player-Controlled Vehicles) ===
	if (GetController())
	{
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
				{
					if (IMC_Vehicle)
					{
						Subsystem->AddMappingContext(IMC_Vehicle, 0);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[Vehicle] IMC_Vehicle mapping context is NULL in BeginPlay! Vehicle: %s"), *GetName());
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[Vehicle] InputSubsystem is NULL in BeginPlay! Vehicle: %s"), *GetName());
				}
			}
		}
	}

	// === [6] Configure VehicleMesh Physics & Collision ===
	if (VehicleMesh)
	{
		VehicleMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		VehicleMesh->SetCenterOfMass(NormalCenterOfMass);
		InitialCenterOfMass = NormalCenterOfMass;
		VehicleMesh->SetMassOverrideInKg(NAME_None, ActualMass, true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Vehicle] VehicleMesh is NULL in BeginPlay! Vehicle: %s"), *GetName());
	}

	// === Backup Balance Settings ===
	bEnableFullAutoBalance_Original = bEnableFullAutoBalance;
	bEnableNoseBalance_Original = bEnableNoseBalance;
	bEnableAutoBalanceLean_Original = bEnableAutoBalanceLean;
	bBalanceToHorizon_Original = bBalanceToHorizon;
	bRealisticHillClimb_Original = bRealisticHillClimb;


	// === Auto-Attach to TrailerReference at BeginPlay ===
	if (TrailerReference && !bTrailerAttached)
	{
		const FVector HitchLocation = TrailerHitchPoint ? TrailerHitchPoint->GetComponentLocation() : GetActorLocation();
		AttachTrailer(TrailerReference, HitchLocation);

	}

	// === Activate the camera mode that is marked as active ===
	for (int32 i = 0; i < CameraModes.Num(); ++i)
	{
		if (CameraModes[i].bEnabled && CameraModes[i].bIsActive && CameraModes[i].bEnabled)
		{
			CurrentCameraIndex = i;
			SetCameraModeByName(CameraModes[i].ModeName);
			break;
		}
	}

	// =========================
	// ENGINE START POLICY (SILENT)
	// =========================
	bEngineRunning = false;
	bEnginePending = false;

	if (bEngineStartsOnBeginPlay)
	{
		bDidBeginPlayEngineStart = true;
		bSuppressNextEngineSound = true;
		SetEngineRunning(true);
	}

}

void AVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	// Ensure the vehicle mesh exists before processing logic
	if (!VehicleMesh) return;

	// =========================
	// AUTO-BALANCE SPAWN PROTECTION
	// =========================
	if (bAutoBalanceSpawnProtected)
	{
		AutoBalanceSpawnTimer += DeltaTime;

		if (AutoBalanceSpawnTimer >= AutoBalanceSpawnProtectionSeconds)
		{
			bAutoBalanceSpawnProtected = false;
		}
	}


	// =========================
	// ENGINE START / STOP DELAY
	// =========================
	if (bEnginePending)
	{
		EngineDelayTimer -= DeltaTime;
		if (EngineDelayTimer <= 0.f)
		{
			bEnginePending = false;
			bEngineRunning = bPendingEngineState;

			if (EngineAudio)
			{
				if (bEngineRunning)
				{
					if (!EngineAudio->IsPlaying())
						EngineAudio->Play();
				}
				else
				{
					EngineAudio->Stop();
				}
			}

			if (!bEngineRunning)
			{
				GasPaddle = 0.f;
				BrakePaddle = 0.f;
				bThrottling = false;
			}
		}
	}


	//if (!IsGameFocused())
	//{
	//	return; // Skip logic if alt-tabbed or not focused
	//}


	// === Smooth camera arm transitions ===
	if (!bIsAI && SpringArm)
	{
		// Interpolate spring arm position for smooth transitions
		SpringArm->SetRelativeLocation(FMath::VInterpTo(SpringArm->GetRelativeLocation(), SecondTargetArmLocation, DeltaTime, 6.f));

		// Interpolate arm length for zoom/field change
		SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, SecondTargetArmLength, DeltaTime, 4.f);
	}

	//ReverseCameraYawOffset = FMath::FInterpTo(ReverseCameraYawOffset, ReverseCameraYawTarget, DeltaTime, ReverseCameraInterpSpeed);
	// === Reverse Camera Logic ===
	if (!bIsAI && GetActiveCameraMode().bCanReverseLook)
	{
		// If vehicle is actively reversing, activate reverse camera
		if (bIsReversing && CachedSpeedKmh < -ReverseCameraStartingSpeed)
		{
			bReverseCameraActive = true;
			ReverseCameraYawTarget = 180.f;
		}
		// If vehicle starts moving forward, reset the camera
		else if (CachedSpeedKmh > ReverseCameraStartingSpeed)
		{
			bReverseCameraActive = false;
			ReverseCameraYawTarget = 0.f;
		}
		// If not reversing and not moving forward, maintain current reverse camera state
	}

	// Smooth interpolation
	ReverseCameraYawOffset = FMath::FInterpTo(ReverseCameraYawOffset, ReverseCameraYawTarget, DeltaTime, ReverseCameraInterpSpeed);


	UpdateCameraRotation(DeltaTime);     // Rotate camera based on input or auto behavior
	ResetCameraToInitial(DeltaTime);    // Return camera to center if input stops

	// ================================
	// [2] PHYSICS & STABILIZATION
	// ================================
	UpdateCenterOfMass(DeltaTime);     // Move COM based on whether airborne or grounded

	if (bEnableFullAutoBalance)
	{
		PerformVehicleAutoBalanceBlend(DeltaTime);         // Core upright auto-balance system
	}
	else {
		PerformVehicleAutoBalance(DeltaTime);         // Core upright auto-balance system
	}


	// ================================
	// [4] SLOPE SLIDE FORCE
	// ================================
	ApplyAntiSlopeSlideForce();       // Push against slope when idle to prevent rolling
	//ApplyGravityAlongSlopeForce();       // Add gravity assist force based on slope
	ApplyDownforces();


	UpdateVehicleStats(DeltaTime);     // Gather data: RPM, speed, traction, throttle
	DrawVehicleHUD();                 // Display debug info on screen (bottom-left)
	CheckAndDoIdleLock(DeltaTime);
	UpdateEngineAudio();
	HandleNitroSystem(DeltaTime);

	// ================================
	// [5] VISUALS & MISC
	// ================================
	UpdateSteeringWheelVisual(DeltaTime);

	// ================================
	// [3] DEBUGGING / STATS
	// ================================
	SimulateDrivetrain(DeltaTime);

	// ================================
	// [1] AI
	// ================================
	if (bIsAI)
	{
		// -------------------------------------------------
		// Local helpers (no new member vars)
		// -------------------------------------------------
		auto AI_ClearAllLocksAndPedals = [&]()
			{
				// Anything that can clamp drivetrain or force neutral
				bIsHandBrake = false;
				//bIsIdleLocked = false;
				//IdleConditionTimer = 0.f;

				bIsBraking = false;
				bBrakeButtonPressed = false;

				// If these exist in your class, they are the usual "car wont move" culprits
				GasPaddle = 0.f;
				BrakePaddle = 0.f;
				bThrottling = false;

				ForwardPaddle = 0.f;

				// Other common "locks"
				bIsHillGripped = false;

				// Kill any obstacle/nav override timers so nothing fights unstuck
				AINav_AvoidOverrideRemain = 0.f;
				AINav_ObstacleBrakePulseRemain = 0.f;
				AI_ObstacleHandbrakeTime = 0.f;
			};

		auto AI_NeutralInputs = [&]()
			{
				Steer(FInputActionValue(0.f));
				MoveForward(FInputActionValue(0.f));
				Reverse(FInputActionValue(0.f));
			};

		// -------------------------------------------------
		// AI OFF: hard reset + keep everything neutral
		// -------------------------------------------------
		if (!bAIActive)
		{
			ResetAIDelay();
			bHasPassedStartDelay = false;

			AI_NeutralInputs();
			AI_ClearAllLocksAndPedals();

			AIUnstuckTimer = 0.f;
			AIStuckAccum = 0.f;
			AIUnstuckSteerDir = 0.f;

			return;
		}

		// -------------------------------------------------
		// START DELAY GATE (AI does NOTHING until passed)
		// -------------------------------------------------
		if (!bHasPassedStartDelay)
		{
			if (StartDelay <= 0.f)
			{
				bHasPassedStartDelay = true;
			}
			else
			{
				AIDelayTimer += DeltaTime;
				if (AIDelayTimer >= StartDelay)
				{
					bHasPassedStartDelay = true;
				}
			}

			AI_NeutralInputs();
			AI_ClearAllLocksAndPedals();

			AIUnstuckTimer = 0.f;
			AIStuckAccum = 0.f;
			AIUnstuckSteerDir = 0.f;

			return;
		}

		// -------------------------------------------------
		// AIMode routing (decouple spline from chase)
		// -------------------------------------------------
		{
			UWorld* W = GetWorld();

			// default
			bAIChasing = false;
			bUseNavMeshSystem = false;

			if (!W)
			{
				AIChaseLockedTarget = nullptr;
				bAIChasing = false;
				bUseNavMeshSystem = false;
			}
			else if (AIMode == EAIModes::FollowSpline)
			{
				// Follow spline ONLY. Ignore chase flags completely.
				AIChaseLockedTarget = nullptr;
				bAIChasing = false;
				bUseNavMeshSystem = true; // harmless, keeps AI_NavIsUsable from early false if you reuse it elsewhere
			}
			else if (AIMode == EAIModes::ChaseByDistance)
			{
				const float StartM = FMath::Max(0.f, ChaseStartDistanceM);
				const float EndM = FMath::Max(StartM, ChaseEndDistanceM);

				const float StartCm = StartM * 100.f;
				const float EndCm = EndM * 100.f;

				if (AIChaseLockedTarget)
				{
					if (!IsValid(AIChaseLockedTarget))
					{
						AIChaseLockedTarget = nullptr;
						bAIChasing = false;
						bUseNavMeshSystem = false;
					}
					else
					{
						const float DistSq =
							FVector::DistSquared(
								AIChaseLockedTarget->GetActorLocation(),
								GetActorLocation());

						if (DistSq > FMath::Square(EndCm))
						{
							AIChaseLockedTarget = nullptr;
							bAIChasing = false;
							bUseNavMeshSystem = false;
						}
						else
						{
							bAIChasing = true;
							bUseNavMeshSystem = true;
						}
					}
				}
				else
				{
					AActor* ClosestTarget = nullptr;
					float ClosestDistSq = FLT_MAX;

					for (TActorIterator<AActor> It(W); It; ++It)
					{
						AActor* Candidate = *It;
						if (!Candidate || Candidate == this) continue;
						if (!Candidate->ActorHasTag(AITargetTag)) continue;

						const float DistSq =
							FVector::DistSquared(
								Candidate->GetActorLocation(),
								GetActorLocation());

						if (DistSq < ClosestDistSq)
						{
							ClosestDistSq = DistSq;
							ClosestTarget = Candidate;
						}
					}

					if (ClosestTarget && ClosestDistSq <= FMath::Square(StartCm))
					{
						AIChaseLockedTarget = ClosestTarget;
						bAIChasing = true;
						bUseNavMeshSystem = true;
					}
					else
					{
						bAIChasing = false;
						bUseNavMeshSystem = false;
					}
				}
			}
			else // EAIModes::ConstantChase
			{
				if (AIChaseLockedTarget)
				{
					if (!IsValid(AIChaseLockedTarget))
					{
						AIChaseLockedTarget = nullptr;
						bAIChasing = false;
						bUseNavMeshSystem = false;
					}
					else
					{
						bAIChasing = true;
						bUseNavMeshSystem = true;
					}
				}
				else
				{
					for (TActorIterator<AActor> It(W); It; ++It)
					{
						AActor* Candidate = *It;
						if (!Candidate || Candidate == this) continue;
						if (!Candidate->ActorHasTag(AITargetTag)) continue;

						AIChaseLockedTarget = Candidate;
						bAIChasing = true;
						bUseNavMeshSystem = true;
						break;
					}

					if (!AIChaseLockedTarget)
					{
						bAIChasing = false;
						bUseNavMeshSystem = false;
					}
				}
			}
		}

		// -------------------------------------------------
		// HARD STOP WHEN CHASE-BY-DISTANCE IS OUT OF RANGE
		// -------------------------------------------------
		if (AIMode == EAIModes::ChaseByDistance && !bAIChasing)
		{
			AI_ClearAllLocksAndPedals();

			Steer(FInputActionValue(0.f));

			const float Speed = CachedSpeedKmh;
			const float SpeedAbs = FMath::Abs(Speed);

			if (SpeedAbs <= 1.f)
			{
				MoveForward(FInputActionValue(0.f));
				Reverse(FInputActionValue(0.f));
				bIsHandBrake = true;
			}
			else
			{
				bIsHandBrake = false;

				if (Speed < 0.f)
				{
					MoveForward(FInputActionValue(1.f));
					Reverse(FInputActionValue(0.f));
				}
				else
				{
					MoveForward(FInputActionValue(0.f));
					Reverse(FInputActionValue(1.f));
				}
			}

			AIUnstuckTimer = 0.f;
			AIStuckAccum = 0.f;
			AIUnstuckSteerDir = 0.f;

			return;
		}

		// =====================================================
		// GLOBAL AI UNSTUCK OVERRIDE
		// Reverse -> Forward -> Respawn + 180 deg impulse
		// =====================================================
		{
			const bool bObstacleHold = (AI_ObstacleHandbrakeTime > 0.f);
			const bool bHoldingStop =
				(bReachedSplineEnd) ||
				(bIsHandBrake && !bObstacleHold);

			// -----------------------------
			// Accumulate stuck time
			// -----------------------------
			if (AIUnstuckTimer <= 0.f)
			{
				if (!bHoldingStop && FMath::Abs(CachedSpeedKmh) < 1.f)
				{
					AIStuckAccum += DeltaTime;
				}
				else
				{
					AIStuckAccum = 0.f;
				}
			}

			// -----------------------------
			// START UNSTUCK
			// -----------------------------
			if (AIUnstuckTimer <= 0.f && !bHoldingStop && AIStuckAccum >= AIStuckTime)
			{
				AIUnstuckTimer = FMath::FRandRange(
					UnstuckingMinDurationSeconds,
					UnstuckingMaxDurationSeconds
				);

				AIUnstuckSteerDir =
					(bEnableRandomSteerOnReverse && FMath::FRand() < 0.5f) ? 1.f : -1.f;

				UnstuckStartLocation = GetActorLocation();
				UnstuckMaxDistSq = 0.f;
				bForwardPhase = false;

				AIStuckAccum = -0.25f;
			}

			// -----------------------------
			// UNSTUCK ACTIVE
			// -----------------------------
			if (AIUnstuckTimer > 0.f)
			{
				AIUnstuckTimer -= DeltaTime;

				// ---- HARD UNLOCK ----
				bIsHandBrake = false;
				//bIsIdleLocked = false;
				//IdleConditionTimer = 0.f;

				bIsBraking = false;
				bBrakeButtonPressed = false;
				GasPaddle = 0.f;
				BrakePaddle = 0.f;
				bThrottling = false;
				ForwardPaddle = 0.f;

				bIsHillGripped = false;

				AINav_AvoidOverrideRemain = 0.f;
				AINav_ObstacleBrakePulseRemain = 0.f;
				AI_ObstacleHandbrakeTime = 0.f;

				// ---- INPUT ----
				Steer(FInputActionValue(AIUnstuckSteerDir));

				if (!bForwardPhase)
				{
					MoveForward(FInputActionValue(0.f));
					Reverse(FInputActionValue(1.f));
				}
				else
				{
					MoveForward(FInputActionValue(1.f));
					Reverse(FInputActionValue(0.f));
				}

				// ---- TRACK REAL MOVEMENT ----
				const float DistSq =
					FVector::DistSquared(GetActorLocation(), UnstuckStartLocation);
				UnstuckMaxDistSq = FMath::Max(UnstuckMaxDistSq, DistSq);

				// -----------------------------
				// PHASE END
				// -----------------------------
				if (AIUnstuckTimer <= 0.f)
				{
					Steer(FInputActionValue(0.f));
					MoveForward(FInputActionValue(0.f));
					Reverse(FInputActionValue(0.f));

					const float MovedDist = FMath::Sqrt(UnstuckMaxDistSq);

					// Reverse failed -> Forward
					if (!bForwardPhase && MovedDist < 75.f)
					{
						bForwardPhase = true;
						AIUnstuckTimer = FMath::FRandRange(
							UnstuckingMinDurationSeconds * 0.7f,
							UnstuckingMaxDurationSeconds
						);

						UnstuckStartLocation = GetActorLocation();
						UnstuckMaxDistSq = 0.f;
						return;
					}

					// Reverse + Forward failed -> Respawn
					if (bForwardPhase && MovedDist < 75.f)
					{
						// -------------------------------------------------
						// SPLINE-AWARE RESPAWN
						// -------------------------------------------------
						USplineComponent* AISpline = GetAISpline();

						if (AISpline)
						{
							// Reset directly to spline (authoritative)
							ResetVehiclePositionToSpline();

							// Re-fetch spline data AFTER teleport
							const FVector MyLoc = GetActorLocation();

							const float Key =
								AISpline->FindInputKeyClosestToWorldLocation(MyLoc);

							const float Dist =
								AISpline->GetDistanceAlongSplineAtSplineInputKey(Key);

							const FVector Tangent =
								AISpline->GetTangentAtDistanceAlongSpline(
									Dist,
									ESplineCoordinateSpace::World
								).GetSafeNormal();

							// Clear all velocities (safety)
							if (VehicleMesh)
							{
								VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
								VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

								// ---- IMPULSE ALONG SPLINE ----
								const FVector Impulse =
									Tangent * RespawnImpulsePower;

								VehicleMesh->AddImpulse(Impulse, NAME_None, true);
							}
						}
						else
						{
							// -------------------------------------------------
							// FALLBACK (non-spline case, preserved behavior)
							// -------------------------------------------------
							const FVector Loc = GetActorLocation();

							FHitResult Hit;
							FCollisionQueryParams Params;
							Params.AddIgnoredActor(this);

							const bool bHit =
								GetWorld()->LineTraceSingleByChannel(
									Hit,
									Loc + FVector(0.f, 0.f, 300.f),
									Loc - FVector(0.f, 0.f, 8000.f),
									ECC_Visibility,
									Params
								);

							const FVector TargetLoc =
								bHit ? (Hit.ImpactPoint + FVector(0.f, 0.f, 120.f)) : Loc;

							const FRotator Rot(
								0.f,
								GetActorRotation().Yaw + AIStuckRotationDegree,
								0.f
							);

							SetActorLocationAndRotation(
								TargetLoc,
								Rot,
								false,
								nullptr,
								ETeleportType::ResetPhysics
							);

							if (VehicleMesh)
							{
								VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
								VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

								const FVector Impulse =
									VehicleMesh->GetForwardVector() * RespawnImpulsePower;

								VehicleMesh->AddImpulse(Impulse, NAME_None, true);
							}
						}

						AIUnstuckTimer = 0.f;
						AIUnstuckSteerDir = 0.f;
						bForwardPhase = false;
					}

					AIUnstuckTimer = 0.f;
					AIUnstuckSteerDir = 0.f;
					bForwardPhase = false;
				}

				return;
			}

			// -----------------------------
			// Cooldown decay
			// -----------------------------
			if (AIStuckAccum < 0.f)
			{
				AIStuckAccum = FMath::Min(0.f, AIStuckAccum + DeltaTime);
			}
		}

	// -------------------------------------------------
	// NORMAL AI (AIMode)
	// -------------------------------------------------
		if (AIMode == EAIModes::FollowSpline)
		{
			UpdateAIDriving(DeltaTime);
		}
		else
		{
			if (bAIChasing)
			{
				UpdateAIChasing(DeltaTime);
			}
			else
			{
				UpdateAIDriving(DeltaTime);
			}
		}


	}


}

void AVehicle::CheckAndDoIdleLock(float DeltaTime)
{
	if (!VehicleMesh) return;

	// Never lock while unstucking or being towed
	if (AIUnstuckTimer > 0.f || bBeingTowed)
	{
		bIsIdleLocked = false;
		IdleConditionTimer = 0.f;
		return;
	}

	// For AI: only allow IdleLock when AI is NOT trying to move
	// (prevents fighting your AI throttle/brake/steer logic)
	if (bIsAI)
	{
		const bool bAIRequestsMove =
			(FMath::Abs(GasPaddle) > 0.01f) ||
			(FMath::Abs(BrakePaddle) > 0.01f) ||
			(FMath::Abs(ForwardPaddle) > 0.01f) ||
			(FMath::Abs(SteeringInput) > 0.05f) ||
			bThrottling ||
			bIsReverseOrBraking ||
			bIsBraking ||
			bIsReversing;

		if (bAIRequestsMove)
		{
			bIsIdleLocked = false;
			IdleConditionTimer = 0.f;
			return;
		}
	}

	// Directionless speed
	const float RawSpeedCm = VehicleMesh->GetPhysicsLinearVelocity().Size();
	const float LocalSpeedKmh = RawSpeedCm * 0.036f;

	if (bIsTank && SteeringInput > KINDA_SMALL_NUMBER)
	{
		bIsIdleLocked = false;
		IdleConditionTimer = 0.f;
		return;
	}

	bIsIdleLocked = false;

	// Immediate hard lock: handbrake + gas at near-zero speed (keeps your original behavior)
	if (FMath::Abs(LocalSpeedKmh) < 2.f && bIsHandBrake && GasPaddle > 0.f && !bIsInAir)
	{
		VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		IdleConditionTimer = 0.f;
		return;
	}

	// Update latest "lock anchor" if any meaningful movement happened
	if ((GetActorLocation() - LastLocationBeforeGettingLocked).Size() > 0.1f)
	{
		LastLocationBeforeGettingLocked = GetActorLocation();
		LastRotationBeforeGettingLocked = GetActorRotation();
	}

	// Idle conditions (handbrake not required)
	const bool bIdleConditions =
		(LocalSpeedKmh < MaxHillGripSpeedKmh) &&
		(GasPaddle == 0.f) &&
		(!bIsInAir) &&
		(!bIsDrifting);

	if (bIdleConditions)
	{
		IdleConditionTimer += DeltaTime;

		if (IdleConditionTimer >= IdleConditionDelay)
		{
			VehicleMesh->SetAllPhysicsPosition(LastLocationBeforeGettingLocked);
			VehicleMesh->SetAllPhysicsRotation(LastRotationBeforeGettingLocked);
			bIsIdleLocked = true;
		}
	}
	else
	{
		IdleConditionTimer = 0.f;
	}
}

void AVehicle::ResetAIDelay()
{
	AIDelayTimer = 0.f;
	bHasPassedStartDelay = false;
}


void AVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(IA_MoveForward, ETriggerEvent::Triggered, this, &AVehicle::MoveForward);
		EnhancedInput->BindAction(IA_MoveForward, ETriggerEvent::Completed, this, &AVehicle::MoveForward);


		EnhancedInput->BindAction(IA_Reverse, ETriggerEvent::Triggered, this, &AVehicle::Reverse);
		EnhancedInput->BindAction(IA_Reverse, ETriggerEvent::Completed, this, &AVehicle::Reverse);

		EnhancedInput->BindAction(IA_Steer, ETriggerEvent::Triggered, this, &AVehicle::Steer);
		EnhancedInput->BindAction(IA_Steer, ETriggerEvent::Completed, this, &AVehicle::Steer);


		EnhancedInput->BindAction(IA_Clutch, ETriggerEvent::Triggered, this, &AVehicle::OnClutch);
		EnhancedInput->BindAction(IA_Clutch, ETriggerEvent::Completed, this, &AVehicle::OnClutch);

		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AVehicle::Look);
		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Completed, this, &AVehicle::Look);

		EnhancedInput->BindAction(IA_HandBrake, ETriggerEvent::Started, this, &AVehicle::OnHandBrakePressed);
		EnhancedInput->BindAction(IA_HandBrake, ETriggerEvent::Completed, this, &AVehicle::OnHandBrakeReleased);

		EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Started, this, &AVehicle::OnBrakePressed);
		EnhancedInput->BindAction(IA_Brake, ETriggerEvent::Completed, this, &AVehicle::OnBrakeReleased);

		EnhancedInput->BindAction(IA_Nitro, ETriggerEvent::Triggered, this, &AVehicle::StartNitro);
		EnhancedInput->BindAction(IA_Nitro, ETriggerEvent::Completed, this, &AVehicle::StopNitro);

		EnhancedInput->BindAction(IA_N, ETriggerEvent::Started, this, &AVehicle::Neutral);
		EnhancedInput->BindAction(IA_N, ETriggerEvent::Completed, this, &AVehicle::Neutral);

		EnhancedInput->BindAction(IA_Tilt, ETriggerEvent::Triggered, this, &AVehicle::Tilt);
		EnhancedInput->BindAction(IA_Tilt, ETriggerEvent::Completed, this, &AVehicle::Tilt);

		EnhancedInput->BindAction(IA_CameraChange, ETriggerEvent::Started, this, &AVehicle::OnCameraChange);

		EnhancedInput->BindAction(IA_Reset, ETriggerEvent::Started, this, &AVehicle::ResetVehiclePosition);

		EnhancedInput->BindAction(IA_GearUp, ETriggerEvent::Completed, this, &AVehicle::ManualUpshift);
		EnhancedInput->BindAction(IA_GearDown, ETriggerEvent::Completed, this, &AVehicle::ManualDownshift);


		EnhancedInput->BindAction(IA_Horn, ETriggerEvent::Triggered, this, &AVehicle::StartHorn);
		EnhancedInput->BindAction(IA_Horn, ETriggerEvent::Completed, this, &AVehicle::StopHorn);

		EnhancedInput->BindAction(IA_Hook, ETriggerEvent::Completed, this, &AVehicle::ToggleTrailer);

		EnhancedInput->BindAction(IA_EngineToggle, ETriggerEvent::Completed, this, &AVehicle::OnEngineToggle);

	}
}

void AVehicle::StartNitro()
{
	if (!bEngineRunning) return;
	bNitroButtonHeld = true;
	if (!bCanNitro) return;
	if (CurrentNitroFuel > 0.f)
	{
		bNitroActive = true;

		if (NitroAudio && !NitroAudio->IsPlaying())
			NitroAudio->Play();

		if (NitroFX)
		{
			if (LeftExhaust->IsVisible())
			{
				if (LeftNitroFX)
				{
					LeftNitroFX->DestroyComponent();
				}
				LeftNitroFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
					NitroFX,
					LeftExhaust,
					NAME_None,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget,
					false,
					true
				);
			}

			if (RightExhaust->IsVisible())
			{
				if (RightNitroFX)
				{
					RightNitroFX->DestroyComponent();
				}
				RightNitroFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
					NitroFX,
					RightExhaust,
					NAME_None,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget,
					false,
					true
				);
			}
		}
	}
}

void AVehicle::StopNitro()
{
	bNitroButtonHeld = false;
	if (!bCanNitro) return;
	bNitroActive = false;

	if (CurrentNitroFuel < MaxNitroFuel && !bNitroButtonHeld)
	{
		NitroRefillDelayTimer = NitroRefillDelay;
	}

	if (NitroAudio && NitroAudio->IsPlaying())
		NitroAudio->Stop();

	if (LeftNitroFX)
	{
		LeftNitroFX->DeactivateImmediate();
		LeftNitroFX->DestroyComponent();
		LeftNitroFX = nullptr;
	}

	if (RightNitroFX)
	{
		RightNitroFX->DeactivateImmediate();
		RightNitroFX->DestroyComponent();
		RightNitroFX = nullptr;
	}
}

void AVehicle::RefillNitro(float Amount)
{
	if (!bCanNitro) return;
	CurrentNitroFuel = FMath::Clamp(CurrentNitroFuel + Amount, 0.f, MaxNitroFuel);
}

void AVehicle::HandleNitroSystem(float DeltaTime)
{
	if (!bCanNitro) return;

	const bool bShouldPlayFX = bNitroActive && CurrentNitroFuel > 0.f;

	// === Fuel Consumption & FX (always active if NitroActive) ===
	if (bNitroActive && CurrentNitroFuel > 0.f)
	{
		CurrentNitroFuel -= NitroConsumptionRate * DeltaTime;
		if (CurrentNitroFuel <= 0.f)
		{
			CurrentNitroFuel = 0.f;
			StopNitro();
			return;
		}
	}

	// === Apply Force Only if Grounded ===
	if (bNitroActive && !bIsInAir && VehicleMesh)
	{
		const FVector ForwardForce = VehicleMesh->GetForwardVector() * NitroForce;
		VehicleMesh->AddForce(ForwardForce);
	}

	// === Refill Logic ===
	if (!bNitroActive && !bNitroButtonHeld)
	{
		if (NitroRefillDelayTimer > 0.f)
		{
			NitroRefillDelayTimer -= DeltaTime;
		}
		else if (bNitroAutoRefill && CurrentNitroFuel < MaxNitroFuel && NitroRefillDuration > 0.f)
		{
			const float RefillRate = MaxNitroFuel / NitroRefillDuration;
			CurrentNitroFuel += RefillRate * DeltaTime;
			CurrentNitroFuel = FMath::Clamp(CurrentNitroFuel, 0.f, MaxNitroFuel);
		}
	}
}

void AVehicle::MoveForward(const FInputActionValue& Value)
{
	if (!bEngineRunning)
	{
		GasPaddle = 0.f;
		BrakePaddle = 0.f;
		bThrottling = false;
		return;
	}

	const float RawValue = Value.Get<float>();
	const float GasValue = FMath::Clamp(RawValue, 0.f, 1.f);
	ForwardPaddle = RawValue;

	const FVector Velocity = VehicleMesh->GetComponentVelocity();
	const FVector Forward = VehicleMesh->GetForwardVector();
	const float DirectionalSpeed = FVector::DotProduct(Forward, Velocity);
	const float DirectionalSpeedKmh = DirectionalSpeed * 0.036f;

	const bool bStillMovingBackwards = DirectionalSpeedKmh < -BrakingSpeedBeforeReverse;

	// === Prevent forward input override only in automatic mode ===
	if (bAutomaticTransmission && bStillMovingBackwards && !bIsDrifting)
	{
		GasPaddle = 0.f;
		BrakePaddle = GasValue;
		bIsBraking = !bBrakeButtonPressed;
		bIsReversing = false;
		bIsReverseOrBraking = true;

	}
	else
	{
		// === Normal forward throttle ===
		GasPaddle = GasValue;
		bThrottling = RawValue > KINDA_SMALL_NUMBER;

		if (!bBrakeButtonPressed)
		{
			BrakePaddle = 0.f;
			bIsBraking = false;
			bIsReversing = false;
			bIsReverseOrBraking = false;
		}
	}
}

void AVehicle::Reverse(const FInputActionValue& Value)
{
	if (!bEngineRunning)
	{
		GasPaddle = 0.f;
		BrakePaddle = 0.f;
		bIsReversing = false;
		bIsReverseOrBraking = false;
		return;
	}
	const float RawValue = Value.Get<float>();

	// === INPUT RELEASED ===
	if (RawValue <= KINDA_SMALL_NUMBER)
	{
		GasPaddle = 0.f;
		BrakePaddle = 0.f;

		if (!bBrakeButtonPressed)
		{
			bIsReversing = false;
			bIsReverseOrBraking = false;
			bIsBraking = false;
		}

		return;
	}

	const float GasValue = FMath::Clamp(RawValue, 0.f, 1.f);
	GasPaddle = GasValue;
	bIsReverseOrBraking = true;

	const FVector Velocity = VehicleMesh->GetComponentVelocity();
	const FVector Forward = VehicleMesh->GetForwardVector();
	const float DirectionalSpeed = FVector::DotProduct(Forward, Velocity);
	const float DirectionalSpeedKmh = DirectionalSpeed * 0.036f;
	const bool bStillMovingForward = DirectionalSpeedKmh > BrakingSpeedBeforeReverse;

	// === BLOCK GEAR SHIFT TO REVERSE if forward gas is held ===
	if (bAutomaticTransmission && ForwardPaddle > KINDA_SMALL_NUMBER)
	{
		// Treat Reverse as brake only
		bIsReversing = false;
		bIsBraking = !bBrakeButtonPressed;
		bIsReverseOrBraking = true;
		BrakePaddle = GasValue;

		return;
	}

	if (bStillMovingForward)
	{
		bIsReversing = false;
		bIsBraking = !bBrakeButtonPressed;
		bIsReverseOrBraking = true;
		BrakePaddle = GasValue;
	}
	else
	{
		// === Dual behavior: reverse only in automatic ===
		if (bAutomaticTransmission)
		{
			CachedGearIndex = 0;
			bIsReversing = true;

			if (!bBrakeButtonPressed)
			{
				bIsBraking = false;
				bIsReverseOrBraking = false;
			}

			BrakePaddle = 0.f;
		}
		else
		{
			bIsReversing = false;
			bIsBraking = !bBrakeButtonPressed;
			bIsReverseOrBraking = true;
			BrakePaddle = GasValue;
		}
	}
}

void AVehicle::StartEngine()
{
	SetEngineRunning(true);
}

void AVehicle::StopEngine()
{
	SetEngineRunning(false);
}

void AVehicle::OnEngineToggle()
{
	if (bIsAI) return;

	// If we're in the middle of starting, ignore toggles entirely
	if (bEnginePending && bPendingEngineState)
	{
		return;
	}

	SetEngineRunning(!bEngineRunning);
}

void AVehicle::SetEngineRunning(bool bRun)
{
	// --------------------------------
	// If we are currently starting (pending ON),
	// IGNORE STOP so start/stop never overlaps.
	// --------------------------------
	if (!bRun && bEnginePending && bPendingEngineState)
	{
		return;
	}

	// --------------------------------
	// Ignore duplicate requests
	// --------------------------------
	if ((bEngineRunning == bRun && !bEnginePending) ||
		(bEnginePending && bPendingEngineState == bRun))
	{
		return;
	}

	// --------------------------------
	// STOP (immediate) - but only if NOT blocked above
	// --------------------------------
	if (!bRun)
	{
		// cancel any pending transition (pending stop etc.)
		bEnginePending = false;
		bPendingEngineState = false;
		EngineDelayTimer = 0.f;

		if (!bSuppressNextEngineSound)
		{
			if (EngineStopSoundCue)
			{
				UGameplayStatics::PlaySoundAtLocation(
					this,
					EngineStopSoundCue,
					GetActorLocation(),
					1.f,
					1.f,
					0.f,
					EngineAttenuationSettings
				);
			}
		}
		bSuppressNextEngineSound = false;

		bEngineRunning = false;

		if (EngineAudio)
		{
			EngineAudio->Stop();
		}

		GasPaddle = 0.f;
		BrakePaddle = 0.f;
		bThrottling = false;

		return;
	}

	// --------------------------------
	// START (BeginPlay must be immediate)
	// --------------------------------
	float Delay = EngineStartDelay;

	// Only bypass delay for the ONE BeginPlay-triggered start
	if (bDidBeginPlayEngineStart)
	{
		Delay = 0.f;
		bDidBeginPlayEngineStart = false; // consume the flag
	}

	if (!bSuppressNextEngineSound)
	{
		if (EngineStartSoundCue)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				EngineStartSoundCue,
				GetActorLocation(),
				1.f,
				1.f,
				0.f,
				EngineAttenuationSettings
			);
		}
	}
	bSuppressNextEngineSound = false;

	// Instant start
	if (Delay <= 0.f)
	{
		bEngineRunning = true;
		bEnginePending = false;
		bPendingEngineState = true;
		EngineDelayTimer = 0.f;

		if (EngineAudio && !EngineAudio->IsPlaying())
		{
			EngineAudio->Play();
		}
		return;
	}

	// Delayed start
	bEnginePending = true;
	bPendingEngineState = true;
	EngineDelayTimer = Delay;
}

void AVehicle::Steer(const FInputActionValue& Value)
{
	const float Raw = Value.Get<float>();

	const float Filtered =
		FMath::Abs(Raw) < SteerDeadzone ? 0.f : Raw;

	SteeringInput = FMath::Clamp(Filtered, -1.f, 1.f);
}

void AVehicle::Look(const FInputActionValue& Value)
{
	if (!GetActiveCameraMode().bCanFreeLook) return;

	const FVector2D Raw = Value.Get<FVector2D>();
	FVector2D Filtered = Raw; // <-- THIS LINE WAS MISSING

	if (FMath::Abs(Filtered.X) < LookDeadzone) Filtered.X = 0.f;
	if (FMath::Abs(Filtered.Y) < LookDeadzone) Filtered.Y = 0.f;

	// Always count any tiny mouse movement as input (prevents "need 5cm" / blocky feel)
	if (GetWorld())
	{
		if (Filtered.X != 0.f || Filtered.Y != 0.f)
		{
			LastCameraInputTime = GetWorld()->GetTimeSeconds();
		}
	}

	// IMPORTANT: no deadzone for mouse look (deadzone makes small deltas become 0)
	float X = Filtered.X;
	float Y = Filtered.Y;

	if (X == 0.f && Y == 0.f) return;

	const float YawMul = CameraSensitivityYawMultiplier * LookSensitivity;
	const float PitchMul = CameraSensitivityPitchMultiplier * LookSensitivity;

	const float YawInput = bInvertLookX ? -X : X;
	const float PitchInput = bInvertLookY ? -Y : Y;

	CameraYaw += YawInput * YawMul;

	float MinPitch = CameraMinPitch;
	float MaxPitch = CameraMaxPitch;

	if (GetActiveCameraMode().ModeName == "Driver")
	{
		CameraYaw = FMath::Clamp(CameraYaw, CameraMinYawInDrive, CameraMaxYawInDrive);
		MinPitch = CameraMinPitchInDrive;
		MaxPitch = CameraMaxPitchInDrive;
	}

	CameraPitch = FMath::Clamp(CameraPitch + (PitchInput * PitchMul), MinPitch, MaxPitch);
}

void AVehicle::OnHandBrakePressed()
{
	bIsHandBrake = true;
}

void AVehicle::OnHandBrakeReleased()
{
	bIsHandBrake = false;
}

void AVehicle::OnBrakePressed()
{
	bBrakeButtonPressed = true;
	bIsReverseOrBraking = true;
	bIsBraking = true;
	bIsReversing = true;
}

void AVehicle::OnBrakeReleased()
{
	bBrakeButtonPressed = false;
	bIsReverseOrBraking = false;
	bIsBraking = false;
	bIsReversing = false;
}

void AVehicle::Tilt(const FInputActionValue& Value)
{
	TiltingInput = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f);
}

void AVehicle::CheckIfInAir()
{
	// Assume vehicle is in air until we find a grounded wheel
	bIsInAir = true;

	// Check all suspensions to see if any wheel is touching ground
	for (USuspension* Suspension : CachedSuspensions)
	{
		if (Suspension && Suspension->bWheelTouchingGround)
		{
			bIsInAir = false;
			break; // Exit early once grounded wheel is found
		}
	}
}

void AVehicle::UpdateCenterOfMass(float DeltaTime)
{
	if (!DynamicCenterOfMass) return;

	// Choose target center of mass offset based on grounded state
	const FVector TargetOffset = bIsInAir ? InAirCenterOfMass : InitialCenterOfMass;

	// Interpolate toward the target center of mass for smooth transition
	CurrentCenterOfMassOffset = FMath::VInterpTo(CurrentCenterOfMassOffset, TargetOffset, DeltaTime, 3.0f);

	// Apply the new center of mass to the physics body
	VehicleMesh->SetCenterOfMass(CurrentCenterOfMassOffset);

	// Optional debug visualization
	if (bShowDebugLines)
	{
		const FVector CoMWorld = VehicleMesh->GetComponentTransform().TransformPosition(CurrentCenterOfMassOffset);
		DrawDebugSphere(GetWorld(), CoMWorld, 12.f, 12, FColor::Cyan, false, -1.f, 0, 2.f);
	}
}

void AVehicle::UpdateVehicleStats(float DeltaTime)
{
	if (CachedSuspensions.Num() == 0) return;

	CheckIfInAir();

	// === Stat Accumulators ===
	float TotalWheelRPM = 0.f, TotalSteer = 0.f; //TotalSpeed = 0.f;
	float TotalEnginePower = 0.f, TotalMaxSpeed = 0.f;
	int32 DriveWheelCount = 0, Count = 0;
	int32 SlopeWheelCount = 0;

	// === Grip State Cache ===
	CachedDriftGrip = 1.f;
	bCachedDriftGripIsRecovering = false;
	bCachedWheelsDriftGripRecovering = 0;


	bool bFoundFirstDriveWheel = false;

	//float TotalRPM = 0.f;
	for (USuspension* Suspension : CachedSuspensions)
	{
		if (!Suspension) continue;

		// Stat Accumulation
		if (Suspension->bIsSteeringWheel)
		{
			TotalSteer += Suspension->CachedSteeringAngle;
		}
		//TotalSpeed += FMath::Abs(Suspension->CachedSpeedKmh);


		if (Suspension->bIsDriveWheel)
		{
			TotalWheelRPM += Suspension->CachedWheelRPM;
			TotalMaxSpeed += DesiredMaxSpeedForFullThrottleKmh;
			DriveWheelCount++;

			if (!bFoundFirstDriveWheel)
			{
				CachedDriftGrip = Suspension->CurrentGrip;
				bFoundFirstDriveWheel = true;
			}

			if (Suspension->bIsRecoveringGrip)
			{
				bCachedWheelsDriftGripRecovering++;
			}

		}

		if (Suspension->bIsOnSlope)
			SlopeWheelCount++;

		Count++;
	}

	// === Grip Recovery Final State ===
	bCachedDriftGripIsRecovering = bCachedWheelsDriftGripRecovering > 0;

	// === Final Averages ===
	if (Count > 0)
	{
		CachedSteeringDeg = TotalSteer / Count;
		//CachedSpeedKmh = TotalSpeed / Count;
	}

	CachedGasPaddlePercent = FMath::Abs(GasPaddle) * 100.f;
	//CachedGearIndex = HighestGearIndex;
	CachedDriveWheelCount = DriveWheelCount;
	bVehicleIsOnSlope = (SlopeWheelCount == Count);

	// === Directional Speed & Braking ===
	const FVector Velocity = VehicleMesh->GetComponentVelocity();
	const FVector Forward = VehicleMesh->GetForwardVector();
	const float DirectionalSpeed = FVector::DotProduct(Velocity, Forward);
	const float DirectionalSpeedKmh = DirectionalSpeed * 0.036f;


	// === RPM Simulation ===// === Simple RPM from Wheel RPMs ===
	CachedSpeedKmh = DirectionalSpeedKmh; // convert to km/h

	bReadyForBurnout = false;
	if (!bIsTank && bIsReverseOrBraking && bThrottling)
	{
		bReadyForBurnout = true;
	}



	// === Gyroscope ===
	UpdateGForceStats(DeltaTime);
	const FVector WorldUp = FVector::UpVector;
	const float PitchAngle = FMath::RadiansToDegrees(FMath::Asin(FVector::DotProduct(VehicleMesh->GetForwardVector(), WorldUp)));
	const float RollAngle = FMath::RadiansToDegrees(FMath::Asin(FVector::DotProduct(VehicleMesh->GetRightVector(), WorldUp)));

	CachedGyroRotation.Pitch = PitchAngle;
	CachedGyroRotation.Roll = RollAngle;
	CachedGyroRotation.Yaw = 0.f;
	UpdateDriftState(DeltaTime);

}

void AVehicle::UpdateDriftState(float DeltaTime)
{
	if (bCachedDriftGripIsRecovering)
	{
		bIsDrifting = true;
		return;
	}

	const FVector Velocity = VehicleMesh->GetComponentVelocity();
	const float Speed = Velocity.Size();

	if (Speed > 300.f) // Only consider if moving fast enough
	{
		const FVector Forward = VehicleMesh->GetForwardVector();
		const FVector VelocityDir = Velocity.GetSafeNormal();

		const float Dot = FVector::DotProduct(Forward, VelocityDir);
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));

		const float DriftAngleThreshold = 15.f; // Tune this
		bIsDrifting = Angle > DriftAngleThreshold;
	}
	else
	{
		bIsDrifting = false;
	}
}

void AVehicle::DrawVehicleHUD()
{
	if (!bShowDebugHUD || bIsAI || !IsPlayerControlledVehicle() || !Controller->IsPlayerController()) return;

	// === ENGINE STATE (FIRST LINE) ===
	GEngine->AddOnScreenDebugMessage(
		2000,
		0.f,
		bEngineRunning ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("Engine: %s"), bEngineRunning ? TEXT("ON") : TEXT("OFF"))
	);

	GEngine->AddOnScreenDebugMessage(2001, 0.f, FColor::Black, FString::Printf(TEXT("Speed: %.1f km/h"), CachedSpeedKmh));
	const bool bShowMinRPM = bIsMotorcycle && CachedGearIndex == 0;
	const float DisplayRPM = bShowMinRPM ? MinRPM : CachedRPM;

	GEngine->AddOnScreenDebugMessage(2002, 0.f, FColor::White, FString::Printf(TEXT("RPM: %.0f"), DisplayRPM));

	FString GearLabel = TEXT("?");
	if (CachedGearIndex == -1)
		GearLabel = TEXT("N");
	else if (CachedGearIndex == 0)
		GearLabel = TEXT("R");
	else
		GearLabel = FString::FromInt(CachedGearIndex);

	GEngine->AddOnScreenDebugMessage(2003, 0.f, FColor::Magenta, FString::Printf(TEXT("Gear: %s"), *GearLabel));
	GEngine->AddOnScreenDebugMessage(2004, 0.f, FColor::Turquoise, FString::Printf(TEXT("Gas Paddle: %.0f%%"), CachedGasPaddlePercent));
	GEngine->AddOnScreenDebugMessage(2011, 0.f, FColor::Emerald, FString::Printf(TEXT("Transmission: %s"), bAutomaticTransmission ? TEXT("Automatic") : TEXT("Manual")));
	GEngine->AddOnScreenDebugMessage(2005, 0.f, FColor::Yellow, FString::Printf(TEXT("Steering: %.1f°"), CachedSteeringDeg));
	GEngine->AddOnScreenDebugMessage(2006, 0.f, FColor::Green, FString::Printf(TEXT("Drive Wheels: %d"), CachedDriveWheelCount));
	GEngine->AddOnScreenDebugMessage(2007, 0.f, FColor::Blue, FString::Printf(TEXT("Braking: %s"), bIsBraking ? TEXT("YES") : TEXT("NO")));
	GEngine->AddOnScreenDebugMessage(2008, 0.f, FColor::Red, FString::Printf(TEXT("Drifting: %s"), bIsDrifting ? TEXT("YES") : TEXT("NO")));
	GEngine->AddOnScreenDebugMessage(2009, 0.f, FColor::Cyan, FString::Printf(TEXT("Hill Grip: %s"), bIsHillGripped ? TEXT("ACTIVE") : TEXT("NO")));
	GEngine->AddOnScreenDebugMessage(2010, 0.f, FColor::Orange, FString::Printf(TEXT("Handbrake: %s"), bIsHandBrake ? TEXT("YES") : TEXT("NO")));
	GEngine->AddOnScreenDebugMessage(2013, 0.f, FColor::Purple, FString::Printf(TEXT("Trailer: %s"), bTrailerAttached ? TEXT("Attached") : TEXT("Not Attached")));
	GEngine->AddOnScreenDebugMessage(2012, 0.f, FColor::Purple, FString::Printf(TEXT("Vehicle Angle [Tilt: %.1f°, Lean: %.1f°]"), CachedGyroRotation.Pitch, CachedGyroRotation.Roll));
	GEngine->AddOnScreenDebugMessage(2022, 0.f, FColor::Yellow, FString::Printf(TEXT("G-Accel: Fwd %.2fG | Lat %.2fG | Vert %.2fG"), LongitudinalG, LateralG, VerticalG));
	GEngine->AddOnScreenDebugMessage(2023, 0.f, bCachedDriftGripIsRecovering ? FColor::Red : FColor::Green,
		FString::Printf(TEXT("Grip: %.2f | %s"), CachedDriftGrip,
			bCachedDriftGripIsRecovering ? *FString::Printf(TEXT("%d Wheels Recovering"), bCachedWheelsDriftGripRecovering) : TEXT("Wheels are Stable")));
	GEngine->AddOnScreenDebugMessage(2031, 0.f, bNitroActive ? FColor::Cyan : FColor::Silver,
		FString::Printf(TEXT("Nitro: %.2f / %.2f %s"), CurrentNitroFuel, MaxNitroFuel,
			bNitroActive ? TEXT("[ACTIVE]") : TEXT("")));

	DrawGForceDial();

	if (!KeybindingsText.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(
			9999,
			0.f,
			FColor::White,
			KeybindingsText,
			true
		);
	}

}

void AVehicle::PerformVehicleAutoBalance(float DeltaTime)
{
	if (!VehicleMesh) return;
	if (bBeingTowed) return;

	const bool bAllowAutoBalance = bEnableAutoBalanceLean;
	const bool bAllowNoseBalance = bEnableNoseBalance;

	const FVector Up = VehicleMesh->GetUpVector();
	const FVector Fwd = VehicleMesh->GetForwardVector();
	const FVector Right = VehicleMesh->GetRightVector();
	const FVector Down = -Up;
	const FVector WorldUp = FVector::UpVector;
	const FVector VehicleLocation = VehicleMesh->GetComponentLocation();


	// =========================
	// BALANCE LOCK (FINAL)
	// =========================
	if (bIdleLockDisablesAutoBalance && bIsIdleLocked)
		return;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = false;
	if (UWorld* W = GetWorld())
	{
		bHit = W->LineTraceSingleByChannel(
			Hit,
			VehicleLocation,
			VehicleLocation + Down * AutoBalanceTraceLength,
			ECC_Visibility,
			Params
		);
	}

	if (bShowDebugLines && GetWorld())
	{
		DrawDebugLine(GetWorld(), VehicleLocation, VehicleLocation + Down * AutoBalanceTraceLength, FColor::Blue, false, 0.f, 0, 2.f);
		if (bHit) DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.f, FColor::Red, false, 0.f);
	}

	// === AIRBORNE DAMPING + CONTROLS ===
	if (bIsInAir)
	{
		FVector AirTorque = FVector::ZeroVector;

		if (bShouldManualAirControls)
		{
			const float RollSign = bInvertAirRoll ? -1.f : 1.f;
			const float PitchSign = bInvertAirPitch ? -1.f : 1.f;

			AirTorque += -Fwd * (SteeringInput * RollSign) * RollingTorque;
			AirTorque += Right * (TiltingInput * PitchSign) * TiltingTorque;
		}

		const FVector AngVel = VehicleMesh->GetPhysicsAngularVelocityInRadians();
		const float RollVel = FVector::DotProduct(AngVel, Fwd);
		const float PitchVel = FVector::DotProduct(AngVel, Right);

		AirTorque += -Fwd * RollVel * RollingTorqueDamping;
		AirTorque += -Right * PitchVel * TiltingTorqueDamping;

		if (!AirTorque.IsNearlyZero())
		{
			VehicleMesh->AddTorqueInRadians(AirTorque * DeltaTime, NAME_None, true);
		}
		return;
	}

	if (!bAllowAutoBalance)
	{
		return;
	}

	// === GROUND BALANCE ===
	FVector UprightTorque = FVector::ZeroVector;

	bool bUseHorizon = bBalanceToHorizon;

	if (bRealisticHillClimb && bHit)
	{
		if (CachedGyroRotation.Pitch > RealisticHillClimbHorizonBalanceEnterAngle &&
			CachedGyroRotation.Pitch < RealisticHillClimbHorizonBalanceExitAngle)
		{
			bUseHorizon = true;
		}
	}

	if (bUseHorizon && bHit)
	{
		const FVector UpProj = FVector::VectorPlaneProject(WorldUp, Fwd).GetSafeNormal();
		const FVector TiltAxis = FVector::CrossProduct(Up, UpProj).GetSafeNormal();
		const float RollAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, UpProj), -1.f, 1.f));
		UprightTorque = TiltAxis * (RollAngle / (PI / 2.f)) * UprightCorrectionTorque;
	}
	else if (bEnableAutoBalanceLean && bHit)
	{
		const FVector GroundUpProj = FVector::VectorPlaneProject(Hit.ImpactNormal, Fwd).GetSafeNormal();
		const FVector TiltAxis = FVector::CrossProduct(Up, GroundUpProj).GetSafeNormal();
		const float RollAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, GroundUpProj), -1.f, 1.f));
		UprightTorque = TiltAxis * (RollAngle / (PI / 2.f)) * UprightCorrectionTorque;
	}

	// === LEAN + DAMPING ===
	const float TargetLean =
		FMath::Clamp(CachedSpeedKmh / 100.f, 0.f, 1.f) * FMath::Clamp(SteeringInput, -1.f, 1.f);

	SmoothedLeanStrength = FMath::FInterpTo(SmoothedLeanStrength, TargetLean, DeltaTime, LeanSmoothingSpeed);
	const FVector LeanTorque = Fwd * -SmoothedLeanStrength * LeanTorqueStrength;

	const FVector AngVel = VehicleMesh->GetPhysicsAngularVelocityInRadians();
	const float RollVel = FVector::DotProduct(AngVel, Fwd);
	const FVector DampingTorque = -Fwd * RollVel * UprightCorrectionTorqueDamping;

	FVector TiltTorque = FVector::ZeroVector;
	if (bShouldTiltWhileGrounded)
	{
		TiltTorque = Right * -TiltingInput * RollingTorqueOnGround;
	}

	FVector FinalTorque = UprightTorque + LeanTorque + DampingTorque + TiltTorque;

	// === Optional Nose Correction ===
	if (bAllowNoseBalance)
	{
		const FVector PitchUpProj = FVector::VectorPlaneProject(WorldUp, Right).GetSafeNormal();
		const FVector PitchTiltAxis = FVector::CrossProduct(Up, PitchUpProj).GetSafeNormal();
		const float PitchAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, PitchUpProj), -1.f, 1.f));
		const FVector NoseTorque = PitchTiltAxis * (PitchAngle / (PI / 2.f)) * UprightCorrectionTorque;

		const float PitchVel = FVector::DotProduct(AngVel, Right);
		const FVector PitchDamping = -Right * PitchVel * UprightCorrectionTorqueDamping;

		FinalTorque += NoseTorque + PitchDamping;
	}

	VehicleMesh->AddTorqueInRadians(FinalTorque * DeltaTime, NAME_None, true);
}

void AVehicle::PerformVehicleAutoBalanceBlend(float DeltaTime)
{
	if (!VehicleMesh) return;
	if (bBeingTowed) return;
	if (!bEnableAutoBalanceLean || !bEnableFullAutoBalance) return;

	const FVector Up = VehicleMesh->GetUpVector();
	const FVector Fwd = VehicleMesh->GetForwardVector();
	const FVector Right = VehicleMesh->GetRightVector();
	const FVector WorldUp = FVector::UpVector;
	const FVector VehicleLocation = VehicleMesh->GetComponentLocation();

	// =========================
	// BALANCE LOCK (FINAL)
	// =========================
	if (bIdleLockDisablesAutoBalance && bIsIdleLocked)
		return;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = false;
	if (UWorld* W = GetWorld())
	{
		bHit = W->LineTraceSingleByChannel(
			Hit,
			VehicleLocation,
			VehicleLocation - Up * AutoBalanceTraceLength,
			ECC_Visibility,
			Params
		);
	}

	if (bShowDebugLines && GetWorld())
	{
		DrawDebugLine(GetWorld(), VehicleLocation, VehicleLocation - Up * AutoBalanceTraceLength, FColor::Purple, false, 0.f, 0, 2.f);
		if (bHit) DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.f, FColor::Magenta, false, 0.f);
	}

	if (bIsInAir)
	{
		FVector AirTorque = FVector::ZeroVector;

		if (bShouldManualAirControls)
		{
			const float RollSign = bInvertAirRoll ? -1.f : 1.f;
			const float PitchSign = bInvertAirPitch ? -1.f : 1.f;

			AirTorque += -Fwd * (SteeringInput * RollSign) * RollingTorque;
			AirTorque += Right * (TiltingInput * PitchSign) * TiltingTorque;
		}

		const FVector AngVel = VehicleMesh->GetPhysicsAngularVelocityInRadians();
		AirTorque += -Fwd * FVector::DotProduct(AngVel, Fwd) * RollingTorqueDamping;
		AirTorque += -Right * FVector::DotProduct(AngVel, Right) * TiltingTorqueDamping;

		if (!AirTorque.IsNearlyZero())
		{
			VehicleMesh->AddTorqueInRadians(AirTorque * DeltaTime, NAME_None, true);
		}
		return;
	}

	if (!bHit) return;

	// === BLENDED GROUND BALANCE ===
	FVector SurfaceTorque = FVector::ZeroVector;
	{
		const FVector SurfaceUpProj = FVector::VectorPlaneProject(Hit.ImpactNormal, Fwd).GetSafeNormal();
		const FVector SurfaceTiltAxis = FVector::CrossProduct(Up, SurfaceUpProj).GetSafeNormal();
		const float SurfaceRollAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, SurfaceUpProj), -1.f, 1.f));
		SurfaceTorque = SurfaceTiltAxis * (SurfaceRollAngle / (PI / 2.f));
	}

	FVector HorizonTorque = FVector::ZeroVector;
	if (CachedGyroRotation.Pitch < RealisticHillClimbHorizonBalanceExitAngle)
	{
		const FVector HorizonUpProj = FVector::VectorPlaneProject(WorldUp, Fwd).GetSafeNormal();
		const FVector HorizonTiltAxis = FVector::CrossProduct(Up, HorizonUpProj).GetSafeNormal();
		const float HorizonRollAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, HorizonUpProj), -1.f, 1.f));
		HorizonTorque = HorizonTiltAxis * (HorizonRollAngle / (PI / 2.f));
	}

	const FVector UprightTorque = (SurfaceTorque + HorizonTorque) * 0.5f * UprightCorrectionTorque;

	// === LEAN + DAMPING ===
	const float TargetLean =
		FMath::Clamp(CachedSpeedKmh / 100.f, 0.f, 1.f) * FMath::Clamp(SteeringInput, -1.f, 1.f);

	SmoothedLeanStrength = FMath::FInterpTo(SmoothedLeanStrength, TargetLean, DeltaTime, LeanSmoothingSpeed);
	const FVector LeanTorque = Fwd * -SmoothedLeanStrength * LeanTorqueStrength;

	const FVector AngVel = VehicleMesh->GetPhysicsAngularVelocityInRadians();
	const float RollVel = FVector::DotProduct(AngVel, Fwd);
	const FVector DampingTorque = -Fwd * RollVel * UprightCorrectionTorqueDamping;

	FVector TiltTorque = FVector::ZeroVector;
	if (bShouldTiltWhileGrounded && CachedSpeedKmh >= RollingTorqueOnGroundEntrySpeedKm)
	{
		TiltTorque = Right * -TiltingInput * RollingTorqueOnGround;
	}

	FVector FinalTorque = UprightTorque + LeanTorque + DampingTorque + TiltTorque;

	// === Optional Nose Balance ===
	if (bEnableNoseBalance)
	{
		const FVector PitchUpProj = FVector::VectorPlaneProject(WorldUp, Right).GetSafeNormal();
		const FVector PitchTiltAxis = FVector::CrossProduct(Up, PitchUpProj).GetSafeNormal();
		const float PitchAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, PitchUpProj), -1.f, 1.f));
		const FVector NoseTorque = PitchTiltAxis * (PitchAngle / (PI / 2.f)) * UprightCorrectionTorque;

		const float PitchVel = FVector::DotProduct(AngVel, Right);
		const FVector PitchDamping = -Right * PitchVel * UprightCorrectionTorqueDamping;

		FinalTorque += NoseTorque + PitchDamping;
	}

	VehicleMesh->AddTorqueInRadians(FinalTorque * DeltaTime, NAME_None, true);
}

void AVehicle::ResetBalanceFunction()
{
	bEnableFullAutoBalance = bEnableFullAutoBalance_Original;
	bEnableNoseBalance = bEnableNoseBalance_Original;
	bEnableAutoBalanceLean = bEnableAutoBalanceLean_Original;
	bBalanceToHorizon = bBalanceToHorizon_Original;
	bRealisticHillClimb = bRealisticHillClimb_Original;
}

void AVehicle::ResetVehiclePosition()
{
	if (!VehicleMesh) return;
	ResetBalanceFunction();

	// Step 2: Get current location
	FVector CurrentLocation = GetActorLocation();

	// Step 3: Perform a downward trace to find the ground
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const FVector Start = CurrentLocation + FVector(0.f, 0.f, 100.f);
	const FVector End = CurrentLocation - FVector(0.f, 0.f, 5000.f); // trace far downward

	bool bHitGround = false;
	bHitGround = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	// Step 4: If hit, move the vehicle to that location and align upright
	FVector TargetLocation = bHitGround ? Hit.ImpactPoint + FVector(0.f, 0.f, 100.f) : CurrentLocation;
	FRotator UprightRotation = FRotator(0.f, GetActorRotation().Yaw, 0.f); // Maintain yaw

	// Step 5: Teleport and reset physics
	SetActorLocationAndRotation(TargetLocation, UprightRotation, false, nullptr, ETeleportType::ResetPhysics);

	// Step 6: Stop all motion
	VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);


	bPendingResetToRoad = false;

}

void AVehicle::UpdateAIDriving(float DeltaTime)
{
	if (!bIsAI || !VehicleMesh) return;

	// ------------------------------------------------------------
	// Direction fatigue timer
	// ------------------------------------------------------------
	if (AI_DirectionFatigueTime > 0.f)
	{
		AI_DirectionFatigueTime = FMath::Max(0.f, AI_DirectionFatigueTime - DeltaTime);
	}

	// ------------------------------------------------------------
	// Obstacle handbrake pulse timer
	// ------------------------------------------------------------
	if (AI_ObstacleHandbrakeTime > 0.f)
	{
		AI_ObstacleHandbrakeTime = FMath::Max(0.f, AI_ObstacleHandbrakeTime - DeltaTime);
	}

	// 0) Tow override
	if (bBeingTowed)
	{
		AI_ApplyInputs(0.f, 0.f, 0.f);
		bDisableResetToSplineCuzOfTowed = true;
		return;
	}
	bDisableResetToSplineCuzOfTowed = false;

	// 1) Spline end handling stays top-priority
	if (bReachedSplineEnd)
	{
		AI_ApplyInputs(0.f, 0.f, 1.f);
		return;
	}

	USplineComponent* AISpline = GetAISpline();
	if (!AISpline) return;

	const FVector MyLoc = GetActorLocation();
	const float SpeedKmh = CachedSpeedKmh;
	float AI_CurveBrake01 = 0.f;

	// ------------------------------------------------------------
	// 2) Build base spline target
	// ------------------------------------------------------------
	float LocalSplineDist = AISpline->GetDistanceAlongSplineAtSplineInputKey(
		AISpline->FindInputKeyClosestToWorldLocation(MyLoc)
	);

	const FVector BaseTarget =
		AISpline->GetLocationAtDistanceAlongSpline(
			LocalSplineDist + 300.f,
			ESplineCoordinateSpace::World
		);


	// ------------------------------------------------------------
	// 3) NavMesh OR spline steering target
	// ------------------------------------------------------------

	FVector DesiredSteerPoint = BaseTarget;
	bool bUsingNav = false;

	// 3A) SPLINE FOLLOW: never use navmesh here, ever.
	if (false)
	{
		// (dead on purpose)
	}
	else
	{
		const float SplineLength = AISpline->GetSplineLength();
		float SplineDist = AISpline->GetDistanceAlongSplineAtSplineInputKey(
			AISpline->FindInputKeyClosestToWorldLocation(MyLoc)
		);

		if (!bAILoop && SplineDist >= SplineLength - 100.f)
		{
			bReachedSplineEnd = true;
			AI_ApplyInputs(0.f, 0.f, 1.f);
			return;
		}

		const float SteerLook = BaseSteerLookAhead + (SpeedKmh * SteerLookSpeedFactor);
		float SteerTargetDist = SplineDist + SteerLook;
		const float BrakeLook = BaseBrakeLookAhead + (SpeedKmh * BrakeLookSpeedFactor);

		float BrakeTargetDist = SplineDist + BrakeLook;
		BrakeTargetDist = bAILoop
			? FMath::Fmod(BrakeTargetDist, SplineLength)
			: FMath::Min(BrakeTargetDist, SplineLength);

		const FVector BrakePoint =
			AISpline->GetLocationAtDistanceAlongSpline(
				BrakeTargetDist,
				ESplineCoordinateSpace::World
			);

		FVector ToBrake = (BrakePoint - MyLoc);
		ToBrake.Z = 0.f;

		if (!ToBrake.IsNearlyZero())
		{
			const FVector Fwd2D = VehicleMesh->GetForwardVector().GetSafeNormal2D();
			const FVector Dir2D = ToBrake.GetSafeNormal();

			const float DotAhead = FMath::Clamp(FVector::DotProduct(Fwd2D, Dir2D), -1.f, 1.f);
			const float AngleDegAhead = FMath::RadiansToDegrees(FMath::Acos(DotAhead));

			AI_CurveBrake01 = FMath::Clamp((AngleDegAhead - 6.f) / 55.f, 0.f, 1.f);
		}

		SteerTargetDist = bAILoop
			? FMath::Fmod(SteerTargetDist, SplineLength)
			: FMath::Min(SteerTargetDist, SplineLength);

		DesiredSteerPoint =
			AISpline->GetLocationAtDistanceAlongSpline(
				SteerTargetDist,
				ESplineCoordinateSpace::World
			);

		const FVector SplineRight =
			AISpline->GetRightVectorAtDistanceAlongSpline(
				SteerTargetDist,
				ESplineCoordinateSpace::World
			);

		DesiredSteerPoint += SplineRight * AISplineOffset;
	}

	// ------------------------------------------------------------
	// 4) Reset-to-spline logic (UNCHANGED)
	// ------------------------------------------------------------
	{
		const bool bIsStuck = FMath::Abs(SpeedKmh) < 5.f;
		const bool bShouldReset = bIsStuck || bIsInAir;

		if (bShouldReset)
		{
			TimeStuckForSplineReset += DeltaTime;
			if (TimeStuckForSplineReset >= ResetToRoadDelay && bResetToRoad)
			{
				ResetVehiclePositionToSpline();
				TimeStuckForSplineReset = 0.f;
			}
		}
		else
		{
			TimeStuckForSplineReset = 0.f;
		}
	}

	// ------------------------------------------------------------
	// 5) OBSTACLE LOGIC (100% PRESERVED)
	// ------------------------------------------------------------
	bool bObstacleOverride = false;
	float ObSteer = 0.f;
	float ObThrottle = 0.f;
	float ObBrake = 0.f;
	bool bNavLineClear = true;

	{
		const FVector Location = MyLoc;
		const FVector Forward = VehicleMesh->GetForwardVector().GetSafeNormal();

		const float ClampedSpeedKmh = FMath::Clamp(SpeedKmh, 0.f, DesiredMaxSpeedForFullThrottleKmh);
		const float TraceDistance = FMath::Lerp(
			AIObstacleTraceLengthMin,
			AIObstacleTraceLengthMax,
			ClampedSpeedKmh / FMath::Max(DesiredMaxSpeedForFullThrottleKmh, 1.f)
		);

		const FVector BaseStart =
			Location +
			Forward * AIObstacleTraceForwardOffset +
			FVector(0.f, 0.f, AIObstacleTraceZOffset);

		const FVector Right = VehicleMesh->GetRightVector();
		const int32 HalfTraceCount = AIObstacleTraceCount / 2;

		float ClosestHitDistance = TraceDistance;
		int32 ClosestHitIndex = 0;
		bool bHitDetected = false;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		// --- nav reclaim test ---
		{
			const FVector ToSteer = DesiredSteerPoint - Location;
			const float Len = ToSteer.Size();

			if (Len > 1.f)
			{
				const FVector NavEnd =
					BaseStart +
					ToSteer.GetSafeNormal() * FMath::Min(Len, TraceDistance);

				FHitResult NavHit;
				const bool bNavHit =
					GetWorld()->LineTraceSingleByChannel(
						NavHit,
						BaseStart,
						NavEnd,
						ECC_Vehicle,
						QueryParams
					);

				if (bNavHit && NavHit.GetActor() && NavHit.GetActor()->ActorHasTag(ObstacleTag))
				{
					bNavLineClear = false;
				}
			}
		}

		for (int32 i = -HalfTraceCount; i <= HalfTraceCount; ++i)
		{
			const FVector Offset = Right * i * AIObstacleTraceSpread;
			const FVector Start = BaseStart + Offset;
			const FVector End = Start + Forward * TraceDistance;

			FHitResult Hit;
			const bool bHit =
				GetWorld()->LineTraceSingleByChannel(
					Hit,
					Start,
					End,
					ECC_Vehicle,
					QueryParams
				);

			if (bHit && Hit.GetActor() && Hit.GetActor()->ActorHasTag(ObstacleTag))
			{
				const float HitDist = (Hit.ImpactPoint - Start).Size();
				if (HitDist < ClosestHitDistance)
				{
					ClosestHitDistance = HitDist;
					ClosestHitIndex = i;
				}
				bHitDetected = true;
			}
		}

		const bool bShouldAllowObstacleOverride = bHitDetected;

		if (bShouldAllowObstacleOverride)
		{
			bObstacleOverride = true;

			const float TL = FMath::Max(1.f, TraceDistance);
			const float Close01 = 1.f - FMath::Clamp(ClosestHitDistance / TL, 0.f, 1.f);

			if (bShouldStopBehindObstacles)
			{
				if (Close01 > 0.35f)
				{
					AI_ObstacleHandbrakeTime = FMath::Max(AI_ObstacleHandbrakeTime, 0.20f);
				}

				const float SideSign = (ClosestHitIndex >= 0) ? -1.f : 1.f;
				ObSteer = FMath::Clamp(SideSign * AIAvoidanceSteeringMultiplier, -1.f, 1.f);
				ObThrottle = 0.f;
				ObBrake = 1.f;
			}
			else
			{
				const float SideSign = (ClosestHitIndex >= 0) ? -1.f : 1.f; // hit on right -> steer left
				ObSteer = FMath::Clamp(SideSign * AIAvoidanceSteeringMultiplier, -1.f, 1.f);
				ObThrottle = FMath::Clamp(AINav_MinThrottle, 0.f, 1.f);
				ObBrake = 0.f;

				AI_ObstacleHandbrakeTime = 0.f;
			}
		}

	}

	if (bObstacleOverride)
	{
		if (bShouldStopBehindObstacles && AI_ObstacleHandbrakeTime > 0.f)
		{
			AI_ApplyInputs(0.f, 0.f, 1.f);
			return;
		}

		AI_ApplyInputs(ObSteer, ObThrottle, ObBrake);
		return;
	}


	// ------------------------------------------------------------
	// 6) NORMAL DRIVING (UNCHANGED)
	// ------------------------------------------------------------
	float SteerCmd = AI_ComputeSteerToPoint(DesiredSteerPoint);

	SteerCmd = FMath::Clamp(
		SteerCmd + AI_NavWallSteerBias(MyLoc),
		-1.f,
		1.f
	);


	const float TurnAbs = FMath::Abs(SteerCmd);
	float Throttle01 = 1.f - TurnAbs * AINav_TurnThrottleDrop;
	Throttle01 = FMath::Clamp(Throttle01, AINav_MinThrottle, 1.f);

	float DesiredSpeedKmh = AIDesiredMaxSpeedKmh * Throttle01;

	if (AI_CurveBrake01 > 0.f)
	{
		const float MinFactor = 0.25f;
		const float CurveFactor = FMath::Clamp(1.f - (AI_CurveBrake01 * 0.80f), MinFactor, 1.f);
		DesiredSpeedKmh *= CurveFactor;
	}

	const float SpeedKmhAbs = FMath::Abs(SpeedKmh);
	const bool bOver =
		SpeedKmhAbs >
		(DesiredSpeedKmh + AINav_BrakeWhenOverSpeedKmh);


	float Brake01 = 0.f;
	if (bOver)
	{
		const float Over = SpeedKmhAbs - DesiredSpeedKmh;
		Brake01 = FMath::Clamp(Over / 30.f, 0.2f, 1.f);
		Throttle01 = 0.f;
	}

	// ------------------------------------------------------------
	// SMART NITRO (SPLINE FOLLOW)
	// ------------------------------------------------------------
	if (bAICanUseNitro &&
		!bNitroActive &&
		!bNitroButtonHeld &&
		CurrentNitroFuel > 0.f &&
		!bIsInAir &&
		Brake01 <= 0.f &&
		AI_CurveBrake01 < 0.15f &&          // straight spline
		TurnAbs < 0.20f &&                  // low steering
		SpeedKmh < (DesiredSpeedKmh - 15.f))// room to accelerate
	{
		StartNitro();
	}
	else if (bNitroActive &&
		(CurrentNitroFuel <= 0.f ||
			AI_CurveBrake01 > 0.30f ||         // entering curve
			Brake01 > 0.f))                    // braking
	{
		StopNitro();
	}

	AI_ApplyInputs(SteerCmd, Throttle01, Brake01);
}

void AVehicle::AI_NavDriveToward(const FVector& SteerPoint, float DeltaTime)
{
	if (!VehicleMesh) return;

	UWorld* W = GetWorld();
	if (!W) return;

	const FVector MyLoc = GetActorLocation();

	// ---------------------------------------------------------
	// ARRIVE / STOP + HOLD (SEAMLESS RESUME)
	// ---------------------------------------------------------
	const float StopR = FMath::Max(10.f, AINav_StopRadiusCm);
	const float StopExitR = StopR * 1.25f;

	if (!AINav_FinalTarget.IsNearlyZero())
	{
		const float Dist2D = FVector::Dist2D(MyLoc, AINav_FinalTarget);

		if (bIsHandBrake && (Dist2D > StopExitR))
		{
			bIsHandBrake = false;
		}

		if (Dist2D <= StopR && CachedSpeedKmh > -1.f)
		{
			Steer(FInputActionValue(0.f));
			MoveForward(FInputActionValue(0.f));

			const float SpeedAbsKmhHold = FMath::Abs(CachedSpeedKmh);

			// Brake with Reverse ONLY while we still have speed.
			// Once nearly stopped, stop feeding Reverse (otherwise it becomes reverse throttle)
			// and use handbrake to hold the vehicle still.
			if (SpeedAbsKmhHold > 1.f)
			{
				bIsHandBrake = false;
				Reverse(FInputActionValue(1.f));
			}
			else
			{
				Reverse(FInputActionValue(0.f));
				bIsHandBrake = true;
			}

			TimeStuck = 0.f;
			TimeReversing = 0.f;
			bIsReversingFromStuck = false;
			ReverseTimeDuration = 0.f;

			ReverseSteerDirection = 0.f;

			AINav_AvoidOverrideRemain = 0.f;
			AINav_ObstacleBrakePulseRemain = 0.f;

			return;
		}

	}

	// ---------------------------------------------------------
	// BASIS VECTORS (needed for BOTH nav and non-nav)
	// ---------------------------------------------------------
	FVector Fwd = VehicleMesh->GetForwardVector();
	Fwd.Z = 0.f;
	Fwd = Fwd.GetSafeNormal();

	FVector Right = VehicleMesh->GetRightVector();
	Right.Z = 0.f;
	Right = Right.GetSafeNormal();

	if (Fwd.IsNearlyZero() || Right.IsNearlyZero())
	{
		Steer(FInputActionValue(0.f));
		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(0.f));
		bIsHandBrake = false;
		return;
	}

	// ---------------------------------------------------------
	// ALWAYS COMPUTE OBSTACLE AVOIDANCE FIRST (UNCONDITIONAL)
	// Fixes:
	// - side sign is computed from vehicle center, NOT from each trace Start
	// - when reversing, do REAR traces (so reverse is not aborted by forward obstacle)
	// ---------------------------------------------------------
	float AvoidSteerBias = 0.f;
	float ClosestObstacleDist = BIG_NUMBER;
	float ClosestObstacleSideSign = 0.f;
	bool bSawObstacle = false;

	float TraceLenUsed = 0.f;

	const bool bDoRearTraces = bIsReversingFromStuck; // key fix

	if (AIObstacleTraceCount > 0)
	{
		const float SpeedAbsKmh0 = FMath::Abs(CachedSpeedKmh);
		const float SpeedRatio = FMath::Clamp(SpeedAbsKmh0 / FMath::Max(1.f, AIDesiredMaxSpeedKmh), 0.f, 1.f);

		const float TraceLen = FMath::Lerp(AIObstacleTraceLengthMin, AIObstacleTraceLengthMax, SpeedRatio);
		TraceLenUsed = TraceLen;

		const FVector TraceDir = bDoRearTraces ? (-Fwd) : (Fwd);

		const FVector TraceStartBase =
			MyLoc
			+ (bDoRearTraces ? (-Fwd) : (Fwd)) * AIObstacleTraceForwardOffset
			+ FVector(0.f, 0.f, AIObstacleTraceZOffset);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		const float Half = 0.5f * float(AIObstacleTraceCount - 1);

		for (int32 i = 0; i < AIObstacleTraceCount; ++i)
		{
			const float OffsetIdx = float(i) - Half;
			const FVector Start = TraceStartBase + Right * (OffsetIdx * AIObstacleTraceSpread);
			const FVector End = Start + TraceDir * TraceLen;

			FHitResult Hit;
			const bool bHit = W->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

			if (bAINav_DrawDebug)
			{
				DrawDebugLine(W, Start, End, bHit ? FColor::Red : FColor::Green, false, 0.f, 0, 1.f);
				if (bHit) DrawDebugPoint(W, Hit.ImpactPoint, 8.f, FColor::Yellow, false, 0.f);
			}

			if (!bHit) continue;

			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor == this) continue;

			if (!ObstacleTag.IsNone() && !HitActor->ActorHasTag(ObstacleTag))
			{
				continue;
			}

			bSawObstacle = true;

			const float Dist = (Hit.ImpactPoint - Start).Size();
			const float DistN = FMath::Clamp(Dist / FMath::Max(1.f, TraceLen), 0.f, 1.f);
			const float Alpha = 1.f - DistN;

			// IMPORTANT FIX: side sign relative to vehicle center, stable across all lines
			const float SideCenter = FVector::DotProduct((Hit.ImpactPoint - MyLoc), Right);
			const float SideSign = (SideCenter >= 0.f) ? -1.f : 1.f;

			if (Dist < ClosestObstacleDist)
			{
				ClosestObstacleDist = Dist;
				ClosestObstacleSideSign = SideSign;
			}

			AvoidSteerBias += SideSign * Alpha;
		}

		AvoidSteerBias = FMath::Clamp(AvoidSteerBias, -1.f, 1.f) * AIAvoidanceSteeringMultiplier;
	}

	// ---------------------------------------------------------
	// OBSTACLE CLOSENESS METRICS + OVERRIDE NAV STEER TIMER
	// ---------------------------------------------------------
	float Close01 = 0.f;
	if (bSawObstacle)
	{
		const float TL = FMath::Max(1.f, TraceLenUsed);
		Close01 = 1.f - FMath::Clamp(ClosestObstacleDist / TL, 0.f, 1.f);

		// be more willing to override nav steer when obstacle is seen (especially in reverse)
		if (Close01 > 0.18f || bDoRearTraces)
		{
			AINav_AvoidOverrideRemain = FMath::Max(
				AINav_AvoidOverrideRemain,
				FMath::Max(0.f, AINav_AvoidOverrideSeconds)
			);
		}

		// optional stable lock (if you add a member var)
		// if (AINav_ObstacleLockSign == 0.f && ClosestObstacleSideSign != 0.f) AINav_ObstacleLockSign = ClosestObstacleSideSign;
		// GLOBAL: if StopBehind is enabled, brake-hold behind obstacle (never reverse forever, never handbrake here).
		if (bShouldStopBehindObstacles)
		{
			// Start/refresh a short brake-hold timer when obstacle is meaningfully close.
			// (Using existing AI_ObstacleHandbrakeTime as "brake hold" - no new vars.)
			if (Close01 > 0.35f)
			{
				AI_ObstacleHandbrakeTime = FMath::Max(AI_ObstacleHandbrakeTime, 0.20f);
			}
		}

		if (bAINav_DrawDebug)
		{
			DrawDebugString(
				W,
				MyLoc + FVector(0.f, 0.f, 120.f),
				FString::Printf(
					TEXT("Close01=%.2f  OverrideRemain=%.2f  Rear=%d"),
					Close01,
					AINav_AvoidOverrideRemain,
					bDoRearTraces ? 1 : 0
				),
				nullptr,
				FColor::Red,
				0.f,
				true
			);
		}

	}

	// tick down override/pulse timers
	if (AINav_AvoidOverrideRemain > 0.f) AINav_AvoidOverrideRemain = FMath::Max(0.f, AINav_AvoidOverrideRemain - DeltaTime);
	if (AINav_ObstacleBrakePulseRemain > 0.f) AINav_ObstacleBrakePulseRemain = FMath::Max(0.f, AINav_ObstacleBrakePulseRemain - DeltaTime);

	// Tick down global brake-hold timer (stop-behind-obstacle)
	if (AI_ObstacleHandbrakeTime > 0.f)
	{
		AI_ObstacleHandbrakeTime = FMath::Max(0.f, AI_ObstacleHandbrakeTime - DeltaTime);
	}

	// If holding behind obstacle, keep steering away smartly but BRAKE (no handbrake).
	if (AI_ObstacleHandbrakeTime > 0.f)
	{
		bIsHandBrake = false;

		// Keep computed FinalSteer (avoid + wall) later, but force brake.
		// We do it early by steering with current best info:
		const float WallBiasHold = AI_NavWallSteerBias(MyLoc);
		const float SteerHold = FMath::Clamp(AvoidSteerBias + WallBiasHold, -1.f, 1.f);

		Steer(FInputActionValue(SteerHold));
		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(1.f));
		return;
	}

	// ---------------------------------------------------------
	// NAVMESH UNDERFOOT CHECK (switch to direct chase if off-nav)
	// ---------------------------------------------------------
	bool bHasNavUnder = true; // default true for spline


	// ---------------------------------------------------------
	// BUILD TARGET DIRECTION
	// ---------------------------------------------------------
	FVector AimPoint = SteerPoint;

	if (bAINav_DrawDebug)
	{
		DrawDebugSphere(
			W,
			SteerPoint,
			35.f,
			16,
			FColor::Green,
			false,
			0.f
		);

		DrawDebugLine(
			W,
			MyLoc,
			SteerPoint,
			FColor::Green,
			false,
			0.f,
			0,
			2.f
		);
	}


	if (!bHasNavUnder)
	{
		if (!AINav_FinalTarget.IsNearlyZero())
		{
			AimPoint = AINav_FinalTarget;
		}
	}

	FVector To = (AimPoint - MyLoc);
	To.Z = 0.f;

	if (To.IsNearlyZero())
	{
		if (bSawObstacle)
		{
			const float WallBias0 = AI_NavWallSteerBias(MyLoc);

			// stable lock direction: reuse ReverseSteerDirection if already set, else use closest sign
			const float LockDir0 = (ReverseSteerDirection != 0.f) ? ReverseSteerDirection : ClosestObstacleSideSign;
			const float Avoid0 = (LockDir0 != 0.f) ? LockDir0 : AvoidSteerBias;

			const float SteerOut0 = FMath::Clamp(Avoid0 + WallBias0, -1.f, 1.f);

			bIsHandBrake = false;
			Steer(FInputActionValue(SteerOut0));

			const float SpeedAbsKmh0 = FMath::Abs(CachedSpeedKmh);
			const float DesiredBlind = FMath::Max(5.f, AIDesiredMaxSpeedKmh * 0.35f);
			const bool bOverBlind = (SpeedAbsKmh0 > (DesiredBlind + AINav_BrakeWhenOverSpeedKmh));

			if (bOverBlind)
			{
				const float Over = SpeedAbsKmh0 - DesiredBlind;
				float Brake01 = FMath::Clamp(Over / 10.f, 0.35f, 1.f);
				Brake01 = FMath::Clamp(Brake01 * AIBrakeAggressiveness, 0.f, 1.f);

				MoveForward(FInputActionValue(0.f));
				Reverse(FInputActionValue(Brake01));
			}
			else
			{
				Reverse(FInputActionValue(0.f));
				MoveForward(FInputActionValue(FMath::Clamp(AINav_MinThrottle, 0.f, 1.f)));
			}

			TimeStuck = 0.f;
			return;
		}

		Steer(FInputActionValue(0.f));
		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(0.f));
		bIsHandBrake = false;
		return;
	}

	// ---------------------------------------------------------
	// REVERSE DECISION (NAV-ONLY, NO TARGET LOGIC)
	// Reverse ONLY if next NAV steer point is behind vehicle
	// ---------------------------------------------------------
	const FVector Dir = To.GetSafeNormal();
	const float DotForward = FVector::DotProduct(Fwd, Dir);

	// reversing state driven ONLY by nav geometry (HARD HOLD)
	static bool bNavReverseHold = false;

	if (DotForward < -0.15f)
	{
		bNavReverseHold = true;
	}
	else if (DotForward > 0.25f) // clear forward again
	{
		bNavReverseHold = false;
	}

	bIsReversingFromStuck = bNavReverseHold;

	const FVector SenseFwd = bIsReversingFromStuck ? (-Fwd) : (Fwd);
	float CrossZ = FVector::CrossProduct(SenseFwd, Dir).Z;

	// flip steering ONLY if reverse-follow is allowed
	if (bIsReversingFromStuck && bInsideNavCanFollowInReverse)
	{
		CrossZ = -CrossZ;
	}



	// ---------------------------------------------------------
	// STEERING: NAV + WALL + AVOID
	// Fix: if obstacle close or override active, nav steer weight goes to near zero
	// ---------------------------------------------------------
	const float BaseSteer = FMath::Clamp(CrossZ * 2.5f, -1.f, 1.f);
	const float WallBias = AI_NavWallSteerBias(MyLoc);

	if (bAINav_DrawDebug && WallBias != 0.f)
	{
		const FVector WallDir =
			(Right * WallBias * 200.f);

		DrawDebugLine(
			W,
			MyLoc,
			MyLoc + WallDir,
			FColor::Orange,
			false,
			0.f,
			0,
			3.f
		);
	}

	float FinalSteer = 0.f;

	if (bSawObstacle)
	{
		if (ReverseSteerDirection == 0.f && ClosestObstacleSideSign != 0.f)
		{
			ReverseSteerDirection = ClosestObstacleSideSign; // stable lock
		}

		const float LockDir = (ReverseSteerDirection != 0.f) ? ReverseSteerDirection : ClosestObstacleSideSign;

		float AvoidSteer = AvoidSteerBias;
		if (LockDir != 0.f)
		{
			AvoidSteer = FMath::Clamp((LockDir * (0.55f + 0.45f * Close01)) + (AvoidSteerBias * 0.25f), -1.f, 1.f);
		}

		const bool bAvoidOverride = (AINav_AvoidOverrideRemain > 0.f);

		if (bAvoidOverride || Close01 > 0.35f)
		{
			// HARD OVERRIDE: no nav mixing when it matters
			FinalSteer = FMath::Clamp(AvoidSteer + WallBias, -1.f, 1.f);
		}
		else
		{
			const float AvoidW = FMath::Clamp(0.20f + Close01 * 0.90f, 0.f, 1.f);
			const float NavW = 1.f - AvoidW;

			FinalSteer = (BaseSteer * NavW) + (AvoidSteer * AvoidW) + WallBias;
			FinalSteer = FMath::Clamp(FinalSteer, -1.f, 1.f);
		}
	}
	else
	{
		ReverseSteerDirection = 0.f;
		FinalSteer = FMath::Clamp(BaseSteer + WallBias, -1.f, 1.f);
	}

	if (bAINav_DrawDebug)
	{
		const FVector FinalDir =
			(Fwd + Right * FinalSteer).GetSafeNormal();

		DrawDebugLine(
			W,
			MyLoc,
			MyLoc + FinalDir * 350.f,
			FColor::White,
			false,
			0.f,
			0,
			4.f
		);
	}


	// ---------------------------------------------------------
	// THROTTLE / BRAKE
	// ---------------------------------------------------------
	bIsHandBrake = false;


	Steer(FInputActionValue(FinalSteer));

	const float TurnAbs = FMath::Abs(FinalSteer);

	float Throttle01 = 1.f - TurnAbs * AINav_TurnThrottleDrop;
	Throttle01 = FMath::Clamp(Throttle01, AINav_MinThrottle, 1.f);
	Throttle01 = FMath::Clamp(Throttle01 * AIGasAggressiveness, 0.f, 1.f);

	// ---------------------------------------------------------
	// APPLY FORWARD OR REVERSE BASED ON NAV SENSE (HARD HOLD)
	// ---------------------------------------------------------
	if (bIsReversingFromStuck)
	{
		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(Throttle01));
		return;
	}

	float DesiredSpeedKmh = AIDesiredMaxSpeedKmh * Throttle01;

	if (bSawObstacle && bAIObstacleAffectsThrottle)
	{
		const float MinSpeedFactor = 0.28f;
		const float SpeedFactor = FMath::Clamp(1.f - (Close01 * Close01) * 0.85f, MinSpeedFactor, 1.f);
		DesiredSpeedKmh *= SpeedFactor;

		if (Close01 > 0.78f && AINav_ObstacleBrakePulseRemain <= 0.f)
		{
			AINav_ObstacleBrakePulseRemain = FMath::Max(0.f, AINav_ObstacleBrakePulseSeconds);
		}
	}



	const float SpeedAbsKmh = FMath::Abs(CachedSpeedKmh);
	const bool bOver = (SpeedAbsKmh > (DesiredSpeedKmh + AINav_BrakeWhenOverSpeedKmh));
	if (bAIObstacleAffectsThrottle &&
		AINav_ObstacleBrakePulseRemain > 0.f &&
		bOver)
	{
		const float Over = SpeedAbsKmh - DesiredSpeedKmh;

		float Brake01 = FMath::Clamp(Over / 8.f, 0.55f, 1.f);
		Brake01 = FMath::Clamp(Brake01 * AIBrakeAggressiveness, 0.f, 1.f);

		// Handbrake ONLY when obstacle is extremely close: 5%-10% of trace length.
		bIsHandBrake = (bSawObstacle && (Close01 >= 0.90f) && (Close01 <= 0.95f));


		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(Brake01));
		return;
	}

	if (bOver)
	{
		const float Over = SpeedAbsKmh - DesiredSpeedKmh;

		float Brake01 = FMath::Clamp(Over / 10.f, 0.35f, 1.f);
		Brake01 = FMath::Clamp(Brake01 * AIBrakeAggressiveness, 0.f, 1.f);

		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(Brake01));
	}
	else
	{
		Reverse(FInputActionValue(0.f));
		MoveForward(FInputActionValue(Throttle01));
	}
}

void AVehicle::UpdateAIChasing(float DeltaTime)
{
	if (!bIsAI || !VehicleMesh || !bAIChasing || !IsValid(AIChaseLockedTarget))
	{
		return;
	}

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	// ============================================================
	// AIRBORNE RECOVERY
	// ============================================================
	if (bIsInAir)
	{
		TimeInAir += DeltaTime;
		if (TimeInAir >= 5.f)
		{
			ResetVehiclePosition();
			TimeInAir = 0.f;
		}
	}
	else
	{
		TimeInAir = 0.f;
	}

	const FVector Location = GetActorLocation();
	const FVector Forward = VehicleMesh->GetForwardVector().GetSafeNormal();
	const float SpeedKmh = CachedSpeedKmh;

	// ============================================================
	// LOCKED TARGET
	// ============================================================
	const FVector TargetLocation = AIChaseLockedTarget->GetActorLocation();
	const FVector ToTarget2D = (TargetLocation - Location).GetSafeNormal2D();
	const float DistanceCm = FVector::Dist(TargetLocation, Location);

	// ============================================================
	// STOP / REACHED ZONE (meters -> cm)
	// ============================================================
	const float ReachedCm = FMath::Max(0.f, ChaseReachedDistanceM) * 100.f;
	const float StopCm = FMath::Max(ReachedCm, 10.f);
	const float StopExitCm = StopCm * 1.25f;

	if (bIsHandBrake && DistanceCm > StopExitCm)
	{
		bIsHandBrake = false;
	}

	if (DistanceCm <= StopCm)
	{
		Steer(FInputActionValue(0.f));

		if (SpeedKmh < -1.f)
		{
			bIsHandBrake = false;
			Reverse(FInputActionValue(0.f));
			MoveForward(FInputActionValue(1.f));
			return;
		}

		if (SpeedKmh > 1.f)
		{
			bIsHandBrake = false;
			MoveForward(FInputActionValue(0.f));
			Reverse(FInputActionValue(1.f));
			return;
		}

		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(0.f));
		bIsHandBrake = true;
		return;
	}

	AI_NavTargetLocation = TargetLocation;
	bAINavExitedMesh = false;


	// ============================================================
	// NAV MESH CHASE (PREFERRED)
	// ============================================================
	if (AI_NavIsUsable() && !bAINavExitedMesh)
	{
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(W);
		if (NavSys)
		{
			FNavLocation ProjectedTarget;
			if (NavSys->ProjectPointToNavigation(
				TargetLocation,
				ProjectedTarget,
				FVector(800.f, 800.f, 400.f)))
			{
				AINav_RepathTimer -= DeltaTime;
				if (!AINavPath || AINav_RepathTimer <= 0.f)
				{
					AINav_StopRadiusCm = StopCm;
					AI_NavBuildPathTo(ProjectedTarget.Location);
					AINav_RepathTimer = 0.35f;
				}

				const FVector SteerPoint = AI_NavGetSteerPoint(Location);
				AI_NavDriveToward(SteerPoint, DeltaTime);
				return;
			}
		}
	}

	// ============================================================
	// REVERSE FOLLOW OVERRIDE (OFF-NAV ONLY)
	// ============================================================
	const bool bOffNav =
		(!AI_NavIsUsable() || bAINavExitedMesh);

	if (bOffNav && IsValid(AIChaseLockedTarget))
	{
		if (AIReverseFollowMode == EAIReverseFollowMode::ReverseTowardTarget)
		{
			const float ReverseDistCm = ReverseFollowDistanceM * 100.f;

			const FVector ToTarget =
				(TargetLocation - Location).GetSafeNormal2D();

			// Dot < 0 means target is behind us
			const float DotToTarget =
				FVector::DotProduct(Forward, ToTarget);

			const bool bTargetBehind = (DotToTarget < -0.15f);

			if (bTargetBehind && DistanceCm <= ReverseDistCm)
			{
				// Rear-facing steering logic
				float CrossZ =
					FVector::CrossProduct(Forward, ToTarget).Z;

				// Invert steering ONLY while reversing
				CrossZ = -CrossZ;

				// Deadzone (prevents zigzag)
				if (FMath::Abs(CrossZ) < 0.05f)
				{
					CrossZ = 0.f;
				}

				// Soft damping (stateless)
				CrossZ *= -FMath::Clamp(DeltaTime * 6.f, 0.f, 1.f);

				const float SteerOut =
					FMath::Clamp(
						CrossZ * AISteeringMultiplier * 3.5f,
						-1.f,
						1.f
					);

				// HARD HOLD REVERSE
				bIsHandBrake = false;
				Steer(FInputActionValue(SteerOut));
				MoveForward(FInputActionValue(0.f));
				Reverse(FInputActionValue(1.f));

				return; // critical: do not fall through
			}
		}
		else if (AIReverseFollowMode == EAIReverseFollowMode::SuperRealisticReverse)
		{
			const float ReverseDistCm = ReverseFollowDistanceM * 100.f;

			const FVector ToTarget =
				(TargetLocation - Location).GetSafeNormal2D();

			// Dot < 0 means target is behind us
			const float DotToTarget =
				FVector::DotProduct(Forward, ToTarget);

			const bool bTargetBehind = (DotToTarget < -0.15f);

			if (bTargetBehind && DistanceCm <= ReverseDistCm)
			{
				// Rear-facing steering logic
				float CrossZ =
					FVector::CrossProduct(Forward, ToTarget).Z;

				// Invert steering ONLY while reversing
				CrossZ = -CrossZ;

				// Deadzone (prevents zigzag)
				if (FMath::Abs(CrossZ) < 0.05f)
				{
					CrossZ = 0.f;
				}

				// Soft damping (stateless)
				CrossZ *= FMath::Clamp(DeltaTime * 6.f, 0.f, 1.f);

				const float SteerOut =
					FMath::Clamp(
						CrossZ * AISteeringMultiplier * 3.5f,
						-1.f,
						1.f
					);

				// HARD HOLD REVERSE
				bIsHandBrake = false;
				Steer(FInputActionValue(SteerOut));
				MoveForward(FInputActionValue(0.f));
				Reverse(FInputActionValue(1.f));

				return; // critical: do not fall through
			}
		}
		else
		{
			// Continue as bellow
		}
	}

	// ============================================================
	// FINAL DIRECT STEERING (ANTI-ZIGZAG + BEHIND FIX)
	// ============================================================
	const float Dot =
		FMath::Clamp(FVector::DotProduct(Forward, ToTarget2D), -1.f, 1.f);

	const float AngleDeg =
		FMath::RadiansToDegrees(FMath::Acos(Dot));

	float CrossZ = FVector::CrossProduct(Forward, ToTarget2D).Z;

	// === TARGET DIRECTLY BEHIND ===
	// Force a hard steering decision instead of hesitation
	if (Dot < -0.85f)
	{
		CrossZ = (CrossZ >= 0.f) ? 1.f : -1.f;
	}

	// Deadzone (normal case only)
	if (FMath::Abs(CrossZ) < 0.02f)
	{
		CrossZ = 0.f;
	}

	// Sensitivity scaled by angle
	float SteerSensitivity =
		FMath::GetMappedRangeValueClamped(
			FVector2D(0.f, 90.f),
			FVector2D(2.5f, 6.0f),
			AngleDeg);

	if (AngleDeg < 10.f)
	{
		SteerSensitivity *= AngleDeg / 10.f;
	}

	float SteerValue =
		CrossZ *
		SteerSensitivity *
		AISteeringMultiplier;

	// Stateless damping
	SteerValue *= FMath::Clamp(DeltaTime * 6.f, 0.f, 1.f);

	Steer(FInputActionValue(FMath::Clamp(SteerValue, -1.f, 1.f)));


	//// ============================================================
	//// FINAL DIRECT STEERING (ANTI-ZIGZAG)
	//// ============================================================
	//const float Dot =
	//	FMath::Clamp(FVector::DotProduct(Forward, ToTarget2D), -1.f, 1.f);

	//const float AngleDeg =
	//	FMath::RadiansToDegrees(FMath::Acos(Dot));

	//float CrossZ = FVector::CrossProduct(Forward, ToTarget2D).Z;

	//// Deadzone
	//if (FMath::Abs(CrossZ) < 0.02f)
	//{
	//	CrossZ = 0.f;
	//}

	//// Sensitivity scaled by angle
	//float SteerSensitivity =
	//	FMath::GetMappedRangeValueClamped(
	//		FVector2D(0.f, 90.f),
	//		FVector2D(2.5f, 6.0f),
	//		AngleDeg);

	//if (AngleDeg < 10.f)
	//{
	//	SteerSensitivity *= AngleDeg / 10.f;
	//}

	//float SteerValue =
	//	CrossZ *
	//	SteerSensitivity *
	//	AISteeringMultiplier;


	//// Stateless damping
	//SteerValue *= FMath::Clamp(DeltaTime * 6.f, 0.f, 1.f);

	//Steer(FInputActionValue(FMath::Clamp(SteerValue, -1.f, 1.f)));

	// ============================================================
	// THROTTLE / BRAKE
	// ============================================================
	const float Brake =
		FMath::Clamp((AngleDeg - 5.f) / 40.f * AIBrakeAggressiveness, 0.f, 1.f);

	const float IdealSpeed =
		FMath::Lerp(
			FMath::Min(AIDesiredMaxSpeedKmh, DesiredMaxSpeedForFullThrottleKmh),
			50.f,
			Brake);

	const float Throttle =
		FMath::Clamp((IdealSpeed - SpeedKmh) / 50.f * AIGasAggressiveness, -1.f, 1.f);

	bIsHandBrake = false;

	if (Throttle < -Brake)
	{
		Reverse(FInputActionValue(FMath::Abs(Throttle)));
		MoveForward(FInputActionValue(0.f));
	}
	else
	{
		Reverse(FInputActionValue(0.f));
		MoveForward(FInputActionValue(FMath::Clamp(Throttle, 0.f, 1.f)));
	}
}

bool AVehicle::AI_NavIsUsable() const
{
	if (!bUseNavMeshSystem) return false;

	UWorld* W = GetWorld();
	if (!W) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	if (!NavSys) return false;

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (!NavData) return false;

	// Quick sanity: can we project our own location to nav?
	FNavLocation Projected;
	const bool bOk = NavSys->ProjectPointToNavigation(GetActorLocation(), Projected);
	return bOk;
}

bool AVehicle::AI_NavBuildPathTo(const FVector& WorldTarget)
{
	UWorld* W = GetWorld();
	if (!W) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	if (!NavSys) return false;

	// NEW:
	AINav_FinalTarget = WorldTarget;
	if (AINav_StopRadiusCm <= 0.f)
	{
		AINav_StopRadiusCm = FMath::Max(10.f, AINav_ArriveRadiusCm);
	}

	const FVector Extent(AINav_ProjectExtentXY, AINav_ProjectExtentXY, AINav_ProjectExtentZ);

	FVector Start = GetActorLocation();
	if (VehicleMesh)
	{
		FVector Fwd = VehicleMesh->GetForwardVector();
		Fwd.Z = 0.f;
		Start = Start + Fwd.GetSafeNormal() * AINav_StartForwardOffsetCm;
	}

	FNavLocation StartOnNav;
	if (!NavSys->ProjectPointToNavigation(Start, StartOnNav, Extent)) return false;

	FNavLocation TargetOnNav;
	if (!NavSys->ProjectPointToNavigation(WorldTarget, TargetOnNav, Extent)) return false;

	AINavPath = NavSys->FindPathToLocationSynchronously(W, StartOnNav.Location, TargetOnNav.Location, this);
	AINavPathIndex = 0;

	if (!AINavPath || AINavPath->PathPoints.Num() < 2) return false;
	return true;
}

float AVehicle::AI_NavWallSteerBias(const FVector& MyLoc) const
{
	if (!bAINav_AvoidWalls || !VehicleMesh) return 0.f;

	UWorld* W = GetWorld();
	if (!W) return 0.f;

	const FVector Fwd0 = VehicleMesh->GetForwardVector().GetSafeNormal();
	const FVector Right = VehicleMesh->GetRightVector().GetSafeNormal();

	// Use "motion-forward" direction: when reversing, treat rear as forward for wall steering.
	const bool bReverseSense = (CachedSpeedKmh < -1.f) || bIsReversingFromStuck;
	const FVector SenseFwd = bReverseSense ? (-Fwd0) : (Fwd0);

	const FVector Start = MyLoc + FVector(0.f, 0.f, 40.f) + SenseFwd * 60.f; // a bit forward/up (relative to motion)
	const FVector LeftStart = Start - Right * AINav_WallTraceSideOffsetCm;
	const FVector RightStart = Start + Right * AINav_WallTraceSideOffsetCm;


	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult HitL, HitR;
	const FVector LeftEnd = LeftStart + SenseFwd * AINav_WallTraceLenCm;
	const FVector RightEnd = RightStart + SenseFwd * AINav_WallTraceLenCm;


	const bool bL = W->LineTraceSingleByChannel(HitL, LeftStart, LeftEnd, ECC_Visibility, Params);
	const bool bR = W->LineTraceSingleByChannel(HitR, RightStart, RightEnd, ECC_Visibility, Params);

	float Bias = 0.f;

	if (bL)
	{
		const float DL = (HitL.ImpactPoint - LeftStart).Size();
		if (DL < AINav_WallMinClearCm)
		{
			const float Alpha = 1.f - FMath::Clamp(DL / FMath::Max(1.f, AINav_WallMinClearCm), 0.f, 1.f);
			Bias += Alpha; // steer right (positive)
		}
	}
	if (bR)
	{
		const float DR = (HitR.ImpactPoint - RightStart).Size();
		if (DR < AINav_WallMinClearCm)
		{
			const float Alpha = 1.f - FMath::Clamp(DR / FMath::Max(1.f, AINav_WallMinClearCm), 0.f, 1.f);
			Bias -= Alpha; // steer left (negative)
		}
	}

	Bias = FMath::Clamp(Bias * AINav_WallSteerStrength, -1.f, 1.f);

	return Bias;
}

FVector AVehicle::AI_NavGetSteerPoint(const FVector& MyLoc) const
{
	if (!AINavPath || AINavPath->PathPoints.Num() < 2) return MyLoc;
	if (bAINav_DrawDebug)
	{
		const TArray<FVector>& PtsDbg = AINavPath->PathPoints;

		for (int32 i = 0; i < PtsDbg.Num() - 1; ++i)
		{
			DrawDebugLine(
				GetWorld(),
				PtsDbg[i],
				PtsDbg[i + 1],
				FColor::Cyan,
				false,
				0.f,
				0,
				3.f
			);
		}
	}

	const TArray<FVector>& Pts = AINavPath->PathPoints;
	int32 Idx = FMath::Clamp(AINavPathIndex, 0, Pts.Num() - 1);

	const bool bReverseSense =
		(CachedSpeedKmh < -1.f) || bIsReversingFromStuck;

	// Only advance path index when NOT reversing
	if (!bReverseSense)
	{
		while (Idx < Pts.Num() - 1)
		{
			const float Dist = FVector::Dist2D(MyLoc, Pts[Idx]);
			if (Dist > AINav_ArriveRadiusCm) break;
			Idx++;
		}

		const_cast<AVehicle*>(this)->AINavPathIndex = Idx;
	}


	// NEW: persist progress
	const_cast<AVehicle*>(this)->AINavPathIndex = Idx;

	float Remaining = AINav_LookAheadCm;
	FVector From = (Idx > 0) ? Pts[Idx - 1] : MyLoc;

	for (int32 i = Idx; i < Pts.Num(); ++i)
	{
		const FVector To = Pts[i];
		const float Seg = FVector::Dist(From, To);

		if (Seg >= Remaining)
		{
			const float Alpha = (Seg > KINDA_SMALL_NUMBER) ? (Remaining / Seg) : 1.f;
			return FMath::Lerp(From, To, Alpha);
		}

		Remaining -= Seg;
		From = To;
	}

	if (bAINav_DrawDebug && AINavPath)
	{
		const TArray<FVector>& PtsDbg = AINavPath->PathPoints;

		for (int32 i = 0; i < PtsDbg.Num() - 1; ++i)
		{
			const FVector A = PtsDbg[i];
			const FVector B = PtsDbg[i + 1];

			DrawDebugLine(
				GetWorld(),
				A,
				B,
				FColor::Cyan,
				false,
				0.f,
				0,
				3.f
			);
		}

		// Draw current path index
		if (PtsDbg.IsValidIndex(AINavPathIndex))
		{
			DrawDebugSphere(
				GetWorld(),
				PtsDbg[AINavPathIndex],
				25.f,
				12,
				FColor::Yellow,
				false,
				0.f
			);
		}
	}


	return Pts.Last();
}

void AVehicle::ResetVehiclePositionToSpline()
{
	if (bDisableResetToSplineCuzOfTowed || !bIsAI || !VehicleMesh)
		return;

	ResetBalanceFunction();

	USplineComponent* AISpline = GetAISpline();
	if (!AISpline) return;

	InAirTime = 0.f;

	// === Closest Spline Location & Rotation ===
	const FVector CurrentLocation = GetActorLocation();
	const float SplineInputKey = AISpline->FindInputKeyClosestToWorldLocation(CurrentLocation);
	const float DistanceOnSpline = AISpline->GetDistanceAlongSplineAtSplineInputKey(SplineInputKey);
	const FVector SplineLocation = AISpline->GetLocationAtDistanceAlongSpline(DistanceOnSpline, ESplineCoordinateSpace::World);
	const FVector SplineTangent = AISpline->GetTangentAtDistanceAlongSpline(DistanceOnSpline, ESplineCoordinateSpace::World);
	const FRotator UprightRotation(0.f, SplineTangent.ToOrientationRotator().Yaw, 0.f);
	const FVector TeleportLocation = SplineLocation + FVector(0, 0, 200.f);

	// === Detach Trailer If Present ===
	if (TrailerReference)
	{
		TrailerReference->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		TrailerReference->SetActorEnableCollision(false);
		TrailerReference->SetActorHiddenInGame(true);
	}

	// === Teleport Vehicle ===
	SetActorLocationAndRotation(TeleportLocation, UprightRotation, false, nullptr, ETeleportType::ResetPhysics);
	VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	// === Position Trailer Behind Vehicle ===
	if (TrailerReference)
	{
		const FVector ForwardDir = UprightRotation.Vector(); // Forward vector from yaw
		const FVector TrailerLocation = TeleportLocation - ForwardDir * TrailerResetForwardOffset;

		TrailerReference->SetActorLocationAndRotation(
			TrailerLocation,
			UprightRotation,
			false, nullptr, ETeleportType::ResetPhysics
		);

		if (UPrimitiveComponent* TrailerMesh = Cast<UPrimitiveComponent>(TrailerReference->GetRootComponent()))
		{
			TrailerMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			TrailerMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	// === Reattach After Delay ===
	if (TrailerReference)
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
			{
				if (TrailerReference)
				{
					TrailerReference->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
					TrailerReference->SetActorEnableCollision(true);
					TrailerReference->SetActorHiddenInGame(false);
				}
			}, 0.05f, false);
	}

	bPendingResetToRoad = false; // ← FIX: ensure re-triggering is possible
	// === Debug Sphere ===
	if (bShowDebugLines)
	{
		DrawDebugSphere(GetWorld(), TeleportLocation, 50.f, 12, FColor::Cyan, false, 2.f);
	}
}

void AVehicle::UpdateCameraRotation(float DeltaTime)
{
	if (bIsAI || !IsPlayerControlledVehicle() || !SpringArm || !GetActiveCameraMode().bCanFreeLook)
		return;

	const FRotator VehicleRot = GetActorRotation();
	const FRotator MeshRot = VehicleMesh->GetComponentRotation();

	// ------------------------
	// TARGET ROTATION
	// ------------------------
	float TargetYaw;
	float TargetPitch;

	if (GetActiveCameraMode().ModeName == "Driver")
	{
		// Driver: pure free look, no vehicle influence
		TargetYaw = FMath::Clamp(CameraYaw, CameraMinYawInDrive, CameraMaxYawInDrive);
		TargetPitch = FMath::Clamp(CameraPitch, CameraMinPitchInDrive, CameraMaxPitchInDrive);
	}
	else
	{
		// External: vehicle yaw + free look + reverse offset
		TargetYaw = VehicleRot.Yaw + CameraYaw + ReverseCameraYawOffset;

		// External pitch follows terrain
		TargetPitch = MeshRot.Pitch + CameraPitch;
		TargetPitch = FMath::Clamp(TargetPitch, -89.9f, 89.9f);
	}

	const FRotator DesiredRot(TargetPitch, TargetYaw, 0.f);

	// ------------------------
	// SMOOTHING (NO INPUT DEPENDENCE)
	// ------------------------
	const float InterpSpeed =
		bIsInAir
		? CameraRotationLagSpeed * InAirCameraYawFollowMultiplier
		: CameraRotationLagSpeed;

	const FRotator CurrentRot = SpringArm->GetRelativeRotation();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, InterpSpeed);

	SpringArm->SetRelativeRotation(NewRot);

	// ------------------------
	// ARM LENGTH DYNAMICS
	// ------------------------
	if (GetActiveCameraMode().ModeName == "ThirdPerson")
	{
		const float SpeedRatio = FMath::Clamp(CachedSpeedKmh / ArmLengthMaxLagSpeed, 0.f, 1.f);
		const float TargetLen = GetActiveCameraMode().ArmLength + (SpeedRatio * ArmLengthLagStretch);

		CurrentArmLength = FMath::FInterpTo(CurrentArmLength, TargetLen, DeltaTime, 5.f);
		SpringArm->TargetArmLength = CurrentArmLength;
	}
	else
	{
		SpringArm->TargetArmLength = GetActiveCameraMode().ArmLength;
		CurrentArmLength = SpringArm->TargetArmLength;
	}
}

void AVehicle::ResetCameraToInitial(float DeltaTime)
{
	if (bIsAI || !IsPlayerControlledVehicle() || HasRecentCameraInput(GetWorld()->GetTimeSeconds()) || !GetActiveCameraMode().bCanFreeLook)
		return;

	// === [1] Determine Target Angles ===
	float TargetYaw = 0.f;
	float TargetPitch = InitialSpringArmRotation.Pitch;

	if (GetActiveCameraMode().ModeName == "Driver")
	{
		TargetYaw = 0.f;
		TargetPitch = 0.f;
	}
	else
	{
		TargetYaw = InitialSpringArmRotation.Yaw;
	}

	// === [2] Normalize both angles before interpolating ===
	// This ensures it takes the shortest angular path (e.g., -5 to 5 instead of 355 to 5)
	CameraYaw = FMath::FInterpTo(
		FMath::UnwindDegrees(CameraYaw),
		FMath::UnwindDegrees(TargetYaw),
		DeltaTime,
		6.f
	);

	CameraPitch = FMath::FInterpTo(
		FMath::UnwindDegrees(CameraPitch),
		FMath::UnwindDegrees(TargetPitch),
		DeltaTime,
		6.f
	);
}

bool AVehicle::HasRecentCameraInput(float CurrentTime) const
{
	return (CurrentTime - LastCameraInputTime) < CameraResetDelay;
}

void AVehicle::ApplyAntiSlopeSlideForce()
{
	if (bBeingTowed)
	{
		bIsHillGripped = false;
		return;
	}
	if (!VehicleMesh || !bCanHillGrip || bIsDrifting)
	{
		bIsHillGripped = false;
		return;
	}


	float tiltAmount = CachedGyroRotation.Pitch;

	FVector SSSS = VehicleMesh->GetPhysicsLinearVelocity();
	FVector zzzzzz = VehicleMesh->GetForwardVector();

	bool Bbb = FMath::Abs(FVector::DotProduct(SSSS, zzzzzz)) < 10.f;
	// This is the *correct* logic gate
	bool bShouldApplyGrip =
		bVehicleIsOnSlope &&
		//FMath::Abs(CachedSpeedKmh) < MaxHillGripSpeedKmh &&
		!bIsInAir &&
		FMath::Abs(tiltAmount) <= MaxHillGripAngle;

	bool bShouldApplyGripMotorcycle = !bIsInAir && bIsMotorcycle;

	if ((bShouldApplyGrip || bShouldApplyGripMotorcycle))
	{


		// === END: Gas + Uphill = reduce grip ===
		const float SpeedThreshold = MaxHillGripSpeedKmh;
		if (FMath::Abs(CachedSpeedKmh) < SpeedThreshold && GasPaddle == 0.f)
		{

			FVector CurrentVel = VehicleMesh->GetPhysicsLinearVelocity();
			FVector HorizontalVel = FVector::VectorPlaneProject(CurrentVel, VehicleMesh->GetUpVector());
			FVector DampedVel = HorizontalVel * 0.05f; // damp to 5% of horizontal velocity
			FVector NewVel = CurrentVel - HorizontalVel + DampedVel;
			VehicleMesh->SetPhysicsLinearVelocity(NewVel);
		}
		else
		{
		}


		bIsHillGripped = true;
	}
	else
	{
		bIsHillGripped = false;
	}
}

void AVehicle::OnCameraChange()
{
	if (CameraModes.Num() == 0 || !SpringArm || !Camera)
		return;

	// === Deactivate All Camera Modes ===
	for (FCameraModeConfig& Mode : CameraModes)
	{
		Mode.bIsActive = false;
	}

	const int32 NumModes = CameraModes.Num();
	int32 NextIndex = CurrentCameraIndex;
	bool bFound = false;

	// Find next enabled mode
	for (int32 Attempt = 0; Attempt < NumModes; ++Attempt)
	{
		NextIndex = (NextIndex + 1) % NumModes;

		if (CameraModes[NextIndex].bEnabled)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("No enabled camera modes found. Reverting to default."));
		SetCameraModeByName("ThirdPerson");
		return;
	}

	// === Activate Mode ===
	CurrentCameraIndex = NextIndex;
	FCameraModeConfig& ActiveMode = CameraModes[CurrentCameraIndex];
	ActiveMode.bIsActive = true;

	// === Apply Spring Arm Settings ===
	SpringArm->bEnableCameraLag = ActiveMode.bEnableCameraLag;
	SpringArm->CameraLagSpeed = ActiveMode.CameraLagSpeed;
	SpringArm->bEnableCameraRotationLag = ActiveMode.bEnableCameraRotationLag;
	SpringArm->CameraRotationLagSpeed = ActiveMode.CameraRotationLagSpeed;
	SpringArm->TargetArmLength = ActiveMode.ArmLength;
	SpringArm->SocketOffset = ActiveMode.SocketOffset;
	SpringArm->bDoCollisionTest = ActiveMode.bDoCollisionTest;
	SpringArm->ProbeChannel = ActiveMode.ProbeChannel;
	SpringArm->ProbeSize = ActiveMode.ProbeSize;
	SpringArm->bUsePawnControlRotation = ActiveMode.bUsePawnControlRotation;
	SpringArm->bInheritPitch = ActiveMode.bInheritPitch;
	SpringArm->bInheritRoll = ActiveMode.bInheritRoll;
	SpringArm->bInheritYaw = ActiveMode.bInheritYaw;

	// === Smooth Arm Movement Transition ===
	SecondTargetArmLocation = ActiveMode.ArmLocation;
	SecondTargetArmLength = ActiveMode.ArmLength;
	InitialSpringArmRotation = ActiveMode.ArmRotation;
	InitialSpringArmLength = ActiveMode.ArmLength;
	InitialSpringArmLocation = ActiveMode.ArmLocation;
	CurrentArmLength = ActiveMode.ArmLength;

	// === Set camera pitch to the mode's value, but Yaw to zero (forward-facing) ===
	CameraPitch = ActiveMode.ArmRotation.Pitch;
	CameraYaw = 0.f;

	// Force rotation if freelook is disabled
	if (!ActiveMode.bCanFreeLook)
	{
		SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
	}

	// === FOV and Z offset ===
	Camera->SetFieldOfView(ActiveMode.bUseFieldOfViewOverride ? ActiveMode.CameraFOV : 90.f);
	Camera->SetRelativeLocation(FVector::UpVector * ActiveMode.TargetOffsetZ);
}

void AVehicle::UpdateSteeringWheelVisual(float DeltaTime)
{
	if (!SteeringWheel) return;

	FRotator NewRotation = SteeringWheelInitialRotation;

	switch (SteeringVisualMode)
	{
	case ESteeringVisualMode::CarArcade:
	{
		const float TargetSteeringAngle = SteeringInput * MaxVisualSteeringAngle;
		CurrentSteeringVisualAngle = FMath::FInterpTo(CurrentSteeringVisualAngle, TargetSteeringAngle, DeltaTime, SteeringWheelLerpSpeed);
		NewRotation.Pitch = -CurrentSteeringVisualAngle;
		break;
	}

	//case ESteeringVisualMode::CarRealistic:
	//{
	//	NewRotation.Pitch = -CachedSteeringDeg * RealisticSteeringVisualMultiplier;
	//	break;
	//}
	case ESteeringVisualMode::CarRealistic:
	{
		NewRotation.Pitch = -CachedSteeringDeg * RealisticSteeringVisualMultiplier;

		// === Apply visual countersteer yaw from drifting suspension (if any) ===
		for (USuspension* Suspension : CachedSuspensions)
		{
			if (Suspension && Suspension->bIsDriftWheel && Suspension->bIsSteeringWheel)
			{
				NewRotation.Yaw = Suspension->CachedVisualYaw;
				break;
			}
		}

		break;
	}




	case ESteeringVisualMode::MotorcycleArcade:
	{
		const float TargetYaw = SteeringInput * MotorcycleMaxSteeringAngle;
		CurrentSteeringVisualAngle = FMath::FInterpTo(CurrentSteeringVisualAngle, TargetYaw, DeltaTime, MotorcycleSteeringLerpSpeed);
		NewRotation.Yaw = CurrentSteeringVisualAngle;
		break;
	}

	case ESteeringVisualMode::MotorcycleRealistic:
	{
		NewRotation.Yaw = CachedSteeringDeg * RealisticMotorcycleSteeringVisualMultiplier;
		break;
	}
	}

	// Preserve original Roll (Z) and apply only Yaw or Pitch rotation
	const FRotator InitialRot = SteeringWheelInitialRotation;

	// Depending on mode, inject into the correct axis only:
	switch (SteeringVisualMode)
	{
		case ESteeringVisualMode::CarArcade:
		{
			const float TargetSteeringAngle = SteeringInput * MaxVisualSteeringAngle;
			CurrentSteeringVisualAngle = FMath::FInterpTo(CurrentSteeringVisualAngle, TargetSteeringAngle, DeltaTime, SteeringWheelLerpSpeed);

			SteeringWheel->SetRelativeRotation(FRotator(
				InitialRot.Pitch,
				InitialRot.Yaw,
				InitialRot.Roll - CurrentSteeringVisualAngle
			));
			break;
		}
		//case ESteeringVisualMode::CarRealistic:
		//{
		//	SteeringWheel->SetRelativeRotation(FRotator(
		//		InitialRot.Pitch,
		//		InitialRot.Yaw,
		//		InitialRot.Roll - CachedSteeringDeg * RealisticSteeringVisualMultiplier
		//	));
		//	break;
		//}
		case ESteeringVisualMode::CarRealistic:
		{
			float SteeringRoll = -CachedSteeringDeg * RealisticSteeringVisualMultiplier;

			// === Use double-smooth countersteer yaw ===
			float TargetYaw = 0.f;

			for (USuspension* Suspension : CachedSuspensions)
			{
				if (Suspension && Suspension->bIsDriftWheel && Suspension->bIsSteeringWheel && Suspension->bIsDrifting)
				{
					TargetYaw = -Suspension->CachedVisualYaw; // Mirror it
					break;
				}
			}

			// === Extra smoothing on steering wheel visual countersteer ===
			SmoothedSteeringWheelYaw = FMath::FInterpTo(SmoothedSteeringWheelYaw, TargetYaw, DeltaTime, 2.f); // <- adjust speed here
			SteeringRoll += SmoothedSteeringWheelYaw * 2.f;;

			// === Apply final roll ===
			SteeringWheel->SetRelativeRotation(FRotator(
				InitialRot.Pitch,
				InitialRot.Yaw,
				InitialRot.Roll + SteeringRoll
			));
			break;
		}


		case ESteeringVisualMode::MotorcycleArcade:
		{
			const float TargetYaw = SteeringInput * MotorcycleMaxSteeringAngle;
			CurrentSteeringVisualAngle = FMath::FInterpTo(CurrentSteeringVisualAngle, TargetYaw, DeltaTime, MotorcycleSteeringLerpSpeed);

			SteeringWheel->SetRelativeRotation(FRotator(
				InitialRot.Pitch,
				InitialRot.Yaw + CurrentSteeringVisualAngle,
				InitialRot.Roll
			));
			break;
		}

		//case ESteeringVisualMode::MotorcycleRealistic:
		//{
		//	// Calculate realistic handlebar tilt — Z axis roll
		//	const float TargetRoll = CachedSteeringDeg * RealisticMotorcycleSteeringVisualMultiplier;
		//	CurrentSteeringVisualAngle = FMath::FInterpTo(CurrentSteeringVisualAngle, TargetRoll, DeltaTime, MotorcycleSteeringLerpSpeed);

		//	SteeringWheel->SetRelativeRotation(FRotator(
		//		InitialRot.Pitch,
		//		InitialRot.Yaw,
		//		InitialRot.Roll + CurrentSteeringVisualAngle // Apply to Roll (Z)
		//	));
		//	break;
		//}
		
		case ESteeringVisualMode::MotorcycleRealistic:
		{
			for (USuspension* Suspension : CachedSuspensions)
			{
				if (Suspension && Suspension->bIsSteeringWheel)
				{
					// Snap exact local rotation from suspension to the steering wheel
					const FRotator SuspensionRotation = Suspension->GetRelativeRotation();
					SteeringWheel->SetRelativeRotation(SuspensionRotation);
					break;
				}
			}
			break;
		}




	}

}

bool AVehicle::IsPlayerControlledVehicle() const
{
	return Controller && Cast<APlayerController>(Controller) != nullptr;
}

FCameraModeConfig AVehicle::GetActiveCameraMode()
{
	for (const FCameraModeConfig& Mode : CameraModes)
	{
		if (Mode.bIsActive)
		{
			return Mode;
		}
	}

	// Return a default if none found
	return FCameraModeConfig();
}

void AVehicle::SetCameraModeByName(FName ModeName)
{
	if (CameraModes.Num() == 0 || !SpringArm || !Camera)
		return;

	for (int32 i = 0; i < CameraModes.Num(); ++i)
	{
		if (CameraModes[i].ModeName == ModeName)
		{
			// === Deactivate All Camera Modes ===
			for (FCameraModeConfig& Mode : CameraModes)
			{
				Mode.bIsActive = false;
			}

			// === Activate Matching Mode ===
			CurrentCameraIndex = i;
			FCameraModeConfig& ActiveMode = CameraModes[i];
			ActiveMode.bIsActive = true;


			// === Set Spring Arm Properties ===
			SpringArm->bEnableCameraLag = ActiveMode.bEnableCameraLag;
			SpringArm->CameraLagSpeed = ActiveMode.CameraLagSpeed;
			SpringArm->bEnableCameraRotationLag = ActiveMode.bEnableCameraRotationLag;
			SpringArm->CameraRotationLagSpeed = ActiveMode.CameraRotationLagSpeed;
			SpringArm->SocketOffset = ActiveMode.SocketOffset;
			SpringArm->bDoCollisionTest = ActiveMode.bDoCollisionTest;
			SpringArm->ProbeChannel = ActiveMode.ProbeChannel;
			SpringArm->ProbeSize = ActiveMode.ProbeSize;
			SpringArm->bUsePawnControlRotation = ActiveMode.bUsePawnControlRotation;
			SpringArm->bInheritPitch = ActiveMode.bInheritPitch;
			SpringArm->bInheritRoll = ActiveMode.bInheritRoll;
			SpringArm->bInheritYaw = ActiveMode.bInheritYaw;


			// === Smooth Transition - Save Current Location, then Update Target ===
			SecondTargetArmLocation = SpringArm->GetRelativeLocation(); // Save current position
			SecondTargetArmLength = SpringArm->TargetArmLength;         // Save current length

			// Apply new target (will interpolate in Tick)
			SecondTargetArmLocation = ActiveMode.ArmLocation;
			SecondTargetArmLength = ActiveMode.ArmLength;

			// === Set Rotation Directly (rotation doesn't interpolate) ===
			SpringArm->SetRelativeRotation(ActiveMode.ArmRotation);


			// Snap the free-look to match new camera mode rotation
			CameraPitch = ActiveMode.ArmRotation.Pitch;
			CameraYaw = ActiveMode.ArmRotation.Yaw;




			InitialSpringArmRotation = SpringArm->GetRelativeRotation();
			CameraPitch = InitialSpringArmRotation.Pitch;
			CameraYaw = InitialSpringArmRotation.Yaw;
			InitialSpringArmLength = SpringArm->TargetArmLength;
			InitialSpringArmLocation = SpringArm->GetRelativeLocation();
			SecondTargetArmLength = InitialSpringArmLength;
			SecondTargetArmLocation = InitialSpringArmLocation;
			CurrentArmLength = SpringArm->TargetArmLength;

			// === Apply Camera Settings ===
			if (ActiveMode.bUseFieldOfViewOverride)
			{
				Camera->SetFieldOfView(ActiveMode.CameraFOV);
			}
			else
			{
				Camera->SetFieldOfView(90.f);
			}

			// === Apply Camera Offset ===
			Camera->SetRelativeLocation(FVector::UpVector * ActiveMode.TargetOffsetZ);

			// === Reset Free Look if Disabled ===
			if (!ActiveMode.bCanFreeLook)
			{
				CameraYaw = 0.f;
				CameraPitch = 0.f;
			}

			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Camera mode '%s' not found."), *ModeName.ToString());
}

void AVehicle::UpdateGForceStats(float DeltaTime)
{
	if (!VehicleMesh || DeltaTime <= KINDA_SMALL_NUMBER) return;

	const FVector CurrentVelocity = VehicleMesh->GetPhysicsLinearVelocity();
	const FVector Accel = (CurrentVelocity - LastVelocityG) / DeltaTime;
	LastVelocityG = CurrentVelocity;

	const float Gravity = 980.f;

	const FVector Forward = VehicleMesh->GetForwardVector();
	const FVector Right = VehicleMesh->GetRightVector();
	const FVector Up = VehicleMesh->GetUpVector();

	FVector NetAccel = Accel;

	// Inject fake Gs when falling in air
	if (bIsInAir)
	{
		// Simulate gravity pressing "downward" in local space
		NetAccel += FVector(0.f, 0.f, -Gravity); // 1G down
	}

	LateralG = FVector::DotProduct(NetAccel, Right) / Gravity;
	LongitudinalG = FVector::DotProduct(NetAccel, Forward) / Gravity;
	VerticalG = FVector::DotProduct(NetAccel, Up) / Gravity;

	CombinedGVector2D = FVector2D(LongitudinalG, LateralG);

}

void AVehicle::DrawGForceDial()
{
	if (!bShowDebugLines) return;

	const FVector VehicleLocation = VehicleMesh->GetComponentLocation();
	const FVector Up = VehicleMesh->GetUpVector();
	const FVector Right = VehicleMesh->GetRightVector();
	const FVector Fwd = VehicleMesh->GetForwardVector();

	const float Radius = 80.f;
	const float GScale = 30.f;
	const float MaxG = 2.5f;

	const FVector DialCenter = VehicleLocation + Up * 150.f;

	// === [1] Circle Outline ===
	const int32 NumSegments = 32;
	for (int32 i = 0; i < NumSegments; ++i)
	{
		const float Angle1 = i * 2 * PI / NumSegments;
		const float Angle2 = (i + 1) * 2 * PI / NumSegments;

		const FVector P1 = DialCenter + (Fwd * FMath::Cos(Angle1) + Right * FMath::Sin(Angle1)) * Radius;
		const FVector P2 = DialCenter + (Fwd * FMath::Cos(Angle2) + Right * FMath::Sin(Angle2)) * Radius;

		if (bShowDebugLines)
		{
			DrawDebugLine(GetWorld(), P1, P2, FColor::White, false, 0.f, 0, 1.f);
		}
	}

	// === [2] Clamp and Invert G-Force Vector ===
	FVector2D ClampedG = CombinedGVector2D;
	if (ClampedG.Size() > MaxG)
	{
		ClampedG.Normalize();
		ClampedG *= MaxG;
	}
	ClampedG *= -1.f; // Invert to represent reactive force on driver

	// === [3] Calculate G-Ball position
	const FVector LocalOffset = Fwd * ClampedG.X * GScale + Right * ClampedG.Y * GScale;
	const FVector MarkerPos = DialCenter + LocalOffset;


	if (bShowDebugLines)
	{
		// === [4] Draw G-meter Dots ===// [4] Center dot - remains unchanged
		DrawDebugSphere(GetWorld(), DialCenter, 4.f, 8, FColor::White, false, 0.f, 0, 1.f);

		// [5] G-force ball — smaller radius, same width, new color
		DrawDebugSphere(GetWorld(), MarkerPos, 3.f, 8, FColor::Orange, false, 0.f, 0, 1.f);
	}

}

void AVehicle::ApplyDownforces()
{
	if (!VehicleMesh) return;// || bIsInAir

	for (const FDownforceElement& Element : DownforcePoints)
	{
		if (!Element.bIsFunctional) continue;
		if (bIsInAir && !Element.bIsFunctionalInAir) continue;

		const float SpeedRatio = FMath::Clamp(CachedSpeedKmh / Element.MaxEffectivenessSpeed, 0.f, 1.f);
		const float FinalForce = Element.Force * SpeedRatio;

		const FVector WorldLocation = VehicleMesh->GetComponentTransform().TransformPosition(Element.LocalOffset);
		const FVector DownVector = -VehicleMesh->GetUpVector(); // Vehicle-local down direction
		const FVector DownForce = DownVector * FinalForce;

		VehicleMesh->AddForceAtLocation(DownForce, WorldLocation);

		if (bShowDebugLines)
		{
			DrawDebugLine(GetWorld(), WorldLocation, WorldLocation + DownForce * 0.001f, FColor::Purple, false, 0.f, 0, 2.f);
			DrawDebugString(GetWorld(), WorldLocation, FString::Printf(TEXT("%s (%.0fN)"), *Element.Name.ToString(), FinalForce), nullptr, FColor::White, 0.f, true);
		}
	}
}

void AVehicle::SimulateDrivetrain(float DeltaTime)
{
	// =========================
	// ENGINE OFF -> VISUAL DECAY
	// =========================
	if (!bEngineRunning)
	{
		TargetRPM = 0.f;
		CachedRPM = FMath::FInterpTo(CachedRPM, 0.f, DeltaTime, 2.5f);
		CachedThrottleForce = 0.f;
		bWasRevvingWithHandbrake = false;
		return;
	}

	// =========================
	// STARTUP FLARE (VISUAL ONLY)
	// MinRPM = idle, MaxRPM = redline
	// =========================
	const float StartupRPM = FMath::Min(MinRPM * 2.f, MaxRPM);

	if (CachedRPM < MinRPM * 0.95f)
	{
		TargetRPM = StartupRPM;
		CachedRPM = FMath::FInterpTo(CachedRPM, TargetRPM, DeltaTime, 6.f);
		CachedThrottleForce = 0.f;
		return; // OWN THE TICK
	}

	// =========================
	// HARD DRIVETRAIN GUARDS
	// =========================
	if (CachedDriveWheelCount <= 0 || !VehicleMesh)
		return;

	const float SpeedKmh = CachedSpeedKmh;
	const float Gas = FMath::Clamp(FMath::Abs(GasPaddle), 0.f, 1.f);

	// =========================
	// NEUTRAL GEAR
	// =========================
	if (CachedGearIndex == -1)
	{
		if (Gas > 0.f && !bIsInAir)
		{
			const float DesiredRPM = FMath::Lerp(MinRPM, MaxRPM, Gas);
			TargetRPM = DesiredRPM;
			CachedRPM = FMath::FInterpTo(CachedRPM, TargetRPM, DeltaTime, 6.f);
		}
		else
		{
			TargetRPM = MinRPM;
			CachedRPM = FMath::FInterpTo(CachedRPM, TargetRPM, DeltaTime, 6.f);
		}

		CachedThrottleForce = 0.f;
		return;
	}

	const bool bShouldRevWithHandbrake =
		!bIsInAir &&
		bIsHandBrake &&
		Gas > 0.05f &&
		CachedGearIndex >= 1 &&
		!bIsBraking;

	const bool bInRevBoostRPMRange = CachedRPM >= RevBoostMinRPM && CachedRPM <= RevBoostMaxRPM;

	const bool bReleasedRev =
		bWasRevvingWithHandbrake &&
		!bShouldRevWithHandbrake &&
		bInRevBoostRPMRange &&
		CachedSpeedKmh <= KINDA_SMALL_NUMBER &&
		CachedGearIndex >= 0;

	if (bReleasedRev && VehicleMesh)
	{
		const FVector BoostDirection = GetActorForwardVector();
		const FVector LaunchImpulse = BoostDirection * LaunchBoostStrength;
		VehicleMesh->AddImpulse(LaunchImpulse);
	}

	bWasRevvingWithHandbrake = bShouldRevWithHandbrake;

	if (bShouldRevWithHandbrake)
	{
		const float RevTarget = FMath::Lerp(MinRPM, MaxRPM * HandbrakeRevLimitAlpha, Gas);
		CachedRPM = FMath::FInterpTo(CachedRPM, RevTarget, DeltaTime, 5.5f);
		TargetRPM = CachedRPM;
		CachedThrottleForce = 0.f;
		return;
	}

	if (bIsInAir && Gas > 0.f)
	{
		const float ClutchBlend = (!bClutchEnabled || Clutch >= 0.99f) ? 1.f : Clutch;
		const float DesiredRPM = FMath::Lerp(MinRPM, MaxRPM, Gas * ClutchBlend);

		if (DesiredRPM > CachedRPM)
		{
			CachedRPM = FMath::FInterpTo(CachedRPM, DesiredRPM, DeltaTime, 5.f);
			CachedRPM = FMath::Clamp(CachedRPM, MinRPM, MaxRPM);
		}

		TargetRPM = CachedRPM;
		CachedThrottleForce = 0.f;
		return;
	}

	const bool bIsInReverse = (CachedGearIndex == 0);
	const int32 NumForwardGears = FMath::Min(GearSpeedThresholdsKmh.Num(), GearPushForces.Num()) - 1;
	int32 CurrentGear = CachedGearIndex;

	if (bAutomaticTransmission && bIsInReverse && !bIsReversing)
	{
		CachedGearIndex = 1;
	}

	if (bIsBraking)
	{
		CachedThrottleForce = 0.f;
		TargetRPM = CachedRPM = MinRPM;
		return;
	}

	if (bIsInReverse)
	{
		const float MaxReverseSpeed = GearSpeedThresholdsKmh.IsValidIndex(0) ? GearSpeedThresholdsKmh[0] : 30.f;
		const float ReversePush = GearPushForces.IsValidIndex(0) ? GearPushForces[0] : 3000.f;
		const float SpeedRatio = FMath::Clamp(FMath::Abs(SpeedKmh) / MaxReverseSpeed, 0.f, 1.f);

		const float ClutchFactor = bClutchEnabled ? FMath::Clamp(Clutch, 0.f, 1.f) : 1.f;
		const float ThrottleRPM = FMath::Lerp(MinRPM, MaxRPM, Gas);

		float WheelRPM = FMath::Lerp(MinRPM, MaxRPM, SpeedRatio);

		if (Gas > 0.f && ClutchFactor > 0.1f && SpeedKmh < 10.f)
		{
			const float Alpha = FMath::Clamp(1.f - (SpeedKmh / 10.f), 0.f, 1.f);
			const float Boost = FMath::Lerp(0.f, 1000.f, Alpha);
			WheelRPM += Boost;
		}

		const float SlipBlend = FMath::Clamp(FMath::Pow(1.f - ClutchFactor, 1.5f), 0.f, 1.f);
		const float TargetRPMValue = FMath::Lerp(WheelRPM, ThrottleRPM, SlipBlend);

		CachedRPM = FMath::FInterpTo(CachedRPM, TargetRPMValue, DeltaTime, 10.f);
		CachedRPM = FMath::Clamp(CachedRPM, MinRPM, MaxRPM);
		TargetRPM = CachedRPM;

		const float ClutchSmooth = FMath::Clamp(FMath::Pow(ClutchFactor, 1.5f), 0.f, 1.f);
		CachedThrottleForce = ReversePush * Gas * ClutchSmooth;
		return;
	}

	const float MaxGearSpeed = GearSpeedThresholdsKmh.IsValidIndex(CurrentGear)
		? GearSpeedThresholdsKmh[CurrentGear]
		: DesiredMaxSpeedForFullThrottleKmh;

	const float RawSpeedRatio = SpeedKmh / MaxGearSpeed;
	const float SpeedRatio = FMath::Clamp(RawSpeedRatio, 0.f, 1.f);

	float TorqueFade = 1.f;
	if (CurrentGear > 1 && GearSpeedThresholdsKmh.IsValidIndex(CurrentGear))
	{
		const float GearMinEffectiveSpeed = GearSpeedThresholdsKmh[CurrentGear] * 0.35f;
		const float SpeedBelowIdeal = FMath::Clamp((GearMinEffectiveSpeed - SpeedKmh) / GearMinEffectiveSpeed, 0.f, 1.f);
		const float FadeStrength = FMath::Pow(SpeedBelowIdeal, 1.4f);
		const float GearWeight = FMath::Pow(float(CurrentGear), 0.6f);
		float TorquePenalty = FMath::Lerp(1.f, 0.1f, FadeStrength * GearWeight);

		if (bClutchEnabled && Clutch < 0.98f)
		{
			TorquePenalty = FMath::Lerp(1.f, TorquePenalty, Clutch);
		}

		TorqueFade *= FMath::Clamp(TorquePenalty, 0.05f, 1.f);
	}

	const float Overspeed = SpeedKmh - MaxGearSpeed;
	const float TriggerMargin = MaxGearSpeed * (GearOverSpeedPercent * 0.01f);

	if (Overspeed > 0.f && TriggerMargin > 0.f)
	{
		const float FadeRatio = FMath::Clamp(Overspeed / TriggerMargin, 0.f, 1.f);
		const float MinTorqueRatio = 1.f - (GearOverSpeedTorqueDropPercent * 0.01f);
		TorqueFade *= FMath::Lerp(1.f, MinTorqueRatio, FadeRatio);
	}

	float ClutchFactor = 1.f;
	if (bClutchEnabled)
	{
		ClutchFactor = Clutch;
		if (ClutchFactor <= 0.01f && !bAutomaticTransmission)
		{
			CachedThrottleForce = 0.f;
			CachedRPM = FMath::FInterpTo(CachedRPM, FMath::Lerp(MinRPM, MaxRPM, Gas), DeltaTime, 10.f);
			CachedRPM = FMath::Clamp(CachedRPM, MinRPM, MaxRPM);
			return;
		}
	}

	const float WheelRPM = FMath::Lerp(MinRPM, MaxRPM, SpeedRatio);

	if (ClutchFactor <= 0.01f)
	{
		const float DesiredRPM = FMath::Lerp(MinRPM, MaxRPM, Gas);
		CachedRPM = FMath::FInterpTo(CachedRPM, DesiredRPM, DeltaTime, 6.f);
	}
	else if (ClutchFactor < 0.95f)
	{
		const float SlipBlend = FMath::Clamp(FMath::Pow(1.f - ClutchFactor, 1.5f), 0.f, 1.f);
		const float ThrottleRPM = FMath::Lerp(MinRPM, MaxRPM, Gas);
		const float Target = FMath::Lerp(WheelRPM, ThrottleRPM, SlipBlend);
		CachedRPM = FMath::FInterpTo(CachedRPM, Target, DeltaTime, 8.f);
	}
	else
	{
		CachedRPM = FMath::FInterpTo(CachedRPM, WheelRPM, DeltaTime, 10.f);
	}

	CachedRPM = FMath::Clamp(CachedRPM, MinRPM, MaxRPM);
	TargetRPM = CachedRPM;

	const float GearPush = GearPushForces.IsValidIndex(CurrentGear) ? GearPushForces[CurrentGear] : 0.f;
	const float ClutchSmooth = FMath::Clamp(FMath::Pow(ClutchFactor, 1.5f), 0.f, 1.f);
	CachedThrottleForce = GearPush * Gas * TorqueFade * ClutchSmooth;

	if (bAutomaticTransmission)
	{
		if (FMath::Abs(GasPaddle) < KINDA_SMALL_NUMBER)
			return;

		if (CachedRPM >= RedLineRPM && CurrentGear < NumForwardGears)
		{
			CurrentGear++;
			PlayGearShiftSound();
		}
		else if (CachedRPM < IdealDownshiftRPM && CurrentGear > 1)
		{
			const int32 DownGear = CurrentGear - 1;
			const float DownGearSpeed = GearSpeedThresholdsKmh.IsValidIndex(DownGear) ? GearSpeedThresholdsKmh[DownGear] : MaxGearSpeed;
			if ((SpeedKmh / DownGearSpeed) < 0.8f)
			{
				CurrentGear--;
				PlayGearShiftSound();
			}
		}
		CachedGearIndex = CurrentGear;
	}

	if (SpeedKmh < 5.f && bIsHandBrake && Gas > 0.f)
	{
		CachedThrottleForce = 0.f;
	}

	const int32 GearDropAmount = PreviousGearIndex - CurrentGear;
	if (GearDropAmount > 0)
	{
		const float KickStrength = FMath::Clamp(GearDropAmount * DownshiftKickStrengthPerGear, 0.f, 5.f);
		CachedThrottleForce *= (1.f - KickStrength);

		const float MaxAllowedSpeedKmh = GearSpeedThresholdsKmh.IsValidIndex(CurrentGear)
			? GearSpeedThresholdsKmh[CurrentGear]
			: DesiredMaxSpeedForFullThrottleKmh;

		const float SpeedOverLimit = CachedSpeedKmh - MaxAllowedSpeedKmh;

		if (SpeedOverLimit > 0.f)
		{
			DownshiftTargetSpeedCm = MaxAllowedSpeedKmh * (100000.f / 3600.f);
		}
	}

	PreviousGearIndex = CurrentGear;

	if (DownshiftTargetSpeedCm > 0.f)
	{
		const FVector Velocity = VehicleMesh->GetPhysicsLinearVelocity();
		const float CurrentSpeedCm = Velocity.Size();

		if (CurrentSpeedCm > DownshiftTargetSpeedCm + 10.f)
		{
			const FVector VelocityDir = Velocity.GetSafeNormal();
			const float SpeedDiff = CurrentSpeedCm - DownshiftTargetSpeedCm;
			const float DecelForce = SpeedDiff * DownshiftBrakeStrength * VehicleMesh->GetMass();
			const FVector BrakeForce = -VelocityDir * DecelForce;
			VehicleMesh->AddForce(BrakeForce);
		}
		else
		{
			DownshiftTargetSpeedCm = -1.f;
		}
	}
}

void AVehicle::ManualUpshift()
{
	if (bClutchEnabled && Clutch > 0.05f) return;
	if (bAutomaticTransmission) return;

	const int32 MaxGear = FMath::Min(GearSpeedThresholdsKmh.Num(), GearPushForces.Num()) - 1;

	if (CachedGearIndex == -1)
	{
		PlayGearShiftSound();
		CachedGearIndex = 1; // Neutral to First
		PlayGearShiftSound();
		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Gear: 1 (from N)"));
		//}
		return;
	}

	if (CachedGearIndex < MaxGear)
	{
		CachedGearIndex++;
		PlayGearShiftSound();
		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Orange, FString::Printf(TEXT("Manual Upshift Gear %d"), CachedGearIndex));
		//}
	}
}

void AVehicle::ManualDownshift()
{
	if (bClutchEnabled && Clutch > 0.05f) return;
	if (bAutomaticTransmission) return;

	if (CachedGearIndex == -1)
	{
		CachedGearIndex = 0; // Neutral to Reverse
		PlayGearShiftSound();
		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("Gear: R (from N)"));
		//}
		return;
	}

	if (CachedGearIndex > 0)
	{
		CachedGearIndex--;
		PlayGearShiftSound();
		FColor MsgColor = CachedGearIndex == 0 ? FColor::Red : FColor::Cyan;
		FString GearLabel = CachedGearIndex == 0 ? TEXT("R") : FString::Printf(TEXT("%d"), CachedGearIndex);
		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 1.f, MsgColor, FString::Printf(TEXT("Manual Downshift Gear %s"), *GearLabel));
		//}
	}
}

void AVehicle::Neutral()
{
	if (bAutomaticTransmission) return;
	if (bClutchEnabled && Clutch > 0.05f) return; // clutch must be pressed

	CachedGearIndex = -1;
	//if (bShowDebugHUD)
	//{
	//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Switched to Neutral (Manual)"));
	//}
}

void AVehicle::OnClutch(const FInputActionInstance& Instance)
{
	const float Raw = FMath::Clamp(Instance.GetValue().Get<float>(), 0.f, 1.f);
	const float TargetClutch = 1.f - Raw;
	const float Deadzone = 0.01f;

	// === SMOOTHED CLUTCH ENGAGEMENT ===
	if (Raw <= Deadzone)
	{
		Clutch = 1.f;
		bClutchActive = false;
	}
	else if (Raw >= 1.f - Deadzone)
	{
		Clutch = 0.f;
		bClutchActive = true;
	}
	else
	{
		Clutch = FMath::FInterpTo(Clutch, TargetClutch, GetWorld()->GetDeltaSeconds(), 12.f);
		Clutch = FMath::Clamp(Clutch, 0.f, 1.f);
		bClutchActive = true;
	}

	// === DEBUG DISPLAY ===
	//if (bShowDebugHUD)
	//{
	//	const FString Status = (Clutch < 0.05f) ? TEXT("DISENGAGED") :
	//		(Clutch > 0.95f) ? TEXT("ENGAGED") : TEXT("PARTIAL");

	//	GEngine->AddOnScreenDebugMessage(9001, 0.f, FColor::Cyan, FString::Printf(TEXT("Clutch: %s [%.2f]"), *Status, Clutch));
	//}
}

void AVehicle::UpdateEngineAudio()
{
	// Do not touch loop audio while engine state is pending (start delay)
	if (bEnginePending)
	{
		return;
	}

	if (!EngineAudio)
	{
		return;
	}

	// --------------------------------
	// ENGINE OFF: hard stop immediately
	// --------------------------------
	if (!bEngineRunning)
	{
		if (EngineAudio->IsPlaying())
		{
			EngineAudio->Stop();
		}
		return;
	}

	// --------------------------------
	// ENGINE ON: ensure loop is playing
	// --------------------------------
	if (!EngineAudio->IsPlaying())
	{
		EngineAudio->Play();
	}

	// --------------------------------
	// Motorcycle reverse = almost silent
	// --------------------------------
	if (bIsMotorcycle && CachedGearIndex == 0)
	{
		EngineAudio->SetPitchMultiplier(0.75f);
		EngineAudio->SetVolumeMultiplier(0.1f);
		return;
	}

	// --------------------------------
	// RPM-based pitch & volume
	// --------------------------------
	const float EffectiveRPM =
		(bClutchEnabled && !bClutchActive)
		? CachedRPM * Clutch
		: CachedRPM;

	const float Pitch = FMath::GetMappedRangeValueClamped(
		FVector2D(MinRPM, MaxRPM),
		FVector2D(0.75f, 2.0f),
		EffectiveRPM
	);
	EngineAudio->SetPitchMultiplier(Pitch);

	const float RPMVolume = FMath::GetMappedRangeValueClamped(
		FVector2D(MinRPM, MaxRPM),
		FVector2D(0.2f, 1.0f),
		EffectiveRPM
	);

	const bool bNeutralOrDisengaged =
		(CachedGearIndex == -1) ||
		(bClutchEnabled && !bClutchActive);

	const float VolumeModifier = bNeutralOrDisengaged ? 0.5f : 1.0f;

	SmoothedVolume = 1.f;
	const float TargetVolume = RPMVolume * VolumeModifier;

	SmoothedVolume = FMath::FInterpTo(
		SmoothedVolume,
		TargetVolume,
		GetWorld()->GetDeltaSeconds(),
		5.0f
	);

	EngineAudio->SetVolumeMultiplier(SmoothedVolume);
}

void AVehicle::OnVehicleHit(UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!OtherActor || OtherActor == this || !OtherComp)
		return;

	if (bActivateAIOnHit)
	{
		if (OtherActor->ActorHasTag(AITargetTag))
		{
			bAIActive = true;
		}
	}

	const float ImpactStrength = NormalImpulse.Size();
	const float DistSq = FVector::DistSquared(LastCollisionFXLocation, Hit.ImpactPoint);
	const bool bShouldPlayCollisionFX = DistSq > FMath::Square(MinCollisionFXDistance);

	// === [0] Optional: Fall On Impact ===
	if (bLoseBalanceOnImpact && ImpactStrength >= LoseBalanceImpactPower && !bAutoBalanceSpawnProtected)
	{

		// Disable auto balance systems
		bEnableFullAutoBalance = false;
		bEnableNoseBalance = false;
		bEnableAutoBalanceLean = false;
		bBalanceToHorizon = false;
		bRealisticHillClimb = false;

		// Schedule balance reset for AI
		if (bIsAI)
		{
			FTimerHandle BalanceResetTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				BalanceResetTimerHandle,
				this,
				&AVehicle::ResetBalanceFunction,
				AIResetBalanceDelayForLostBalanceOnImpact,
				false
			);
		}

		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
		//		FString::Printf(TEXT("FallOnImpact triggered! Impact: %.0f"), ImpactStrength));
		//}
	}

	// === [1] Debug Info ===
	//if (bShowDebugHUD)
	//{
	//	GEngine->AddOnScreenDebugMessage(9999, 1.5f, FColor::Red,
	//		FString::Printf(TEXT("COLLISION: Hit %s (%s) | Impulse: %.1f"),
	//			*OtherActor->GetName(), *OtherComp->GetName(), ImpactStrength));
	//	if (!bShouldPlayCollisionFX)
	//	{
	//		GEngine->AddOnScreenDebugMessage(9998, 1.5f, FColor::Silver, TEXT("Collision skipped: Too close to last FX"));
	//	}
	//}

	// === [2] Play Sound and FX if far enough ===
	if (bShouldPlayCollisionFX)
	{
		// === [SOUND] ===
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CollisionSoundCue && CurrentTime - LastCollisionSoundTime >= CollisionSoundCooldown)
		{
			const float MinImpact = 10000.f;
			const float MaxImpact = 200000.f;

			float Volume = FMath::GetMappedRangeValueClamped(
				FVector2D(MinImpact, MaxImpact),
				FVector2D(0.1f, 1.0f),
				ImpactStrength
			);

			UGameplayStatics::PlaySoundAtLocation(
				GetWorld(),
				CollisionSoundCue,
				Hit.ImpactPoint,
				Volume,
				1.f,
				0.f,
				CollisionSoundAttenuation
			);

			LastCollisionSoundTime = CurrentTime;
		}

		// === [NIAGARA] ===
		if (CollisionFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				CollisionFX,
				Hit.ImpactPoint,
				Hit.ImpactNormal.Rotation(),
				FVector(1.f),
				true,
				true,
				ENCPoolMethod::AutoRelease
			);
		}

		// Update cached location
		LastCollisionFXLocation = Hit.ImpactPoint;
	}
}

void AVehicle::PlayGearShiftSound()
{
	if (GearShiftSoundCue)
	{
		UGameplayStatics::SpawnSoundAttached(
			GearShiftSoundCue,
			VehicleMesh,
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			false,                // bStopWhenAttachedToDestroyed
			GearShiftVolume,      // VolumeMultiplier (user-configurable)
			GearShiftPitch,       // PitchMultiplier (user-configurable)
			0.f,                  // StartTime
			GearAttenuationSettings  // Optional attenuation asset (3D distance fade)
		);
	}
}

void AVehicle::ForceSetGear(int32 NewGearIndex)
{
	if (bAutomaticTransmission)
	{
		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("ForceSetGear ignored - Auto Transmission is ON"));
		//}
		return;
	}

	if (bClutchEnabled && Clutch > 0.05f)
	{
		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("ForceSetGear failed - Clutch must be pressed"));
		//}
		return;
	}

	const int32 MaxGear = FMath::Min(GearSpeedThresholdsKmh.Num(), GearPushForces.Num()) - 1;

	// Valid gear range: -1 (Neutral), 0 (Reverse), 1 to MaxGear
	if (NewGearIndex < -1 || NewGearIndex > MaxGear)
	{
		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red,
		//		FString::Printf(TEXT("Invalid Gear Index: %d (Valid: N = -1, R = 0, 1-%d)"), NewGearIndex, MaxGear));
		//}
		return;
	}

	CachedGearIndex = NewGearIndex;
	PreviousGearIndex = NewGearIndex;
	PlayGearShiftSound();

	FString GearLabel;
	switch (NewGearIndex)
	{
	case -1: GearLabel = TEXT("N"); break;
	case  0: GearLabel = TEXT("R"); break;
	default: GearLabel = FString::FromInt(NewGearIndex); break;
	}

	//if (bShowDebugHUD)
	//{
	//	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green,
	//		FString::Printf(TEXT("Force Gear: %s"), *GearLabel));
	//}
}

void AVehicle::StartHorn()
{
	if (HornAudio && !HornAudio->IsPlaying())
	{
		HornAudio->Play();
	}
}

void AVehicle::StopHorn()
{
	if (HornAudio && HornAudio->IsPlaying())
	{
		HornAudio->Stop();
	}
}

void AVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld()) return;

	CachedSuspensions.Empty();
	GetComponents<USuspension>(CachedSuspensions);

	FlushPersistentDebugLines(World);

	for (USuspension* Suspension : CachedSuspensions)
	{
		if (!Suspension || !bShowWheelLocationOnConstruction) continue;

		const FVector SuspensionBase = Suspension->GetComponentLocation();
		const FVector Up = Suspension->GetUpVector();

		const float Height = Suspension->SuspensionHeight;
		const float RestRatio = Suspension->SuspensionRestRatio;
		const float WheelRadius = Suspension->VisualWheelRadius;

		// === Calculated Suspension Positions ===
		const FVector SuspensionTip = SuspensionBase - Up * Height;
		const FVector RestPos = SuspensionBase - Up * (Height * RestRatio);

		// === Draw Suspension Line from Component Base ===
		DrawDebugLine(World, SuspensionBase, SuspensionTip, FColor::Green, true, -1.f, 0, 2.f);
		DrawDebugSphere(World, RestPos, 4.f, 8, FColor::Yellow, true, -1.f, 0, 1.f);

		// === If we have a visual wheel mesh, draw actual position/radius ===
		if (Suspension->WheelMeshReference)
		{
			const FVector WheelPos = Suspension->WheelMeshReference->GetComponentLocation();

			DrawDebugSphere(World, WheelPos, WheelRadius, 12, FColor::Red, true, -1.f, 0, 0.5f);
			DrawDebugLine(World, SuspensionBase, WheelPos, FColor::Orange, true, -1.f, 0, 1.f);
			DrawDebugString(World, WheelPos + FVector(0.f, 0.f, 10.f), Suspension->GetName(), nullptr, FColor::White, 0.f, true);
		}
		else
		{
			// Fallback if no wheel mesh reference
			DrawDebugSphere(World, RestPos, WheelRadius, 12, FColor::Red, true, -1.f, 0, 0.5f);
			DrawDebugString(World, SuspensionBase + FVector(0.f, 0.f, 10.f), Suspension->GetName(), nullptr, FColor::White, 0.f, true);
		}
	}
#endif
}

void AVehicle::ToggleTrailer()
{
	if (!TrailerConstraint || !TrailerHitchPoint || !CanTow)
		return;

	if (bTrailerAttached)
	{
		AActor* ConstrainedActor = TrailerConstraint->ConstraintActor2;

		TrailerConstraint->BreakConstraint();
		bTrailerAttached = false;

		if (AVehicle* OtherVehicle = Cast<AVehicle>(TrailerReference))
			OtherVehicle->bBeingTowed = false;
		else if (ConstrainedActor)
			ConstrainedActor->Tags.Remove("IsTowed");

		//if (bShowDebugHUD)
		//{
		//	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Trailer detached."));
		//}
		return;
	}


	// === LINE TRACE ===
	FVector Center = GetActorLocation();
	FVector Hitch = TrailerHitchPoint->GetComponentLocation();

	FHitResult LineHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (bShowDebugLines)
	{
		DrawDebugLine(GetWorld(), Center, Hitch, FColor::Cyan, false, 2.f, 0, 2.f);
	}

	if (GetWorld()->LineTraceSingleByChannel(LineHit, Center, Hitch, ECC_PhysicsBody, Params))
	{
		AActor* HitActor = LineHit.GetActor();
		if (HitActor && HitActor->Tags.Contains("Towable") && !HasXYZTag(HitActor))
		{
			TrailerReference = HitActor;
			AttachTrailer(HitActor, LineHit.ImpactPoint);
			return;
		}
	}

	// === SPHERE TRACE ===
	TArray<FHitResult> Hits;
	const float SphereRadius = HookDetectionRadius;
	FVector SphereCenter = TrailerHitchPoint->GetComponentLocation();

	if (bShowDebugLines)
	{
		DrawDebugSphere(GetWorld(), SphereCenter, SphereRadius, 12, FColor::Magenta, false, 2.f);
	}

	if (GetWorld()->SweepMultiByChannel(Hits, SphereCenter, SphereCenter, FQuat::Identity,
		ECC_PhysicsBody, FCollisionShape::MakeSphere(SphereRadius), Params))
	{
		for (const FHitResult& Hit : Hits)
		{
			AActor* Candidate = Hit.GetActor();
			if (Candidate && Candidate->Tags.Contains("Towable") && HasXYZTag(Candidate))
			{
				FVector HookWorld;
				if (ExtractXYZWorldLocation(Candidate, HookWorld))
				{
					float Dist = FVector::Dist(HookWorld, SphereCenter);
					if (Dist <= SphereRadius)
					{
						AttachTrailer(Candidate, HookWorld);
						return;
					}
				}
			}
		}
	}

	//if (bShowDebugHUD)
	//{
	//	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("No suitable trailer found."));
	//}
}

void AVehicle::AttachTrailer(AActor* HitActor, const FVector& AttachFrom)
{
	if (!HitActor || !TrailerConstraint || !TrailerHitchPoint || !CanTow)
		return;

	UPrimitiveComponent* MyRoot = Cast<UPrimitiveComponent>(GetRootComponent());
	UPrimitiveComponent* OtherRoot = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
	if (!MyRoot || !OtherRoot)
		return;

	// === Check for XYZ and teleport if needed ===
	if (HasXYZTag(HitActor))
	{
		FVector HookWorld;
		if (ExtractXYZWorldLocation(HitActor, HookWorld))
		{
			// Calculate offset from current position to match XYZ to hitch
			const FVector Delta = TrailerHitchPoint->GetComponentLocation() - HookWorld;
			HitActor->AddActorWorldOffset(Delta, false, nullptr, ETeleportType::TeleportPhysics);
			//if (bShowDebugHUD)
			//{
			//	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Teleported to align XYZ point."));
			//}

		}
	}

	// === Constraint Setup ===
	TrailerConstraint->SetConstrainedComponents(MyRoot, NAME_None, OtherRoot, NAME_None);
	bTrailerAttached = true;

	// === Mark the other actor as being towed ===
	if (AVehicle* OtherVehicle = Cast<AVehicle>(HitActor))
	{
		OtherVehicle->bBeingTowed = true;
	}
	else
	{
		HitActor->Tags.AddUnique("IsTowed");
	}

	//if (bShowDebugHUD)
	//{
	//	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Trailer attached."));
	//}
}

bool AVehicle::HasXYZTag(AActor* Actor)
{
	for (const FName& Tag : Actor->Tags)
	{
		const FString TagStr = Tag.ToString();
		if (TagStr.StartsWith("(") && TagStr.EndsWith(")"))
			return true;
	}
	return false;
}

bool AVehicle::ExtractXYZWorldLocation(AActor* Actor, FVector& OutWorldLocation)
{
	for (const FName& Tag : Actor->Tags)
	{
		const FString TagStr = Tag.ToString();
		if (TagStr.StartsWith("(") && TagStr.EndsWith(")"))
		{
			FVector LocalOffset;
			if (FParse::Value(*TagStr, TEXT("X="), LocalOffset.X) &&
				FParse::Value(*TagStr, TEXT("Y="), LocalOffset.Y) &&
				FParse::Value(*TagStr, TEXT("Z="), LocalOffset.Z))
			{
				// Convert local to world using actor's transform
				OutWorldLocation = Actor->GetTransform().TransformPosition(LocalOffset);
				return true;
			}
		}
	}
	return false;
}

USplineComponent* AVehicle::GetAISpline() const
{
	if (!AISplineActor) return nullptr;
	return AISplineActor->FindComponentByClass<USplineComponent>();
}

float AVehicle::Clamp01(float v) { return FMath::Clamp(v, 0.f, 1.f); }

void AVehicle::AI_ApplyInputs(float SteerCmd, float Throttle01, float Brake01)
{
	Steer(FInputActionValue(FMath::Clamp(SteerCmd, -1.f, 1.f)));

	Throttle01 = Clamp01(Throttle01);
	Brake01 = Clamp01(Brake01);

	if (Brake01 > KINDA_SMALL_NUMBER)
	{
		MoveForward(FInputActionValue(0.f));
		Reverse(FInputActionValue(Brake01));
	}
	else
	{
		Reverse(FInputActionValue(0.f));
		MoveForward(FInputActionValue(Throttle01));
	}
}

float AVehicle::AI_ComputeSteerToPoint(const FVector& WorldPoint) const
{
	if (!VehicleMesh) return 0.f;

	const FVector MyLoc = GetActorLocation();
	FVector To = (WorldPoint - MyLoc);
	To.Z = 0.f;
	if (To.IsNearlyZero()) return 0.f;

	FVector Fwd = VehicleMesh->GetForwardVector();
	Fwd.Z = 0.f;
	Fwd = Fwd.GetSafeNormal();

	const FVector Dir = To.GetSafeNormal();
	const float CrossZ = FVector::CrossProduct(Fwd, Dir).Z;
	const float Dot = FVector::DotProduct(Fwd, Dir);

	float SteerCmd = FMath::Clamp(CrossZ * 2.5f, -1.f, 1.f);
	if (Dot < -0.15f) SteerCmd = (CrossZ >= 0.f) ? 1.f : -1.f;

	return SteerCmd;
}
