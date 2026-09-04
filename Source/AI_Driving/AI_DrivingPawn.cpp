// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI_DrivingPawn.h"
#include "AI_DrivingWheelFront.h"
#include "AI_DrivingWheelRear.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AI_Driving.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimationAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "VRHandPresenceComponent.h"
#include "Components/PoseableMeshComponent.h"

#if WITH_VEHICLE_SOUND
#include "Components/VehicleSoundComponent.h"
#endif

#if WITH_METAXR_HANDS
#include "OculusXRHandComponent.h"
#endif

#define LOCTEXT_NAMESPACE "VehiclePawn"

AAI_DrivingPawn::AAI_DrivingPawn()
{
	// construct the front camera boom
	FrontSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Front Spring Arm"));
	FrontSpringArm->SetupAttachment(GetMesh());
	FrontSpringArm->TargetArmLength = 0.0f;
	FrontSpringArm->bDoCollisionTest = false;
	FrontSpringArm->bEnableCameraRotationLag = true;
	FrontSpringArm->CameraRotationLagSpeed = 15.0f;
	FrontSpringArm->SetRelativeLocation(FVector(30.0f, 0.0f, 120.0f));

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Front Camera"));
	FrontCamera->SetupAttachment(FrontSpringArm);
	FrontCamera->bAutoActivate = false;

	// construct the back camera boom
	BackSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Back Spring Arm"));
	BackSpringArm->SetupAttachment(GetMesh());
	BackSpringArm->TargetArmLength = 650.0f;
	BackSpringArm->SocketOffset.Z = 150.0f;
	BackSpringArm->bDoCollisionTest = false;
	BackSpringArm->bInheritPitch = false;
	BackSpringArm->bInheritRoll = false;
	BackSpringArm->bEnableCameraRotationLag = true;
	BackSpringArm->CameraRotationLagSpeed = 2.0f;
	BackSpringArm->CameraLagMaxDistance = 50.0f;

	BackCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Back Camera"));
	BackCamera->SetupAttachment(BackSpringArm);

	// construct the seated driver position. No spring arm and no lag: any smoothing between the
	// car and the headset reads as the world sliding around the driver, which is what makes
	// people sick. This is a starting point for the sports car; tune it per vehicle Blueprint.
	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VR Origin"));
	VROrigin->SetupAttachment(GetMesh());
	VROrigin->SetRelativeLocation(FVector(15.0f, -38.0f, 110.0f));

	// construct the VR camera. It replaces its own relative transform with the headset pose each
	// frame, so it has to sit under VROrigin rather than carry the eye offset itself.
	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VR Camera"));
	VRCamera->SetupAttachment(VROrigin);
	VRCamera->bAutoActivate = false;
	VRCamera->bLockToHmd = true;

	// construct the steering wheel. Placed by eye for the sports car: this has to be moved in the
	// vehicle Blueprint until it sits exactly where the physical rim is, because a driver reaching
	// for a wheel that isn't where they see it loses the illusion instantly. If the vehicle mesh
	// already models a wheel, clear the mesh here and point HandPresence at that one instead.
	SteeringWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Steering Wheel"));
	SteeringWheelMesh->SetupAttachment(GetMesh());
	// SteeringMesh is a disc in its own XY plane with the thickness along Z, so its face has to
	// be stood up: -90 points it back at the driver, and the 20 back off that is the column tilt.
	// Position checked against the sports car in the editor rather than guessed.
	SteeringWheelMesh->SetRelativeLocation(FVector(68.0f, -38.0f, 85.0f));
	SteeringWheelMesh->SetRelativeRotation(FRotator(-70.0f, 0.0f, 0.0f));
	SteeringWheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMesh(TEXT("/Game/VR_SteeringV2/VR_Steering/Meshes/SteeringMesh.SteeringMesh"));

	if (WheelMesh.Succeeded())
	{
		SteeringWheelMesh->SetStaticMesh(WheelMesh.Object);
	}

	// construct the hands that sit on the rim. Only a right hand ships with the asset pack, so the
	// left is the same mesh with its Y scale negated, which turns a right hand into a left one.
	// Mirroring this way leaves the normal map handed the wrong way round, which on a hand gripping
	// a rim is not worth authoring a second mesh over.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> HandMesh(TEXT("/Game/VR_SteeringV2/VR_Steering/Hands/Mesh/SK_MannequinHand_Right.SK_MannequinHand_Right"));

	LeftGripHand = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Left Grip Hand"));
	LeftGripHand->SetupAttachment(SteeringWheelMesh);
	// These come from BP_SteeringBase in the VR Steering asset pack rather than from guesswork:
	// its authored hand pivot and mesh transforms composed down into one, with the grip points it
	// uses at 18cm out from the hub. Note the left hand is the right hand mirrored on Z, not Y.
	LeftGripHand->SetRelativeLocation(FVector(-10.16f, -15.43f, -1.72f));
	LeftGripHand->SetRelativeRotation(FRotator(2.44f, -11.39f, 29.26f));
	LeftGripHand->SetRelativeScale3D(FVector(1.0f, 1.0f, -1.0f));
	LeftGripHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightGripHand = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Right Grip Hand"));
	RightGripHand->SetupAttachment(SteeringWheelMesh);
	RightGripHand->SetRelativeLocation(FVector(10.16f, 15.46f, 2.48f));
	RightGripHand->SetRelativeRotation(FRotator(0.0f, 0.0f, -163.88f));
	RightGripHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (HandMesh.Succeeded())
	{
		LeftGripHand->SetSkeletalMesh(HandMesh.Object);
		RightGripHand->SetSkeletalMesh(HandMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> GripPose(TEXT("/Game/VR_SteeringV2/VR_Steering/Hands/Animations/MannequinHand_Right_Grab.MannequinHand_Right_Grab"));

	if (GripPose.Succeeded())
	{
		GripHandPose = GripPose.Object;
	}

	// construct the hands drawn from headset tracking. These hang off VROrigin like the camera
	// does, so they share its tracking space. The Meta XR component fetches the runtime hand mesh
	// on its own, which is why there's no mesh to assign here.
#if WITH_METAXR_HANDS
	UOculusXRHandComponent* MetaLeftHand = CreateDefaultSubobject<UOculusXRHandComponent>(TEXT("Left Tracked Hand"));
	UOculusXRHandComponent* MetaRightHand = CreateDefaultSubobject<UOculusXRHandComponent>(TEXT("Right Tracked Hand"));

	MetaLeftHand->SkeletonType = EOculusXRHandType::HandLeft;
	MetaLeftHand->MeshType = EOculusXRHandType::HandLeft;
	MetaRightHand->SkeletonType = EOculusXRHandType::HandRight;
	MetaRightHand->MeshType = EOculusXRHandType::HandRight;

	// leave visibility alone: HandPresence is the only thing that should decide when a hand shows,
	// and two systems fighting over it would make the hands flicker
	MetaLeftHand->ConfidenceBehavior = EOculusXRConfidenceBehavior::None;
	MetaRightHand->ConfidenceBehavior = EOculusXRConfidenceBehavior::None;

	LeftTrackedHand = MetaLeftHand;
	RightTrackedHand = MetaRightHand;
#else
	LeftTrackedHand = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Left Tracked Hand"));
	RightTrackedHand = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Right Tracked Hand"));
#endif

	LeftTrackedHand->SetupAttachment(VROrigin);
	LeftTrackedHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightTrackedHand->SetupAttachment(VROrigin);
	RightTrackedHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// construct the vehicle audio. It finds the Chaos movement component itself and reads RPM,
	// speed, gear and wheel slip from it, so there's nothing to wire up here. Assign a sound
	// preset in the vehicle Blueprint to give it something to play.
#if WITH_VEHICLE_SOUND
	VehicleSound = CreateDefaultSubobject<UVehicleSoundComponent>(TEXT("Vehicle Sound"));
#endif

	// construct the hand swapper and point it at what it drives
	HandPresence = CreateDefaultSubobject<UVRHandPresenceComponent>(TEXT("VR Hand Presence"));
	HandPresence->SetWheelAndGripHands(SteeringWheelMesh, LeftGripHand, RightGripHand);
	HandPresence->SetTrackedHands(LeftTrackedHand, RightTrackedHand);

	// Configure the car mesh
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// get the Chaos Wheeled movement component
	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

}

void AAI_DrivingPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// steering 
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &AAI_DrivingPawn::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &AAI_DrivingPawn::Steering);

		// throttle 
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AAI_DrivingPawn::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AAI_DrivingPawn::Throttle);

		// break 
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &AAI_DrivingPawn::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &AAI_DrivingPawn::StartBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &AAI_DrivingPawn::StopBrake);

		// handbrake 
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &AAI_DrivingPawn::StartHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &AAI_DrivingPawn::StopHandbrake);

		// look around 
		EnhancedInputComponent->BindAction(LookAroundAction, ETriggerEvent::Triggered, this, &AAI_DrivingPawn::LookAround);

		// toggle camera 
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &AAI_DrivingPawn::ToggleCamera);

		// reset the vehicle 
		EnhancedInputComponent->BindAction(ResetVehicleAction, ETriggerEvent::Triggered, this, &AAI_DrivingPawn::ResetVehicle);

		// recenter the VR view. Optional action, so only bind it if one has been assigned
		if (RecenterVRAction)
		{
			EnhancedInputComponent->BindAction(RecenterVRAction, ETriggerEvent::Started, this, &AAI_DrivingPawn::RecenterVR);
		}
	}
	else
	{
		UE_LOG(LogAI_Driving, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AAI_DrivingPawn::BeginPlay()
{
	Super::BeginPlay();

	bVRModeActive = IsVRRenderingForWorld(GetWorld());

	if (bVRModeActive)
	{
		// seated experience. The tracking origin sits where the driver's head starts out, not on the floor
		GEngine->XRSystem->SetTrackingOrigin(EHMDTrackingOrigin::Local);

		// hand the view to the headset camera
		FrontCamera->SetActive(false);
		BackCamera->SetActive(false);
		VRCamera->SetActive(true);

		// not DoRecenterVR() here: at BeginPlay the headset usually has no pose yet and the
		// recenter is silently dropped ("Could not retrieve a valid head pose for recentering").
		// Tick retries until tracking is actually up.
		bRecenterPending = true;
	}

	// hold the grip pose. A single node player is enough, since these hands never let go
	if (GripHandPose)
	{
		LeftGripHand->PlayAnimation(GripHandPose, true);
		RightGripHand->PlayAnimation(GripHandPose, true);
	}

	if (!bVRModeActive)
	{
		// the wheel and the hands exist for the driver's own view. On a flat screen the chase
		// camera would just see a pair of hands floating in the cabin
		SteeringWheelMesh->SetVisibility(false, true);
		LeftTrackedHand->SetVisibility(false, true);
		RightTrackedHand->SetVisibility(false, true);
		HandPresence->SetComponentTickEnabled(false);
	}

	// set up the flipped check timer
	GetWorld()->GetTimerManager().SetTimer(FlipCheckTimer, this, &AAI_DrivingPawn::FlippedCheck, FlipCheckTime, true);
}

void AAI_DrivingPawn::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// clear the flipped check timer
	GetWorld()->GetTimerManager().ClearTimer(FlipCheckTimer);

	Super::EndPlay(EndPlayReason);
}

void AAI_DrivingPawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// add some angular damping if the vehicle is in midair
	bool bMovingOnGround = ChaosVehicleMovement->IsMovingOnGround();
	GetMesh()->SetAngularDamping(bMovingOnGround ? 0.0f : 3.0f);

	// the headset's pose is not valid for the first few frames of play, so the opening recenter
	// waits here rather than being dropped at BeginPlay
	if (bRecenterPending && GEngine && GEngine->XRSystem.IsValid()
		&& GEngine->XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId))
	{
		DoRecenterVR();
		bRecenterPending = false;
	}

	// realign the camera yaw to face front. The chase camera is unused in VR
	if (!bVRModeActive)
	{
		float CameraYaw = BackSpringArm->GetRelativeRotation().Yaw;
		CameraYaw = FMath::FInterpTo(CameraYaw, 0.0f, Delta, 1.0f);

		BackSpringArm->SetRelativeRotation(FRotator(0.0f, CameraYaw, 0.0f));
	}
}

void AAI_DrivingPawn::Steering(const FInputActionValue& Value)
{
	// route the input
	DoSteering(Value.Get<float>());
}

void AAI_DrivingPawn::Throttle(const FInputActionValue& Value)
{
	// route the input
	DoThrottle(Value.Get<float>());
}

void AAI_DrivingPawn::Brake(const FInputActionValue& Value)
{
	// route the input
	DoBrake(Value.Get<float>());
}

void AAI_DrivingPawn::StartBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStart();
}

void AAI_DrivingPawn::StopBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStop();
}

void AAI_DrivingPawn::StartHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStart();
}

void AAI_DrivingPawn::StopHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStop();
}

void AAI_DrivingPawn::LookAround(const FInputActionValue& Value)
{
	// route the input
	DoLookAround(Value.Get<float>());
}

void AAI_DrivingPawn::ToggleCamera(const FInputActionValue& Value)
{
	// route the input
	DoToggleCamera();
}

void AAI_DrivingPawn::ResetVehicle(const FInputActionValue& Value)
{
	// route the input
	DoResetVehicle();
}

void AAI_DrivingPawn::RecenterVR(const FInputActionValue& Value)
{
	// route the input
	DoRecenterVR();
}

void AAI_DrivingPawn::DoSteering(float SteeringValue)
{
	// add the input
	ChaosVehicleMovement->SetSteeringInput(SteeringValue);
}

void AAI_DrivingPawn::DoThrottle(float ThrottleValue)
{
	// add the input
	ChaosVehicleMovement->SetThrottleInput(ThrottleValue);

	// reset the brake input
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void AAI_DrivingPawn::DoBrake(float BrakeValue)
{
	// add the input
	ChaosVehicleMovement->SetBrakeInput(BrakeValue);

	// reset the throttle input
	ChaosVehicleMovement->SetThrottleInput(0.0f);
}

void AAI_DrivingPawn::DoBrakeStart()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(true);
}

void AAI_DrivingPawn::DoBrakeStop()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(false);

	// reset brake input to zero
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void AAI_DrivingPawn::DoHandbrakeStart()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(true);

	// call the Blueprint hook for the break lights
	BrakeLights(true);
}

void AAI_DrivingPawn::DoHandbrakeStop()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(false);

	// call the Blueprint hook for the break lights
	BrakeLights(false);
}

void AAI_DrivingPawn::DoLookAround(float YawDelta)
{
	// rotate the spring arm
	BackSpringArm->AddLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void AAI_DrivingPawn::DoToggleCamera()
{
	// the headset owns the view in VR, so there is nothing to toggle to
	if (bVRModeActive)
	{
		return;
	}

	// toggle the active camera flag
	bFrontCameraActive = !bFrontCameraActive;

	FrontCamera->SetActive(bFrontCameraActive);
	BackCamera->SetActive(!bFrontCameraActive);
}

void AAI_DrivingPawn::DoResetVehicle()
{
	// reset to a location slightly above our current one
	FVector ResetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	// reset to our yaw. Ignore pitch and roll
	FRotator ResetRotation = GetActorRotation();
	ResetRotation.Pitch = 0.0f;
	ResetRotation.Roll = 0.0f;

	// teleport the actor to the reset spot and reset physics
	SetActorTransform(FTransform(ResetRotation, ResetLocation, FVector::OneVector), false, nullptr, ETeleportType::TeleportPhysics);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);

	// bring the driver's view back in line with the car
	DoRecenterVR();
}

bool AAI_DrivingPawn::IsVRRenderingForWorld(const UWorld* World)
{
	// IsHeadTrackingAllowedForWorld is the world-aware check: it returns false for the extra
	// non-VR windows VR Preview spawns alongside the headset view
	return World && GEngine && GEngine->XRSystem.IsValid()
		&& GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*const_cast<UWorld*>(World));
}

void AAI_DrivingPawn::DoRecenterVR()
{
	if (bVRModeActive && GEngine && GEngine->XRSystem.IsValid())
	{
		// zero yaw, so the recentered forward direction is the VR camera's own forward: straight down the car
		GEngine->XRSystem->ResetOrientationAndPosition(0.0f);
	}
}

void AAI_DrivingPawn::FlippedCheck()
{
	// check the difference in angle between the mesh's up vector and world up
	const float UpDot = FVector::DotProduct(FVector::UpVector, GetMesh()->GetUpVector());

	if (UpDot < FlipCheckMinDot)
	{
		// is this the second time we've checked that the vehicle is still flipped?
		if (bPreviousFlipCheck)
		{
			// reset the vehicle to upright
			DoResetVehicle();
		}
		
		// set the flipped check flag so the next check resets the car
		bPreviousFlipCheck = true;

	} else {

		// we're upright. reset the flipped check flag
		bPreviousFlipCheck = false;
	}
}

#undef LOCTEXT_NAMESPACE