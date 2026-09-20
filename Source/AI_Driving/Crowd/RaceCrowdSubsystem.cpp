#include "Crowd/RaceCrowdSubsystem.h"
#include "Crowd/RaceCrowdStand.h"
#include "RaceDirectorSubsystem.h"
#include "RaceParticipantComponent.h"
#include "RacingAITypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaceCrowd, Log, All);

URaceCrowdSubsystem* URaceCrowdSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URaceCrowdSubsystem>() : nullptr;
}

TStatId URaceCrowdSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URaceCrowdSubsystem, STATGROUP_Tickables);
}

void URaceCrowdSubsystem::Deinitialize()
{
	if (URaceDirectorSubsystem* Director = BoundDirector.Get())
	{
		Director->OnRaceStarted.RemoveDynamic(this, &URaceCrowdSubsystem::HandleRaceStarted);
		Director->OnRacerFinished.RemoveDynamic(this, &URaceCrowdSubsystem::HandleRacerFinished);
		Director->OnRaceReset.RemoveDynamic(this, &URaceCrowdSubsystem::HandleRaceReset);
	}

	Stands.Empty();
	LastPosition.Empty();

	Super::Deinitialize();
}

void URaceCrowdSubsystem::RegisterStand(ARaceCrowdStand* Stand)
{
	if (Stand)
	{
		Stands.AddUnique(Stand);
	}
}

void URaceCrowdSubsystem::UnregisterStand(ARaceCrowdStand* Stand)
{
	Stands.RemoveAll([Stand](const TWeakObjectPtr<ARaceCrowdStand>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == Stand;
	});
}

void URaceCrowdSubsystem::BindToDirector()
{
	URaceDirectorSubsystem* Director = GetWorld() ? GetWorld()->GetSubsystem<URaceDirectorSubsystem>() : nullptr;

	if (!Director)
	{
		return;
	}

	Director->OnRaceStarted.AddDynamic(this, &URaceCrowdSubsystem::HandleRaceStarted);
	Director->OnRacerFinished.AddDynamic(this, &URaceCrowdSubsystem::HandleRacerFinished);
	Director->OnRaceReset.AddDynamic(this, &URaceCrowdSubsystem::HandleRaceReset);

	BoundDirector = Director;
	bBound = true;
}

void URaceCrowdSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bBound)
	{
		BindToDirector();
	}

	if (Stands.Num() == 0)
	{
		return;
	}

	EventPulse = FMath::Max(EventPulse - EventPulseDecay * DeltaTime, 0.0f);

	Accumulator += DeltaTime;

	if (Accumulator < UpdateInterval)
	{
		return;
	}

	UpdateCrowd(Accumulator);
	Accumulator = 0.0f;
}

void URaceCrowdSubsystem::UpdateCrowd(float DeltaTime)
{
	URaceDirectorSubsystem* Director = GetWorld() ? GetWorld()->GetSubsystem<URaceDirectorSubsystem>() : nullptr;

	if (Director && Director->GetRaceState() == ERaceState::Racing)
	{
		DetectOvertakes(*Director);
	}

	for (const TWeakObjectPtr<ARaceCrowdStand>& Weak : Stands)
	{
		ARaceCrowdStand* Stand = Weak.Get();

		if (!Stand)
		{
			continue;
		}

		float Target;

		if (ExcitementOverride >= 0.0f)
		{
			Target = FMath::Clamp(ExcitementOverride, 0.0f, 1.0f);
		}
		else
		{
			Target = Director ? ComputeLocalExcitement(*Stand, *Director) : IdleExcitement;
		}

		// rising and falling at different rates is most of what makes this read as people rather
		// than as a volume slider: a crowd catches on to something in a moment and takes a long
		// time to settle afterwards
		const float Current = Stand->GetExcitement();
		const float Rate = Target > Current ? RiseRate : FallRate;

		Stand->SetExcitement(FMath::FInterpConstantTo(Current, Target, DeltaTime, Rate));
	}
}

float URaceCrowdSubsystem::ComputeLocalExcitement(const ARaceCrowdStand& Stand, URaceDirectorSubsystem& Director) const
{
	float Base;

	switch (Director.GetRaceState())
	{
	case ERaceState::Countdown: Base = CountdownExcitement; break;
	case ERaceState::Racing:    Base = RacingExcitement;    break;
	case ERaceState::Finished:  Base = FinishExcitement;    break;
	default:                    Base = IdleExcitement;      break;
	}

	const float Radius = FMath::Max(Stand.ReactionRadius, 1.0f);

	// what is in front of this stand right now: the closest car, how fast it is going, and
	// whether anyone is close enough behind it to be a fight rather than a procession
	float BestProximity = 0.0f;
	float BestSpeedFactor = 0.0f;
	float BestDistanceAlong = 0.0f;
	bool bFoundCar = false;

	TArray<URaceParticipantComponent*> Participants = Director.GetParticipantsByPosition();

	for (const URaceParticipantComponent* Participant : Participants)
	{
		const AActor* Owner = Participant ? Participant->GetOwner() : nullptr;

		if (!Owner)
		{
			continue;
		}

		// to the nearest seat, not to the actor. On a hundred metres of grandstand the origin
		// can be fifty metres from the part the car is actually passing, which would have the
		// far end of the stand reacting and the near end ignoring it
		const float Distance = FVector::Dist(Owner->GetActorLocation(), Stand.GetClosestPointTo(Owner->GetActorLocation()));

		if (Distance > Radius)
		{
			continue;
		}

		const float Proximity = 1.0f - (Distance / Radius);

		if (Proximity > BestProximity)
		{
			BestProximity = Proximity;
			BestSpeedFactor = FMath::Clamp(FMath::Abs(Participant->Progress.ForwardSpeed) / ExcitingSpeed, 0.0f, 1.0f);
			BestDistanceAlong = Participant->Progress.TotalDistance;
			bFoundCar = true;
		}
	}

	float Excitement = Base + PassBoost * BestProximity * BestSpeedFactor;

	if (bFoundCar)
	{
		// measured along the track rather than through the air: two cars side by side on
		// opposite sides of a hairpin are metres apart and not racing each other at all
		for (const URaceParticipantComponent* Participant : Participants)
		{
			if (!Participant || !Participant->GetOwner())
			{
				continue;
			}

			const float Along = FMath::Abs(Participant->Progress.TotalDistance - BestDistanceAlong);

			if (Along > KINDA_SMALL_NUMBER && Along < BattleGap)
			{
				Excitement += BattleBoost * BestProximity;
				break;
			}
		}
	}

	return FMath::Clamp(Excitement + EventPulse, 0.0f, 1.0f);
}

void URaceCrowdSubsystem::DetectOvertakes(URaceDirectorSubsystem& Director)
{
	for (URaceParticipantComponent* Participant : Director.GetParticipantsByPosition())
	{
		if (!Participant || Participant->bFinished)
		{
			continue;
		}

		const int32 Position = Participant->Progress.Position;
		const int32* Previous = LastPosition.Find(Participant);

		// a position that improved means this car got past somebody. The car that lost the place
		// reports the mirror of the same event, so only gains are counted or every pass is two
		if (Previous && Position > 0 && Position < *Previous)
		{
			EventPulse = FMath::Min(EventPulse + EventPulseStrength, 1.0f);

			if (const AActor* Owner = Participant->GetOwner())
			{
				ReactNearest(Owner->GetActorLocation(), 0.8f);
			}
		}

		LastPosition.Add(Participant, Position);
	}
}

void URaceCrowdSubsystem::ReactNearest(const FVector& Location, float Intensity)
{
	ARaceCrowdStand* Nearest = nullptr;
	float BestDistance = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<ARaceCrowdStand>& Weak : Stands)
	{
		ARaceCrowdStand* Stand = Weak.Get();

		if (!Stand)
		{
			continue;
		}

		const float Distance = FVector::Dist(Stand->GetClosestPointTo(Location), Location);

		if (Distance < BestDistance && Distance <= Stand->ReactionRadius)
		{
			BestDistance = Distance;
			Nearest = Stand;
		}
	}

	// nothing happens if it was out of everyone's earshot. A cheer from an empty hillside for
	// something nobody could see is worse than silence
	if (Nearest)
	{
		Nearest->PlayReaction(Intensity, Location);
	}
}

void URaceCrowdSubsystem::HandleRaceStarted()
{
	EventPulse = FMath::Min(EventPulse + EventPulseStrength, 1.0f);
	LastPosition.Empty();
}

void URaceCrowdSubsystem::HandleRacerFinished(URaceParticipantComponent* Participant, int32 FinishPosition)
{
	// the winner is worth more than the rest of the field arriving afterwards
	const float Intensity = FinishPosition <= 1 ? 1.0f : 0.6f;
	EventPulse = FMath::Min(EventPulse + EventPulseStrength * Intensity, 1.0f);

	if (Participant)
	{
		if (const AActor* Owner = Participant->GetOwner())
		{
			ReactNearest(Owner->GetActorLocation(), Intensity);
		}
	}
}

void URaceCrowdSubsystem::HandleRaceReset()
{
	EventPulse = 0.0f;
	LastPosition.Empty();
}

void URaceCrowdSubsystem::LogState() const
{
	const URaceDirectorSubsystem* Director = GetWorld() ? GetWorld()->GetSubsystem<URaceDirectorSubsystem>() : nullptr;

	UE_LOG(LogRaceCrowd, Display,
		TEXT("Crowd: %d stands  pulse %.2f  director %s%s"),
		Stands.Num(), EventPulse,
		Director ? TEXT("yes") : TEXT("MISSING"),
		ExcitementOverride >= 0.0f
			? *FString::Printf(TEXT("  OVERRIDE PINNED AT %.2f"), ExcitementOverride)
			: TEXT(""));

	for (const TWeakObjectPtr<ARaceCrowdStand>& Weak : Stands)
	{
		if (const ARaceCrowdStand* Stand = Weak.Get())
		{
			UE_LOG(LogRaceCrowd, Display, TEXT("    %s"), *Stand->DescribeState());
		}
	}
}

// --- Console commands ---
//
// A crowd is judged entirely by ear, and the two questions that come up first are "does the bed
// react to excitement at all" and "why is it doing that". Pinning the value answers the first
// without having to stage a race; the dump answers the second.

namespace
{
	URaceCrowdSubsystem* GetCrowd(const UWorld* World)
	{
		return World ? World->GetSubsystem<URaceCrowdSubsystem>() : nullptr;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GCrowdExcitement(
	TEXT("crowd.Excitement"),
	TEXT("crowd.Excitement <0..1 | -1> - pins every grandstand to a fixed excitement, or -1 to hand it back to the race. Pin it to 1 to hear whether the beds react at all."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		URaceCrowdSubsystem* Crowd = GetCrowd(World);

		if (Args.Num() < 1 || !Crowd)
		{
			UE_LOG(LogRaceCrowd, Warning, TEXT("Usage: crowd.Excitement <0..1 | -1>"));
			return;
		}

		Crowd->ExcitementOverride = FCString::Atof(*Args[0]);

		UE_LOG(LogRaceCrowd, Display, TEXT("crowd.Excitement: %s across %d stands."),
			Crowd->ExcitementOverride < 0.0f
				? TEXT("released")
				: *FString::Printf(TEXT("pinned to %.2f"), Crowd->ExcitementOverride),
			Crowd->GetStandCount());
	}));

static FAutoConsoleCommandWithWorld GCrowdDumpState(
	TEXT("crowd.DumpState"),
	TEXT("Writes every grandstand's excitement, level and assigned sounds to the log."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (const URaceCrowdSubsystem* Crowd = GetCrowd(World))
		{
			Crowd->LogState();
		}
	}));
