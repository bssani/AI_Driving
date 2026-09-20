#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceCrowdStand.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * One section of grandstand, heard as a body of people rather than as a speaker.
 *
 * Place several along a stand rather than one in the middle. A crowd is a wall of sound thirty
 * metres wide, and a single source cannot be that however loud it is: drive past it and it swings
 * around the head like a point, which is the one thing a crowd never does. The attenuation has to
 * be a box for the same reason - a sphere collapses to its centre as soon as you are outside it.
 *
 * The subsystem drives Excitement; this actor only plays.
 */
UCLASS()
class AI_DRIVING_API ARaceCrowdStand : public AActor
{
	GENERATED_BODY()

public:
	ARaceCrowdStand();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The continuous bed. A MetaSound here is sent Excitement (0-1) and can crossfade between a
	 *  murmur and a roar; a plain looping wave just gets louder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TObjectPtr<USoundBase> CrowdLoop;

	/** Falloff for the bed.
	 *
	 *  **Use a box, and turn doppler off.** A sphere makes a thirty-metre stand behave like a
	 *  point the moment the listener is outside it. Doppler is computed from relative velocity,
	 *  so a car passing at racing speed sweeps the pitch of the whole crowd - people do not
	 *  change pitch as you drive past them, and on a wide bed it reads as nausea rather than
	 *  speed. Leave empty and the stand will say so rather than playing something wrong. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TObjectPtr<USoundConcurrency> Concurrency;

	/** One-shots for something happening in front of this stand - an overtake, the leader
	 *  arriving, the finish. Picked at random; intensity sets the volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TArray<TObjectPtr<USoundBase>> ReactionSounds;

	/** Level of the bed before excitement is applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float BaseVolume = 1.0f;

	/** How loud the bed is at zero excitement, as a fraction of BaseVolume. Never zero: an empty
	 *  grandstand is still a room full of people, and a crowd that switches on when a car
	 *  arrives sounds like a trigger rather than a stand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleVolumeFraction = 0.45f;

	/** How far a car can be and still be this stand's business.
	 *
	 *  Only used to decide which stand reacts and how hard; the falloff above decides what is
	 *  audible. Roughly the length of the stand plus the track width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (Units = "cm", ClampMin = "100.0"))
	float ReactionRadius = 4000.0f;

	/** Shortest gap between two one-shots from this stand. Without it a pack of cars crossing in
	 *  front fires one per car per frame, which is applause rather than a reaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (Units = "s", ClampMin = "0.0"))
	float MinTimeBetweenReactions = 1.2f;

	/** 0 is a murmur, 1 is on its feet. Sent to the bed as the Excitement parameter and used for
	 *  its level; the subsystem works it out */
	void SetExcitement(float InExcitement);

	/** Fires a one-shot reaction, subject to the rate limit. Returns whether it played */
	bool PlayReaction(float Intensity);

	float GetExcitement() const { return Excitement; }

	/** Where this stand counts a car as being in front of it */
	FVector GetListeningLocation() const { return GetActorLocation(); }

	/** For the log: what this stand is currently doing */
	FString DescribeState() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crowd", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioComponent> CrowdAudio;

	float Excitement = 0.0f;
	float TimeSinceReaction = 0.0f;

	/** Ticking only to age the reaction rate limit; the bed is driven from the subsystem */
	virtual void Tick(float DeltaTime) override;
};
