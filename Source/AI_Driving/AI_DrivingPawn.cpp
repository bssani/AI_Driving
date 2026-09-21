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
#include "Animation/Skeleton.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "VRHandPresenceComponent.h"
#include "VehicleImpactFXComponent.h"
#include "ChaosBrakeReverseGuardComponent.h"
#include "RaceParticipantComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
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

	// construct the hands that sit on the rim. Which mesh they wear, where they sit and how they
	// are posed are all properties, so the whole hand can be replaced from the vehicle Blueprint
	// without touching code. What is set below is only the default: the hand from the VR Steering
	// asset pack, placed where that pack places it.
	LeftGripHand = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Left Grip Hand"));
	LeftGripHand->SetupAttachment(SteeringWheelMesh);
	LeftGripHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightGripHand = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Right Grip Hand"));
	RightGripHand->SetupAttachment(SteeringWheelMesh);
	RightGripHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// No mesh, no pose and no offset by default: the grip hands are set up in the vehicle
	// Blueprint. Code that picks a hand for you is code that has to be undone before you can use
	// your own, and a placement worked out for somebody else's hand is wrong for every other one
	// - the pivot, the bone orientation and the finger spread all differ.
	//
	// For reference, the values that fitted the SK_MannequinHand_Right from the VR_SteeringV2
	// pack, in case that hand is what you want back:
	//   Left  rotation (2.44, -11.39, 29.26)  location (-10.16, -15.43, -1.72)  scale (1, 1, -1)
	//   Right rotation (0, 0, -163.88)        location (10.16, 15.46, 2.48)     scale (1, 1, 1)
	// The left scale of -1 on Z mirrors a right hand, which is what that pack needed because it
	// ships no left one. A mesh that is already a left hand wants a scale of 1.

	ApplyGripHandSetup();

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

	// sparks on contact. Finding the contact point is its own problem here: async substepping
	// means no hit events reach the game thread, so it looks for the surface itself.
	ImpactFX = CreateDefaultSubobject<UVehicleImpactFXComponent>(TEXT("Impact FX"));

	// braking at speed must not drop the gearbox into reverse. It turns itself off on cars nobody
	// is driving, since the AI holds the brake at the grid and would otherwise roll backwards
	BrakeReverseGuard = CreateDefaultSubobject<UChaosBrakeReverseGuardComponent>(TEXT("Brake Reverse Guard"));

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

void AAI_DrivingPawn::ApplyGripHandSetup()
{
	struct FGripHandSetup
	{
		USkeletalMeshComponent* Component;
		USkeletalMesh* Mesh;
		FTransform Offset;
		UClass* AnimClass;
		UAnimationAsset* Pose;
	};

	const FGripHandSetup Hands[] =
	{
		{ LeftGripHand, LeftGripHandMesh, LeftGripHandOffset, LeftGripHandAnimClass.Get(), LeftGripHandPose },
		{ RightGripHand, RightGripHandMesh, RightGripHandOffset, RightGripHandAnimClass.Get(), RightGripHandPose }
	};

	for (const FGripHandSetup& Hand : Hands)
	{
		// the components are thrown away on every car nobody is sitting in, so they can be gone
		if (!Hand.Component)
		{
			continue;
		}

		if (Hand.Component->GetSkeletalMeshAsset() != Hand.Mesh)
		{
			Hand.Component->SetSkeletalMeshAsset(Hand.Mesh);
		}

		Hand.Component->SetRelativeTransform(Hand.Offset);

		if (Hand.AnimClass)
		{
			Hand.Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);

			if (Hand.Component->GetAnimClass() != Hand.AnimClass)
			{
				Hand.Component->SetAnimInstanceClass(Hand.AnimClass);
			}
		}
		else if (Hand.Pose)
		{
			// a pose built on another skeleton is silently dropped by PlayAnimation and the hand
			// shows its reference pose instead, which reads as "my grip animation did nothing".
			// Say so rather than leaving it to be guessed at.
			if (Hand.Mesh && Hand.Pose->GetSkeleton() != Hand.Mesh->GetSkeleton())
			{
				UE_LOG(LogAI_Driving, Warning, TEXT("Grip pose '%s' is built on skeleton '%s' but hand mesh '%s' uses '%s'. The hand will stay in its reference pose. Retarget the animation onto the mesh's skeleton, or drive the hand with an Animation Blueprint instead."),
					*GetNameSafe(Hand.Pose), *GetNameSafe(Hand.Pose->GetSkeleton()), *GetNameSafe(Hand.Mesh), *GetNameSafe(Hand.Mesh->GetSkeleton()));
			}
			else
			{
				// a single node player is enough, since these hands never let go of the rim
				Hand.Component->PlayAnimation(Hand.Pose, true);
			}
		}

#if WITH_EDITOR
		// pose the hands in the editor viewport as well, so a new hand can be lined up with the
		// rim by eye instead of by starting the game and looking through a headset
		Hand.Component->SetUpdateAnimationInEditor(true);
#endif
	}
}

void AAI_DrivingPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyGripHandSetup();
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

	// hold the grip pose. OnConstruction has already done this, but a hand swapped from a
	// construction script or from another Blueprint would land after it
	ApplyGripHandSetup();

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

void AAI_DrivingPawn::DiscardDriverRig()
{
	// Hand tracking reports one person's hands, so these components have nothing to represent on
	// an AI car. Hiding them was not enough: the tracked hand components poll the runtime and pose
	// a skeletal mesh every frame regardless of visibility, once per hand per car. Nor is there a
	// driver body for the mesh hands to belong to, so a car without them also reads better than one
	// gripping its wheel with a disembodied pair.
	USceneComponent* const DriverOnly[] = { LeftTrackedHand, RightTrackedHand, LeftGripHand, RightGripHand };

	for (USceneComponent* Component : DriverOnly)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}

	LeftTrackedHand = nullptr;
	RightTrackedHand = nullptr;
	LeftGripHand = nullptr;
	RightGripHand = nullptr;

	if (HandPresence)
	{
		HandPresence->DestroyComponent();
		HandPresence = nullptr;
	}
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

	// Only the car the headset wearer sits in needs hands. Deciding this at BeginPlay does not
	// work: the game mode spawns the player's pawn and possesses it afterwards, so at BeginPlay
	// even the driver's own car still looks unpossessed. By the first tick possession has landed.
	if (!bDriverRigResolved)
	{
		bDriverRigResolved = true;

		if (!IsPlayerControlled() || !IsLocallyControlled())
		{
			DiscardDriverRig();
		}
	}

	UpdateBrakeLights();

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
	// the lights are not touched here. They follow the vehicle from Tick instead, which is the
	// only way the AI cars get any: they never run these handlers
}

void AAI_DrivingPawn::DoBrakeStop()
{
	// reset brake input to zero
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void AAI_DrivingPawn::DoHandbrakeStart()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(true);
}

void AAI_DrivingPawn::DoHandbrakeStop()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(false);
}

void AAI_DrivingPawn::UpdateBrakeLights()
{
	if (!ChaosVehicleMovement)
	{
		return;
	}

	// it takes more pedal to light them than to keep them lit, so a foot resting on the switch
	// settles on an answer instead of strobing
	const float Threshold = bBrakeLightsOn
		? BrakeLightThreshold * BrakeLightReleaseRatio
		: BrakeLightThreshold;

	const bool bBraking = ChaosVehicleMovement->GetBrakeInput() > Threshold
		|| ChaosVehicleMovement->GetHandbrakeInput();

	if (bBraking == bBrakeLightsOn)
	{
		return;
	}

	bBrakeLightsOn = bBraking;

	// the Blueprint hook is unchanged, so whatever already drives the lamps keeps working
	BrakeLights(bBraking);
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
	// on a race track, put the car back on the track. Righting a car where it sits leaves a car
	// wedged against a barrier still wedged against the barrier
	if (URaceParticipantComponent* Participant = FindComponentByClass<URaceParticipantComponent>())
	{
		// holding on the grid before the start: moving the car would break the grid, but the
		// driver can still straighten their view
		if (Participant->IsInputLocked())
		{
			DoRecenterVR();
			return;
		}

		if (Participant->RespawnOnTrack())
		{
			// the car jumps in an instant. Starting black and fading in hides the jump from the driver
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				if (PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.4f, FLinearColor::Black, false, false);
				}
			}

			DoRecenterVR();
			return;
		}
	}

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