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
class UAnimationAsset;
class UVRHandPresenceComponent;
class UPoseableMeshComponent;
class UVehicleSoundComponent;
class UChaosWheeledVehicleMovementComponent;
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

	/** Pose the mesh hands hold. They never let go of the rim, so one pose is enough */
	UPROPERTY(EditAnywhere, Category="VR")
	TObjectPtr<UAnimationAsset> GripHandPose;

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

	/** Handle reset vehicle input by input actions or mobile interface */
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

	/** Checks if the car is flipped upside down and automatically resets it */
	UFUNCTION()
	void FlippedCheck();

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
