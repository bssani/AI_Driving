// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleImpactFXComponent.generated.h"

class UNiagaraSystem;

/**
 *  Spawns sparks where a vehicle hits something.
 *
 *  Finding out that a collision happened is the easy half. The hard half is finding out *where*,
 *  and this project cannot do it the usual way: physics runs with async substepping
 *  (bSubsteppingAsync in DefaultEngine.ini), and with that on, OnActorHit does not reach the game
 *  thread. There is no FHitResult to read a contact point out of.
 *
 *  So the collision is spotted the way the sound plugin spots it - as a loss of speed too sharp
 *  for driving to explain - and then the contact point is found on purpose: the impulse pushed the
 *  car along the change in velocity, so whatever it hit is in the opposite direction. A short
 *  sweep that way lands on the surface, and the sparks go there, facing out along its normal.
 *
 *  The hit event is still subscribed to. It costs nothing when it never fires, and if substepping
 *  is ever turned off it gives a contact point straight away without a sweep.
 */
UCLASS(ClassGroup = "Vehicle", meta = (BlueprintSpawnableComponent, DisplayName = "Vehicle Impact FX"))
class AI_DRIVING_API UVehicleImpactFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleImpactFXComponent();

	/** Sparks to spawn at the contact point. Nothing happens until one is assigned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX")
	TObjectPtr<UNiagaraSystem> SparkSystem;

	/**
	 *  Float user parameter on the system that receives how hard the hit was, 0 to 1.
	 *
	 *  Leave it matching a parameter in the Niagara system to make a heavy crash throw more
	 *  sparks than a kerb clip. If the system has no such parameter this does nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX")
	FName SeverityParameterName = TEXT("Severity");

	/** Speed lost (cm/s) below which a collision is not worth showing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 250.f;

	/** Speed lost (cm/s) that counts as a full-severity crash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "1.0"))
	float MaxImpactSpeed = 1600.f;

	/**
	 *  Deceleration (cm/s^2) above which a frame is treated as contact rather than driving.
	 *
	 *  Braking and cornering also slow a car down. This has to sit above what the vehicle can do
	 *  on its own or every corner throws sparks. Roughly 2 G is a good starting point.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "100.0"))
	float ContactDeceleration = 2000.f;

	/** Keep throwing sparks while a scrape continues, this often (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "0.02", Units = "s"))
	float ScrapeInterval = 0.08f;

	/** How far past the vehicle's own bounds to look for the surface it hit (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "0.0"))
	float ContactSearchMargin = 120.f;

	/** Radius of the sphere used to find the contact point (cm). Too small and it misses corners */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "1.0"))
	float ContactSearchRadius = 25.f;

	/** Draws the sweep and the contact point it found */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX")
	bool bDrawDebug = false;

	/** Spawns sparks at a world point. Exposed so a vehicle that resolves its own contacts can use it */
	UFUNCTION(BlueprintCallable, Category = "Impact FX")
	void SpawnSparks(const FVector& Location, const FVector& Normal, float Severity);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/** Looks for what the vehicle hit, given the direction the impulse pushed it */
	bool FindContactPoint(const FVector& PushDirection, FVector& OutLocation, FVector& OutNormal) const;

	/** Sparks at the contact found by sweeping, or nothing if no surface is there */
	void ReportImpactFromVelocity(const FVector& VelocityChange);

	FVector PreviousVelocity = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	bool bHasPreviousFrame = false;

	/** Contact has been going on since this time, so a scrape can be paced */
	double LastSparkTime = 0.0;

	/** True while the game thread is receiving hit events, so the sweep is not needed */
	bool bHitEventsWorking = false;
};
