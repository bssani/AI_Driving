// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleImpactFXComponent.generated.h"

class UNiagaraSystem;

/**
 *  Spawns sparks where a vehicle hits something.
 *
 *  Hit events are the main route and they do arrive, async substepping or not, as long as the
 *  physics bodies have SetNotifyRigidBodyCollision on - which the sound plugin and this component
 *  both switch on for the owner. They carry an exact contact point and surface normal.
 *
 *  Where they don't arrive, the collision is spotted the way the sound plugin spots it - as a loss
 *  of speed too sharp for driving to explain - and the contact point is worked out: the impulse
 *  pushed the car along the change in velocity, so whatever it hit is in the opposite direction,
 *  and a short sweep that way lands on the surface.
 *
 *  What counts as worth showing is not what counts as worth hearing. A crash is judged on how fast
 *  the surfaces closed, but sparks come from grinding, so a contact sparks when the surfaces are
 *  sliding past each other as well - otherwise a car brushing down a barrier throws nothing and
 *  the only thing that sparks is a square hit.
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
	 *  Sliding speed (cm/s) along a surface below which a contact does not spark.
	 *
	 *  Sparks come from grinding, not from crumpling. Judging them only on how fast the surfaces
	 *  closed - which is what the crash sound is judged on - means a car brushing down a barrier
	 *  throws nothing, because sliding along a wall has almost no closing speed at all. Then the
	 *  only thing that sparks is a square hit, which in a race is mostly car on car.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "0.0"))
	float MinScrapeSpeed = 300.f;

	/** Sliding speed (cm/s) that counts as a full-severity scrape */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact FX", meta = (ClampMin = "1.0"))
	float MaxScrapeSpeed = 4000.f;

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
	float ScrapeInterval = 0.12f;

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

	/** Decides whether a contact is worth showing and throws the sparks. Both routes end here so
	 *  they judge a contact the same way */
	void ReportContact(const FVector& Location, const FVector& Normal, const FVector& RelativeVelocity);

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
