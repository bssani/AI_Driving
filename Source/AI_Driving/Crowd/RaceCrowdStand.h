#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceCrowdStand.generated.h"

class UAudioComponent;
class USplineComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * One grandstand, heard as a body of people rather than as a speaker.
 *
 * **Draw the spline along the seating and the emitters place themselves.** A crowd is a wall of
 * sound tens of metres wide, and one source cannot be that however loud it is: drive past and it
 * swings around the head like a point, which is the one thing a real crowd never does. So a stand
 * needs several emitters spread along it - but making someone place and configure four actors per
 * stand is how one of them ends up without an attenuation and nobody notices.
 *
 * Leave the spline at its default single point and this is one emitter at the actor, which is
 * what a small isolated stand wants.
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

	/** Draw this along the seating. Emitters are spread along it at EmitterSpacing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crowd")
	TObjectPtr<USplineComponent> StandSpline;

	/** The continuous bed. A MetaSound here is sent Excitement (0-1) and can crossfade between a
	 *  murmur and a roar; a plain looping wave just gets louder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TObjectPtr<USoundBase> CrowdLoop;

	/** Falloff for each emitter.
	 *
	 *  **Use a box.** A sphere behaves like its centre as soon as the listener is outside it, so
	 *  a stand built from spheres is a row of points rather than a wall. Leave empty and the
	 *  stand says so rather than playing something quietly wrong. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TObjectPtr<USoundConcurrency> Concurrency;

	/** How far apart the emitters sit along the spline.
	 *
	 *  Wants to be near the width of the attenuation box so they tile with a little overlap. Too
	 *  far apart and the stand has holes in it; too close and voices are paid for several times
	 *  over for no width gained. Att_CrowdStand is 30 m wide, so 25 m overlaps slightly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (Units = "cm", ClampMin = "100.0"))
	float EmitterSpacing = 2500.0f;

	/** Safety net on a very long spline: voices are not free, and a kilometre of stand at 25 m
	 *  spacing is forty of them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "1"))
	int32 MaxEmitters = 12;

	/** One-shots for something happening in front of this stand - an overtake, the leader
	 *  arriving, the finish. Played from the part of the stand it happened in front of */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TArray<TObjectPtr<USoundBase>> ReactionSounds;

	/** Level of each emitter before excitement is applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float BaseVolume = 1.0f;

	/** How loud the bed is at zero excitement, as a fraction of BaseVolume. Never zero: a full
	 *  grandstand is still a room full of people, and a crowd that switches on when a car
	 *  arrives sounds like a trigger rather than a stand */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleVolumeFraction = 0.45f;

	/** How far a car can be from the stand and still be its business. Measured to the nearest
	 *  point on the spline, not to the actor, so a long stand reacts to what is in front of the
	 *  part the car is actually passing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (Units = "cm", ClampMin = "100.0"))
	float ReactionRadius = 4000.0f;

	/** How often the emitters are checked for having fallen silent on their own.
	 *
	 *  A bed is meant to run for the whole session, but an audio component can be stopped out from
	 *  under it - concurrency limits and voice stealing both do it - and nothing tells the actor.
	 *  The emitter is then alive and silent, which is the one state that never recovers by itself,
	 *  and the stand sounds like it played once and gave up. The vehicle sound plugin carries the
	 *  same guard for the same reason.
	 *
	 *  Checked on an interval rather than every frame: it is a rare fault and there are several
	 *  emitters per stand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (Units = "s", ClampMin = "0.1"))
	float RestartCheckInterval = 1.0f;

	/** Shortest gap between two one-shots from this stand. Without it a pack of cars crossing in
	 *  front fires one per car per frame, which is applause rather than a reaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (Units = "s", ClampMin = "0.0"))
	float MinTimeBetweenReactions = 1.2f;

	/** 0 is a murmur, 1 is on its feet. Sent to every emitter as the Excitement parameter and
	 *  used for their level; the subsystem works it out */
	void SetExcitement(float InExcitement);

	/** Fires a one-shot from the part of the stand nearest the given place. Returns whether it
	 *  played; the rate limit can refuse */
	bool PlayReaction(float Intensity, const FVector& NearTo);

	/** 0-1. Exposed so a Blueprint can drive crowd animation, banners or VFX from the same
	 *  number the sound is using - a stand that sounds excited and looks asleep is worse than
	 *  one that does neither */
	UFUNCTION(BlueprintPure, Category = "Crowd")
	float GetExcitement() const { return Excitement; }

	/** The point on this stand closest to somewhere else. For a single-point spline this is just
	 *  the actor; for a long stand it is the seat the car is driving past */
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FVector GetClosestPointTo(const FVector& World) const;

	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetEmitterCount() const { return Emitters.Num(); }

	/** For the log: what this stand is currently doing */
	FString DescribeState() const;

private:
	/** Builds the emitters along the spline. Called once, at BeginPlay */
	void CreateEmitters();

	/** Restarts any emitter that has stopped on its own */
	void RestartStoppedEmitters();

	UPROPERTY()
	TArray<TObjectPtr<UAudioComponent>> Emitters;

	float Excitement = 0.0f;
	float TimeSinceReaction = 0.0f;
	float TimeSinceRestartCheck = 0.0f;

	/** So a stand that keeps losing its voices says so once rather than every second */
	bool bReportedEmitterStopped = false;

	/** Ticking to age the reaction rate limit and to notice an emitter that has gone silent;
	 *  the beds themselves are driven from the subsystem */
	virtual void Tick(float DeltaTime) override;
};
