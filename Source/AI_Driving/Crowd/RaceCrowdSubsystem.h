#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RaceCrowdSubsystem.generated.h"

class ARaceCrowdStand;
class URaceDirectorSubsystem;
class URaceParticipantComponent;

/**
 * Works out how excited the crowd is and drives every grandstand from it.
 *
 * The stands themselves only play. Everything that decides what they should be doing lives here,
 * for the same reason the race director exists: each stand asking the world about the race
 * separately is both slower and inconsistent, and a crowd that disagrees with itself across a
 * stadium is worse than one that is simply wrong.
 *
 * Excitement is a per-stand number, not a global one. A crowd at the far hairpin has no reason to
 * roar because something happened at the start line, and a stadium that reacts as one body reads
 * as a single speaker no matter how many actors it is made of.
 */
UCLASS()
class AI_DRIVING_API URaceCrowdSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Crowd", meta = (WorldContext = "WorldContext"))
	static URaceCrowdSubsystem* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	void RegisterStand(ARaceCrowdStand* Stand);
	void UnregisterStand(ARaceCrowdStand* Stand);

	//--------------------------------------------------------------------------
	// Tuning. A crowd is judged entirely by ear, so all of it is reachable at runtime
	//--------------------------------------------------------------------------

	/** How often the crowd is re-evaluated. A body of people does not need a game frame: ten
	 *  times a second is already finer than the ear can follow on a sound this wide */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (Units = "s", ClampMin = "0.01"))
	float UpdateInterval = 0.1f;

	/** Baseline before anything is happening. Never zero - an empty-sounding grandstand full of
	 *  people is a worse mistake than one that is slightly too loud */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleExcitement = 0.12f;

	/** Waiting for the lights. Higher than racing: the anticipation before a start is the loudest
	 *  a crowd gets that is not a crash */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CountdownExcitement = 0.40f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RacingExcitement = 0.28f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FinishExcitement = 0.85f;

	/** Added when a car is passing directly in front of this stand, at speed */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PassBoost = 0.50f;

	/** Added on top when two cars are close enough to be fighting as they pass. A procession and
	 *  a battle are not the same event and a crowd is the main thing that says which it was */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BattleBoost = 0.28f;

	/** How close two cars have to be, along the track, to count as fighting */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (Units = "cm", ClampMin = "100.0"))
	float BattleGap = 1500.0f;

	/** Speed at which a car going past is as exciting as it gets. Below it the boost scales down,
	 *  so a car limping past the stand does not get a standing ovation */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (Units = "cm/s", ClampMin = "1.0"))
	float ExcitingSpeed = 2500.0f;

	/** How much an overtake or a finish adds, before it decays */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EventPulseStrength = 0.45f;

	/** How fast an event pulse fades, per second */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.01"))
	float EventPulseDecay = 0.55f;

	/** How fast a stand can get louder, in excitement per second. Crowds catch on quickly */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.01"))
	float RiseRate = 4.0f;

	/** How fast a stand settles again. Slower than the rise on purpose: a crowd that drops as
	 *  fast as it climbs sounds like a fader, because that is what it is */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Tuning", meta = (ClampMin = "0.01"))
	float FallRate = 0.8f;

	/** Pins every stand to a fixed excitement, ignoring the race. Negative hands it back.
	 *  Driven by crowd.Excitement: pinning it answers whether the beds and the graph respond at
	 *  all, which is otherwise impossible to tell apart from the race never being exciting */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Debug")
	float ExcitementOverride = -1.0f;

	/** Writes what every stand is doing, and what is driving it, to the log */
	void LogState() const;

	int32 GetStandCount() const { return Stands.Num(); }

private:
	void UpdateCrowd(float DeltaTime);

	/** Overtakes are not broadcast by the director, so they are noticed here by watching each
	 *  participant's position change. Only while actually racing: finishing and resetting both
	 *  reshuffle the order without anyone having overtaken anybody */
	void DetectOvertakes(URaceDirectorSubsystem& Director);

	/** Excitement this stand has earned from what is in front of it right now */
	float ComputeLocalExcitement(const ARaceCrowdStand& Stand, URaceDirectorSubsystem& Director) const;

	/** Fires a one-shot at whichever stand the event happened in front of */
	void ReactNearest(const FVector& Location, float Intensity);

	UFUNCTION()
	void HandleRaceStarted();

	UFUNCTION()
	void HandleRacerFinished(URaceParticipantComponent* Participant, int32 FinishPosition);

	UFUNCTION()
	void HandleRaceReset();

	/** Bound on the first tick rather than in Initialize: subsystem creation order is not
	 *  guaranteed, and the director may not exist yet when this one is made */
	void BindToDirector();

	TArray<TWeakObjectPtr<ARaceCrowdStand>> Stands;

	/** Last known finishing position per participant, for spotting overtakes */
	TMap<TWeakObjectPtr<URaceParticipantComponent>, int32> LastPosition;

	TWeakObjectPtr<URaceDirectorSubsystem> BoundDirector;

	float Accumulator = 0.0f;
	float EventPulse = 0.0f;
	bool bBound = false;
};
