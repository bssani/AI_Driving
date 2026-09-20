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

	float Base = IdleExcitement;
	FPlayerFocus Focus;

	if (Director)
	{
		switch (Director->GetRaceState())
		{
		case ERaceState::Countdown: Base = CountdownExcitement; break;
		case ERaceState::Racing:    Base = RacingExcitement;    break;
		case ERaceState::Finished:  Base = FinishExcitement;    break;
		default:                    Base = IdleExcitement;      break;
		}

		if (bCheerPositionChanges && Director->GetRaceState() == ERaceState::Racing)
		{
			DetectPlayerPositionChange(*Director);
		}

		// once, not once per stand: where the player is and what they are doing is the same
		// answer for every grandstand on the circuit
		Focus = GatherPlayerFocus(*Director);
	}

	for (const TWeakObjectPtr<ARaceCrowdStand>& Weak : Stands)
	{
		ARaceCrowdStand* Stand = Weak.Get();

		if (!Stand)
		{
			continue;
		}

		const float Target = ExcitementOverride >= 0.0f
			? FMath::Clamp(ExcitementOverride, 0.0f, 1.0f)
			: ComputeLocalExcitement(*Stand, Base, Focus);

		// rising and falling at different rates is most of what makes this read as people rather
		// than as a volume slider: a crowd catches on to something in a moment and takes a long
		// time to settle afterwards
		const float Current = Stand->GetExcitement();
		const float Rate = Target > Current ? RiseRate : FallRate;

		Stand->SetExcitement(FMath::FInterpConstantTo(Current, Target, DeltaTime, Rate));
	}
}

URaceCrowdSubsystem::FPlayerFocus URaceCrowdSubsystem::GatherPlayerFocus(URaceDirectorSubsystem& Director) const
{
	FPlayerFocus Focus;

	const URaceParticipantComponent* Player = Director.GetPlayerParticipant();
	const AActor* PlayerActor = Player ? Player->GetOwner() : nullptr;

	// No player is not an error - a level can be opened without one - but there is then nothing
	// for the crowd to watch, so every stand sits at the baseline for the race state.
	if (!PlayerActor)
	{
		return Focus;
	}

	Focus.bValid = true;
	Focus.Location = PlayerActor->GetActorLocation();
	Focus.SpeedFactor = FMath::Clamp(FMath::Abs(Player->Progress.ForwardSpeed) / ExcitingSpeed, 0.0f, 1.0f);

	// measured along the track rather than through the air: a car on the far side of a hairpin is
	// metres away and not racing the player at all
	for (const URaceParticipantComponent* Other : Director.GetParticipantsByPosition())
	{
		if (!Other || Other == Player || !Other->GetOwner())
		{
			continue;
		}

		if (FMath::Abs(Other->Progress.TotalDistance - Player->Progress.TotalDistance) < BattleGap)
		{
			Focus.bInBattle = true;
			break;
		}
	}

	return Focus;
}

float URaceCrowdSubsystem::ComputeLocalExcitement(const ARaceCrowdStand& Stand, float Base, const FPlayerFocus& Focus) const
{
	if (!Focus.bValid)
	{
		return FMath::Clamp(Base + EventPulse, 0.0f, 1.0f);
	}

	// to the nearest seat, not to the actor. On a hundred metres of grandstand the origin can be
	// fifty metres from the part the player is actually passing, which would have the far end
	// reacting and the near end ignoring them
	const float Radius = FMath::Max(Stand.ReactionRadius, 1.0f);
	const float Distance = FVector::Dist(Focus.Location, Stand.GetClosestPointTo(Focus.Location));

	if (Distance > Radius)
	{
		return FMath::Clamp(Base + EventPulse, 0.0f, 1.0f);
	}

	const float Proximity = 1.0f - (Distance / Radius);

	// the product, not the sum: a car parked in front of the stand is not exciting, and a car at
	// full speed two hundred metres away is not this stand's business
	float Excitement = Base + PassBoost * Proximity * Focus.SpeedFactor;

	if (Focus.bInBattle)
	{
		Excitement += BattleBoost * Proximity;
	}

	return FMath::Clamp(Excitement + EventPulse, 0.0f, 1.0f);
}

void URaceCrowdSubsystem::DetectPlayerPositionChange(URaceDirectorSubsystem& Director)
{
	const URaceParticipantComponent* Player = Director.GetPlayerParticipant();

	if (!Player || Player->bFinished)
	{
		return;
	}

	const int32 Position = Player->Progress.Position;

	if (Position <= 0)
	{
		return;
	}

	// only the player's own places are events. An AI passing another AI three corners away is
	// not something the driver saw, heard, or will ever know happened
	if (LastPlayerPosition > 0 && Position != LastPlayerPosition)
	{
		const bool bGained = Position < LastPlayerPosition;
		const float Scale = bGained ? 1.0f : OvertakenPulseScale;

		EventPulse = FMath::Min(EventPulse + EventPulseStrength * Scale, 1.0f);

		if (const AActor* Owner = Player->GetOwner())
		{
			ReactNearest(Owner->GetActorLocation(), bGained ? 0.9f : 0.5f);
		}
	}

	LastPlayerPosition = Position;
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
	// the position is cleared either way, so that switching position changes on mid-session does
	// not immediately fire on the difference between now and whenever it was last looked at
	LastPlayerPosition = 0;

	if (bCheerRaceStart)
	{
		EventPulse = FMath::Min(EventPulse + EventPulseStrength, 1.0f);
	}
}

void URaceCrowdSubsystem::HandleRacerFinished(URaceParticipantComponent* Participant, int32 FinishPosition)
{
	// only the player crossing the line is an event. The AI arriving afterwards is bookkeeping,
	// and cheering it while the driver is still out on track tells them a story about somebody
	// else at the exact moment their own is still running
	if (!bCheerPlayerFinish || !Participant || !Participant->bIsPlayer)
	{
		return;
	}

	// winning is worth more than finishing
	const float Intensity = FinishPosition <= 1 ? 1.0f : 0.7f;
	EventPulse = FMath::Min(EventPulse + EventPulseStrength * Intensity, 1.0f);

	if (const AActor* Owner = Participant->GetOwner())
	{
		ReactNearest(Owner->GetActorLocation(), Intensity);
	}
}

void URaceCrowdSubsystem::HandleRaceReset()
{
	EventPulse = 0.0f;
	LastPlayerPosition = 0;
}

void URaceCrowdSubsystem::LogState() const
{
	const URaceDirectorSubsystem* Director = GetWorld() ? GetWorld()->GetSubsystem<URaceDirectorSubsystem>() : nullptr;

	UE_LOG(LogRaceCrowd, Display,
		TEXT("Crowd: %d stands  pulse %.2f  director %s  cheer[overtake %d start %d finish %d]%s"),
		Stands.Num(), EventPulse,
		Director ? TEXT("yes") : TEXT("MISSING"),
		bCheerPositionChanges ? 1 : 0, bCheerRaceStart ? 1 : 0, bCheerPlayerFinish ? 1 : 0,
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

static FAutoConsoleCommandWithWorldAndArgs GCrowdCheer(
	TEXT("crowd.Cheer"),
	TEXT("crowd.Cheer <overtake|start|finish> <0|1> - whether the crowd treats that as an event. Overtakes are off by default: most of a real grandstand cannot see the pass and hears about it from the big screen. No argument lists the current settings."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		URaceCrowdSubsystem* Crowd = GetCrowd(World);

		if (!Crowd)
		{
			return;
		}

		if (Args.Num() < 2)
		{
			UE_LOG(LogRaceCrowd, Display, TEXT("crowd.Cheer: overtake %d  start %d  finish %d"),
				Crowd->bCheerPositionChanges ? 1 : 0,
				Crowd->bCheerRaceStart ? 1 : 0,
				Crowd->bCheerPlayerFinish ? 1 : 0);
			UE_LOG(LogRaceCrowd, Display, TEXT("Usage: crowd.Cheer <overtake|start|finish> <0|1>"));
			return;
		}

		const bool bOn = FCString::Atoi(*Args[1]) != 0;
		const FString Which = Args[0].ToLower();

		if (Which == TEXT("overtake"))
		{
			Crowd->bCheerPositionChanges = bOn;
		}
		else if (Which == TEXT("start"))
		{
			Crowd->bCheerRaceStart = bOn;
		}
		else if (Which == TEXT("finish"))
		{
			Crowd->bCheerPlayerFinish = bOn;
		}
		else
		{
			UE_LOG(LogRaceCrowd, Warning, TEXT("Usage: crowd.Cheer <overtake|start|finish> <0|1>"));
			return;
		}

		UE_LOG(LogRaceCrowd, Display, TEXT("crowd.Cheer: %s %s"), *Which, bOn ? TEXT("on") : TEXT("off"));
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
