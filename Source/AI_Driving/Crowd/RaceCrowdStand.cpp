#include "Crowd/RaceCrowdStand.h"
#include "Crowd/RaceCrowdSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/Attenuation.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaceCrowd, Log, All);

ARaceCrowdStand::ARaceCrowdStand()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	CrowdAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("CrowdAudio"));
	CrowdAudio->SetupAttachment(RootComponent);
	CrowdAudio->bAutoActivate = false;
	CrowdAudio->bAutoDestroy = false;
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
		CrowdAudio->SetSound(CrowdLoop);

		if (Attenuation)
		{
			CrowdAudio->AttenuationSettings = Attenuation;

			// The shape is the one setting that cannot be heard as wrong from a standing start.
			// A sphere sounds fine until you drive past, and then the whole stand swings around
			// the head like a single speaker, because outside the sphere that is what it is.
			if (Attenuation->Attenuation.AttenuationShape != EAttenuationShape::Box)
			{
				UE_LOG(LogRaceCrowd, Warning,
					TEXT("%s uses a non-box attenuation. A grandstand is a wall of people tens of ")
					TEXT("metres wide; outside a sphere it collapses to its centre. Use a box, and ")
					TEXT("split a long stand across several actors."), *GetName());
			}
		}
		else
		{
			UE_LOG(LogRaceCrowd, Warning,
				TEXT("%s has no Attenuation, so the crowd plays with no distance or direction at ")
				TEXT("all. Use a box shape."), *GetName());
		}

		if (Concurrency)
		{
			CrowdAudio->ConcurrencySet.Add(Concurrency);
		}

		CrowdAudio->SetVolumeMultiplier(BaseVolume * IdleVolumeFraction);
		CrowdAudio->Play();
	}

	if (URaceCrowdSubsystem* Crowd = URaceCrowdSubsystem::Get(this))
	{
		Crowd->RegisterStand(this);
	}
}

void ARaceCrowdStand::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URaceCrowdSubsystem* Crowd = URaceCrowdSubsystem::Get(this))
	{
		Crowd->UnregisterStand(this);
	}

	if (CrowdAudio)
	{
		CrowdAudio->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

void ARaceCrowdStand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeSinceReaction += DeltaTime;
}

void ARaceCrowdStand::SetExcitement(float InExcitement)
{
	Excitement = FMath::Clamp(InExcitement, 0.0f, 1.0f);

	if (!CrowdAudio)
	{
		return;
	}

	// a graph can do far more with this than a volume can - a murmur and a roar are different
	// recordings, not the same one at two levels - but the level still has to move for the
	// stands that are only given a single loop
	CrowdAudio->SetFloatParameter(FName("Excitement"), Excitement);

	const float Level = FMath::Lerp(IdleVolumeFraction, 1.0f, Excitement);
	CrowdAudio->SetVolumeMultiplier(BaseVolume * Level);
}

bool ARaceCrowdStand::PlayReaction(float Intensity)
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

	// at the stand, not at the car. The people making the noise are not the thing they are
	// reacting to, and a cheer that tracks the car past the grandstand is a very odd sound
	UGameplayStatics::PlaySoundAtLocation(
		this, Sound, GetActorLocation(), GetActorRotation(),
		BaseVolume * FMath::Clamp(Intensity, 0.0f, 1.0f), 1.0f, 0.0f,
		Attenuation, Concurrency);

	return true;
}

FString ARaceCrowdStand::DescribeState() const
{
	return FString::Printf(TEXT("%-24s excite %.2f  vol %.2f  loop %s  reactions %d"),
		*GetName(), Excitement,
		CrowdAudio ? CrowdAudio->VolumeMultiplier : 0.0f,
		CrowdLoop ? *CrowdLoop->GetName() : TEXT("(none)"),
		ReactionSounds.Num());
}
