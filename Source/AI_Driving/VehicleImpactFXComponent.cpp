// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleImpactFXComponent.h"

#include "AI_Driving.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UVehicleImpactFXComponent::UVehicleImpactFXComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UVehicleImpactFXComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		// Free when it never fires, and an exact contact point when it does.
		Owner->OnActorHit.AddDynamic(this, &UVehicleImpactFXComponent::HandleActorHit);

		// A body says nothing about its collisions unless asked to.
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Owner->GetComponents(Primitives);

		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive && Primitive->IsSimulatingPhysics())
			{
				Primitive->SetNotifyRigidBodyCollision(true);
			}
		}
	}
}

void UVehicleImpactFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnActorHit.RemoveDynamic(this, &UVehicleImpactFXComponent::HandleActorHit);
	}

	Super::EndPlay(EndPlayReason);
}

void UVehicleImpactFXComponent::HandleActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!SelfActor)
	{
		return;
	}

	// From here on the velocity path can stop sweeping: the engine is telling us where contact is.
	bHitEventsWorking = true;

	const FVector RelativeVelocity = SelfActor->GetVelocity() - (OtherActor ? OtherActor->GetVelocity() : FVector::ZeroVector);

	// How fast the surfaces closed, not how fast the car was going. Sliding along a barrier at
	// 200 km/h is a scrape; meeting it head on at 40 is a crash.
	const float ClosingSpeed = FMath::Abs(FVector::DotProduct(RelativeVelocity, Hit.ImpactNormal));

	if (ClosingSpeed < MinImpactSpeed)
	{
		return;
	}

	// Contact is reported every frame while the car stays against the wall, so a scrape has to be
	// paced. Without this a single rub along a barrier spawns a system per frame, which is both a
	// solid sheet of sparks and a lot of Niagara instances.
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	if (Now - LastSparkTime < ScrapeInterval)
	{
		return;
	}

	LastSparkTime = Now;

	const float Severity = FMath::Clamp(
		(ClosingSpeed - MinImpactSpeed) / FMath::Max(MaxImpactSpeed - MinImpactSpeed, 1.f), 0.f, 1.f);

	SpawnSparks(Hit.ImpactPoint, Hit.ImpactNormal, Severity);
}

void UVehicleImpactFXComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();

	if (!Owner || !SparkSystem || bHitEventsWorking || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Velocity = Owner->GetVelocity();
	const FVector Location = Owner->GetActorLocation();

	if (!bHasPreviousFrame)
	{
		PreviousVelocity = Velocity;
		PreviousLocation = Location;
		bHasPreviousFrame = true;

		return;
	}

	const FVector VelocityChange = Velocity - PreviousVelocity;
	const float DistanceMoved = FVector::Dist(Location, PreviousLocation);

	// A respawn zeroes velocity and moves the car, which reads exactly like a wall. Real motion
	// covers roughly speed times delta time; a jump far past that was a teleport.
	const float PlausibleDistance = PreviousVelocity.Size() * DeltaTime * 2.f + 100.f;
	const bool bTeleported = DistanceMoved > PlausibleDistance;

	PreviousVelocity = Velocity;
	PreviousLocation = Location;

	if (bTeleported)
	{
		return;
	}

	// Braking and cornering slow a car too. Only a pull harder than driving can manage is contact.
	if (VelocityChange.Size() < ContactDeceleration * DeltaTime)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Contact reports arrive every frame while two bodies stay touching, so a scrape is paced
	// rather than throwing a burst per frame.
	const double Now = World->GetTimeSeconds();

	if (Now - LastSparkTime < ScrapeInterval)
	{
		return;
	}

	LastSparkTime = Now;

	ReportImpactFromVelocity(VelocityChange);
}

void UVehicleImpactFXComponent::ReportImpactFromVelocity(const FVector& VelocityChange)
{
	// One frame of a collision, not the whole collision: this is a rate, so it is compared against
	// the per-second thresholds after scaling by the pacing interval.
	const float FrameSpeedLost = VelocityChange.Size();

	FVector ContactLocation = FVector::ZeroVector;
	FVector ContactNormal = FVector::ZeroVector;

	// The impulse pushed the car along the change in velocity, so what it hit is the other way.
	if (!FindContactPoint(-VelocityChange.GetSafeNormal(), ContactLocation, ContactNormal))
	{
		return;
	}

	const float Severity = FMath::Clamp(
		(FrameSpeedLost - MinImpactSpeed * ScrapeInterval)
			/ FMath::Max((MaxImpactSpeed - MinImpactSpeed) * ScrapeInterval, 1.f), 0.f, 1.f);

	SpawnSparks(ContactLocation, ContactNormal, Severity);
}

bool UVehicleImpactFXComponent::FindContactPoint(const FVector& PushDirection, FVector& OutLocation, FVector& OutNormal) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!Owner || !World || PushDirection.IsNearlyZero())
	{
		return false;
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	Owner->GetActorBounds(true, Origin, Extent);

	const float Reach = Extent.Size2D() + ContactSearchMargin;
	const FVector End = Origin + PushDirection * Reach;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VehicleImpactFX), false, Owner);
	Params.bReturnPhysicalMaterial = false;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_Vehicle);

	FHitResult Hit;
	const bool bFound = World->SweepSingleByObjectType(
		Hit, Origin, End, FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(ContactSearchRadius), Params);

	if (bDrawDebug)
	{
		DrawDebugLine(World, Origin, End, bFound ? FColor::Green : FColor::Red, false, 2.f, 0, 2.f);
	}

	if (!bFound)
	{
		return false;
	}

	OutLocation = Hit.ImpactPoint;
	OutNormal = Hit.ImpactNormal;

	if (bDrawDebug)
	{
		DrawDebugSphere(World, OutLocation, 15.f, 8, FColor::Yellow, false, 2.f);
	}

	return true;
}

void UVehicleImpactFXComponent::SpawnSparks(const FVector& Location, const FVector& Normal, float Severity)
{
	if (!SparkSystem)
	{
		return;
	}

	UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, SparkSystem, Location, Normal.Rotation(), FVector::OneVector, true, true);

	if (Spawned && !SeverityParameterName.IsNone())
	{
		Spawned->SetVariableFloat(SeverityParameterName, Severity);
	}

	UE_LOG(LogAI_Driving, Verbose, TEXT("Sparks at %s (severity %.2f) on %s"),
		*Location.ToCompactString(), Severity, *GetNameSafe(GetOwner()));
}
