// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "AI_DrivingPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimationAsset;
class UAnimInstance;
class UVRHandPresenceComponent;
class UPoseableMeshComponent;
class UVehicleSoundComponent;
class UChaosWheeledVehicleMovementComponent;
class UVehicleImpactFXComponent;
class UChaosBrakeReverseGuardComponent;
struct FInputActionValue;

/**
 *  Vehicle Pawn class
 *  Handles common functionality for all vehicle types,
 *  including input handling and camera management.
 *  
 *  Specific vehicle configurations are handled in subclasses.
 */
UCLASS(abstract)
class AAI_DrivingPawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()

	/** Spring Arm for the front camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* FrontSpringArm;

	/** Front Camera component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FrontCamera;

	/** Spring Arm for the back camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* BackSpringArm;

	/** Back Camera component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* BackCamera;

	/** Driver's seated head position. VRCamera overwrites its own relative transform with the raw
	 *  headset pose every frame, so the eye offset has to live on this parent instead */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USceneComponent* VROrigin;

	/** Camera driven by the head mounted display. Rigidly attached to the mesh so the view tracks the car exactly */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* VRCamera;

	/** Steering wheel the driver sees. Where a physical wheel exists this only has to line up
	 *  with it; the hands hang off this so they turn with it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* SteeringWheelMesh;

	/** Hand shown while the driver is holding the wheel. The asset pack only ships a right hand,
	 *  so this one is the same mesh mirrored across the wheel's left-right axis */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* LeftGripHand;

	/** Hand shown while the driver is holding the wheel */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* RightGripHand;

	/** Hand drawn from headset tracking while it is off the wheel. Where the Meta XR plugin is
	 *  present this is one of its hand components, which loads the runtime hand mesh itself */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UPoseableMeshComponent* LeftTrackedHand;

	/** Hand drawn from headset tracking while it is off the wheel */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UPoseableMeshComponent* RightTrackedHand;

	/** Engine, tyre, wind and impact audio. Reads its own state off the Chaos movement component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UVehicleSoundComponent* VehicleSound;

	/** Decides which hand representation the driver sees, and turns the wheel */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UVRHandPresenceComponent* HandPresence;

	/** Sparks where the car hits things. Assign a Niagara system on it to see anything */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UVehicleImpactFXComponent* ImpactFX;

	/** Keeps the brake from selecting reverse while the car is still moving. Otherwise braking at
	 *  speed spins the engine through the reverse ratio and the engine note shoots up */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UChaosBrakeReverseGuardComponent* BrakeReverseGuard;

	/** Cast pointer to the Chaos Vehicle movement component */
	TObjectPtr<UChaosWheeledVehicleMovementComponent> ChaosVehicleMovement;

protected:

	/** Steering Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SteeringAction;

	/** Throttle Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ThrottleAction;

	/** Brake Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* BrakeAction;

	/** Handbrake Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* HandbrakeAction;

	/** Look Around Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAroundAction;

	/** Toggle Camera Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ToggleCameraAction;

	/** Reset Vehicle Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ResetVehicleAction;

	/** Recenter VR View Action. Optional; the view is also recentered whenever the vehicle is reset */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* RecenterVRAction;

	/** Keeps track of which camera is active */
	bool bFrontCameraActive = false;

	/** Mesh shown in place of the driver's left hand while they hold the wheel. Leave it empty to
	 *  show no hand on that side. Any skeletal mesh will do: nothing in code assumes the hand that
	 *  ships with the steering asset pack */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	TObjectPtr<USkeletalMesh> LeftGripHandMesh;

	/** Mesh shown in place of the driver's right hand while they hold the wheel */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	TObjectPtr<USkeletalMesh> RightGripHandMesh;

	/** Where the left hand sits, relative to the steering wheel. A negative scale mirrors the
	 *  mesh, which is what turns a right hand into a left one; set the scale to 1 when the mesh is
	 *  already a left hand. The default mirrors, because the asset pack only ships a right hand */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	FTransform LeftGripHandOffset;

	/** Where the right hand sits, relative to the steering wheel */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	FTransform RightGripHandOffset;

	/** Animation Blueprint the left hand runs. Set this when the grip is driven by a graph, for a
	 *  hand that reacts to input; leave it empty to hold the single pose below instead */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	TSubclassOf<UAnimInstance> LeftGripHandAnimClass;

	/** Animation Blueprint the right hand runs */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	TSubclassOf<UAnimInstance> RightGripHandAnimClass;

	/** Pose the left hand holds when it has no Animation Blueprint. The hands never let go of the
	 *  rim, so a single pose is enough. It has to be built on the same skeleton as the mesh above,
	 *  or it is ignored and the hand shows its reference pose */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	TObjectPtr<UAnimationAsset> LeftGripHandPose;

	/** Pose the right hand holds when it has no Animation Blueprint */
	UPROPERTY(EditAnywhere, Category="VR|Grip Hands")
	TObjectPtr<UAnimationAsset> RightGripHandPose;

	/** Pushes the properties above onto the two hand components. Runs on construction as well as
	 *  at BeginPlay, so a hand swapped in the Blueprint can be lined up in the viewport */
	void ApplyGripHandSetup();

	/** True while this pawn is driving a head mounted display */
	bool bVRModeActive = false;

	/** Set while we still owe the driver a recenter. The headset usually has no valid pose yet
	 *  when play begins, so the first attempt has to wait for tracking to come up */
	bool bRecenterPending = false;

	/** Cleared once we have decided whether this car carries a driver, so the check runs once */
	bool bDriverRigResolved = false;

	/** Throws away the hands and the component that swaps them. Called on every car the headset
	 *  wearer is not sitting in, which is all of them but one */
	void DiscardDriverRig();

	/** Keeps track of whether the car is flipped. If this is true for two flip checks, resets the vehicle automatically */
	bool bPreviousFlipCheck = false;

	/** Time between automatic flip checks */
	UPROPERTY(EditAnywhere, Category="Flip Check", meta = (Units = "s"))
	float FlipCheckTime = 3.0f;

	/** Minimum dot product value for the vehicle's up direction that we still consider upright */
	UPROPERTY(EditAnywhere, Category="Flip Check")
	float FlipCheckMinDot = -0.2f;

	/** Flip check timer */
	FTimerHandle FlipCheckTimer;

public:
	AAI_DrivingPawn();

	// Begin Pawn interface

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	// End Pawn interface

	// Begin Actor interface

	/** Applies the Blueprint's hand setup, so hands can be placed with the viewport open */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Update */
	virtual void Tick(float Delta) override;

	// End Actor interface

protected:

	/** Handles steering input */
	void Steering(const FInputActionValue& Value);

	/** Handles throttle input */
	void Throttle(const FInputActionValue& Value);

	/** Handles brake input */
	void Brake(const FInputActionValue& Value);

	/** Handles brake start/stop inputs */
	void StartBrake(const FInputActionValue& Value);
	void StopBrake(const FInputActionValue& Value);

	/** Handles handbrake start/stop inputs */
	void StartHandbrake(const FInputActionValue& Value);
	void StopHandbrake(const FInputActionValue& Value);

	/** Handles look around input */
	void LookAround(const FInputActionValue& Value);

	/** Handles toggle camera input */
	void ToggleCamera(const FInputActionValue& Value);

	/** Handles reset vehicle input */
	void ResetVehicle(const FInputActionValue& Value);

	/** Handles recenter VR view input */
	void RecenterVR(const FInputActionValue& Value);

public:

	/** Handle steering input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSteering(float SteeringValue);

	/** Handle throttle input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoThrottle(float ThrottleValue);

	/** Handle brake input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrake(float BrakeValue);

	/** Handle brake start input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStart();

	/** Handle brake stop input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStop();

	/** Handle handbrake start input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStart();

	/** Handle handbrake stop input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStop();

	/** Handle look input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoLookAround(float YawDelta);

	/** Handle toggle camera input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoToggleCamera();

	/** Handle reset vehicle input by input actions or mobile interface. On a race track this puts
	 *  the car back onto the nearest point of the track; elsewhere it rights the car where it sits.
	 *  Either way the VR view is recentered */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoResetVehicle();

	/** Returns true if the given world is rendering to a headset. False for the extra non-VR
	 *  windows VR Preview spawns. Static so callers don't have to wait on this pawn's BeginPlay */
	static bool IsVRRenderingForWorld(const UWorld* World);

	/** Realigns the headset's forward direction with the car. Does nothing outside VR */
	UFUNCTION(BlueprintCallable, Category="VR")
	void DoRecenterVR();

	/** Returns true if this pawn is currently rendering to a head mounted display */
	UFUNCTION(BlueprintPure, Category="VR")
	bool IsVRModeActive() const { return bVRModeActive; }

protected:

	/** Called when the brake lights are turned on or off */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void BrakeLights(bool bBraking);

	/**
	 * Brake input at which the lights come on, like the switch on a real pedal: a touch is
	 * enough, and lifting off the throttle is not braking however hard the car slows down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BrakeLightThreshold = 0.05f;

	/**
	 * How far the pedal has to come back up before the lights go out, as a fraction of the
	 * threshold. A foot resting right on the switch would otherwise flicker the lights at frame
	 * rate, and an analogue pedal is never perfectly still.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BrakeLightReleaseRatio = 0.6f;

	/** Whether the brake lights are currently lit. Driven from the vehicle, not from input */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	bool AreBrakeLightsOn() const { return bBrakeLightsOn; }

	/** Checks if the car is flipped upside down and automatically resets it */
	UFUNCTION()
	void FlippedCheck();

	/**
	 * Lights the brake lamps from what the car is actually being asked to do.
	 *
	 * Read from the movement component rather than from the input handlers, because the input
	 * handlers only run for a human. The AI drives through IRacingVehicleInput, which lands in
	 * the same SetBrakeInput, so watching the vehicle covers both and watching the pedal events
	 * covers only one - which is why the AI cars used to brake in the dark.
	 */
	void UpdateBrakeLights();

	bool bBrakeLightsOn = false;

public:
	/** Returns the front spring arm subobject */
	FORCEINLINE USpringArmComponent* GetFrontSpringArm() const { return FrontSpringArm; }
	/** Returns the front camera subobject */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FrontCamera; }
	/** Returns the back spring arm subobject */
	FORCEINLINE USpringArmComponent* GetBackSpringArm() const { return BackSpringArm; }
	/** Returns the back camera subobject */
	FORCEINLINE UCameraComponent* GetBackCamera() const { return BackCamera; }
	/** Returns the VR origin subobject */
	FORCEINLINE USceneComponent* GetVROrigin() const { return VROrigin; }
	/** Returns the VR camera subobject */
	FORCEINLINE UCameraComponent* GetVRCamera() const { return VRCamera; }
	/** Returns the steering wheel subobject */
	FORCEINLINE UStaticMeshComponent* GetSteeringWheelMesh() const { return SteeringWheelMesh; }
	/** Returns the hand presence subobject */
	FORCEINLINE UVRHandPresenceComponent* GetHandPresence() const { return HandPresence; }
	/** Returns the cast Chaos Vehicle Movement subobject */
	FORCEINLINE const TObjectPtr<UChaosWheeledVehicleMovementComponent>& GetChaosVehicleMovement() const { return ChaosVehicleMovement; }
};
