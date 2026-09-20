#include "Crowd/RaceCrowdStand.h"
#include "Crowd/RaceCrowdSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/Attenuation.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaceCrowd, Log, All);

ARaceCrowdStand::ARaceCrowdStand()
{
	PrimaryActorTick.bCanEverTick = true;

	StandSpline = CreateDefaultSubobject<USplineComponent>(TEXT("StandSpline"));
	RootComponent = StandSpline;

	// a fresh stand is a single point until somebody draws it out, so that dropping one in and
	// assigning a sound is all it takes for the simple case
	StandSpline->ClearSplinePoints(false);
	StandSpline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
	StandSpline->UpdateSpline();
}

void ARaceCrowdStand::BeginPlay()
{
	Super::BeginPlay();

	if (!CrowdLoop)
	{
		UE_LOG(LogRaceCrowd, Warning,
			TEXT("%s has no CrowdLoop assigned, so this stand is silent."), *GetName());
	}
	else
	{
		if (Attenuation)
		{
			// The shape is the one setting that cannot be heard as wrong from a standing start.
			// A sphere sounds fine until you drive past, and then each emitter swings around the
			// head like a single speaker, because outside the sphere that is what it is.
			if (Attenuation->Attenuation.AttenuationShape != EAttenuationShape::Box)
			{
				UE_LOG(LogRaceCrowd, Warning,
					TEXT("%s uses a non-box attenuation. A grandstand is a wall of people; outside ")
					TEXT("a sphere each emitter collapses to its centre and the stand becomes a row ")
					TEXT("of points. Use a box."), *GetName());
			}
		}
		else
		{
			UE_LOG(LogRaceCrowd, Warning,
				TEXT("%s has no Attenuation, so the crowd plays with no distance or direction at ")
				TEXT("all. Use a box shape."), *GetName());
		}

		CreateEmitters();
	}

	if (URaceCrowdSubsystem* Crowd = URaceCrowdSubsystem::Get(this))
	{
		Crowd->RegisterStand(this);
	}
}

void ARaceCrowdStand::CreateEmitters()
{
	const float Length = StandSpline ? StandSpline->GetSplineLength() : 0.0f;

	// one emitter for a stand that was never drawn out, otherwise enough of them to tile the
	// spline. Rounding up rather than down: a stand that is one metre longer than two spacings
	// wants three emitters with a little overlap, not two with a metre of silence between them
	const int32 Count = Length > KINDA_SMALL_NUMBER
		? FMath::Clamp(FMath::CeilToInt(Length / FMath::Max(EmitterSpacing, 1.0f)), 1, MaxEmitters)
		: 1;

	if (Length > KINDA_SMALL_NUMBER && Count == MaxEmitters
		&& FMath::CeilToInt(Length / FMath::Max(EmitterSpacing, 1.0f)) > MaxEmitters)
	{
		UE_LOG(LogRaceCrowd, Warning,
			TEXT("%s is %.0f m long, which wants more than MaxEmitters (%d) at %.0f m spacing. ")
			TEXT("The emitters will be spread further apart than asked for; either raise the cap ")
			TEXT("or split the stand."),
			*GetName(), Length / 100.0f, MaxEmitters, EmitterSpacing / 100.0f);
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		UAudioComponent* Emitter = NewObject<UAudioComponent>(this);

		if (!Emitter)
		{
			continue;
		}

		Emitter->SetSound(CrowdLoop);
		Emitter->bAutoActivate = false;
		Emitter->bAutoDestroy = false;

		if (Attenuation)
		{
			Emitter->AttenuationSettings = Attenuation;
		}

		if (Concurrency)
		{
			Emitter->ConcurrencySet.Add(Concurrency);
		}

		Emitter->SetupAttachment(StandSpline);
		Emitter->RegisterComponent();

		if (Length > KINDA_SMALL_NUMBER)
		{
			// centred in its own share of the spline rather than at the ends, so the first and
			// last emitters do not hang half off the stand
			const float Distance = Length * (Index + 0.5f) / Count;
			Emitter->SetWorldLocation(StandSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
			Emitter->SetWorldRotation(StandSpline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
		}

		Emitter->SetVolumeMultiplier(BaseVolume * IdleVolumeFraction);
		Emitter->Play();

		Emitters.Add(Emitter);
	}

	UE_LOG(LogRaceCrowd, Log, TEXT("%s: %d emitter(s) across %.0f m."),
		*GetName(), Emitters.Num(), Length / 100.0f);
}

void ARaceCrowdStand::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URaceCrowdSubsystem* Crowd = URaceCrowdSubsystem::Get(this))
	{
		Crowd->UnregisterStand(this);
	}

	for (UAudioComponent* Emitter : Emitters)
	{
		if (Emitter)
		{
			Emitter->Stop();
		}
	}
	Emitters.Empty();

	Super::EndPlay(EndPlayReason);
}

void ARaceCrowdStand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeSinceReaction += DeltaTime;
}

FVector ARaceCrowdStand::GetClosestPointTo(const FVector& World) const
{
	if (StandSpline && StandSpline->GetSplineLength() > KINDA_SMALL_NUMBER)
	{
		return StandSpline->FindLocationClosestToWorldLocation(World, ESplineCoordinateSpace::World);
	}

	return GetActorLocation();
}

void ARaceCrowdStand::SetExcitement(float InExcitement)
{
	Excitement = FMath::Clamp(InExcitement, 0.0f, 1.0f);

	// a graph can do far more with this than a volume can - a murmur and a roar are different
	// recordings, not the same one at two levels - but the level still has to move for the
	// stands that are only given a single loop
	const float Level = BaseVolume * FMath::Lerp(IdleVolumeFraction, 1.0f, Excitement);

	for (UAudioComponent* Emitter : Emitters)
	{
		if (Emitter)
		{
			Emitter->SetFloatParameter(FName("Excitement"), Excitement);
			Emitter->SetVolumeMultiplier(Level);
		}
	}
}

bool ARaceCrowdStand::PlayReaction(float Intensity, const FVector& NearTo)
{
	if (ReactionSounds.Num() == 0 || TimeSinceReaction < MinTimeBetweenReactions)
	{
		return false;
	}

	USoundBase* Sound = ReactionSounds[FMath::RandRange(0, ReactionSounds.Num() - 1)];

	if (!Sound)
	{
		return false;
	}

	TimeSinceReaction = 0.0f;

	// from the part of the stand it happened in front of, not from the middle of the actor and
	// not from the car. The people making the noise are not the thing they are reacting to, and
	// on a hundred metres of grandstand the difference is the length of a football pitch
	UGameplayStatics::PlaySoundAtLocation(
		this, Sound, GetClosestPointTo(NearTo), GetActorRotation(),
		BaseVolume * FMath::Clamp(Intensity, 0.0f, 1.0f), 1.0f, 0.0f,
		Attenuation, Concurrency);

	return true;
}

FString ARaceCrowdStand::DescribeState() const
{
	const float Length = StandSpline ? StandSpline->GetSplineLength() : 0.0f;

	return FString::Printf(TEXT("%-22s excite %.2f  emitters %d over %.0fm  loop %s  reactions %d"),
		*GetName(), Excitement, Emitters.Num(), Length / 100.0f,
		CrowdLoop ? *CrowdLoop->GetName() : TEXT("(none)"),
		ReactionSounds.Num());
}
