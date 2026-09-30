// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaceRankRowWidget.generated.h"

class UImage;
class UTextBlock;
class URaceParticipantComponent;

/**
 *  One car's row on the rank board. The board makes one per car and slides it; this only says
 *  who the car is.
 *
 *  Lay it out in a widget blueprint. Every part below is optional and found by name, so a row
 *  can leave out whatever it does not want:
 *
 *    NameText       Text Block. Gets the driver's name
 *    LiveryStripe   Image or Border. Tinted the car's livery colour - the same TintColor the
 *                   livery applier sets on the car's paint
 *    Emblem         Image. Gets the emblem from the car's grid slot - a flag, a team logo. It
 *                   stays with the driver whatever livery the car is given. Hidden, keeping
 *                   its space, for a car without one
 *
 *  Nothing else is coloured from code. Anything more - a mark on the player's row, a flash on
 *  overtaking, a different font for the leader - goes in OnAssigned and OnPlaceChanged.
 *
 *  Keep every row the same height as the place numbers beside it, or the two columns drift apart.
 */
UCLASS(Abstract)
class AI_DRIVING_API URaceRankRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fills the row for this car. Called by the board, once, right after it makes the row */
	void AssignParticipant(URaceParticipantComponent* InParticipant);

	/** Fills the row with a made-up driver, for the designer. Called by the board */
	void AssignPreview(const FText& Name, const FLinearColor& Livery, bool bInIsPlayer);

	/** Tells the row where its car now is. Called by the board, which only calls when it changes */
	void SetPlace(int32 NewPlace);

	/** The car this row is for. Empty in the designer */
	UFUNCTION(BlueprintPure, Category = "Rank Row")
	URaceParticipantComponent* GetParticipant() const;

	/** 1 for the leader. 0 until the board has placed the row */
	UFUNCTION(BlueprintPure, Category = "Rank Row")
	int32 GetPlace() const { return Place; }

	UFUNCTION(BlueprintPure, Category = "Rank Row")
	bool IsPlayer() const { return bIsPlayer; }

protected:
	/** After the row has been filled in, name, colour and emblem already set. Emblem may be empty */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rank Row")
	void OnAssigned(const FText& Name, FLinearColor Livery, UObject* DriverEmblem, bool bInIsPlayer);

	/**
	 *  When the car gains or loses places. Fires as the row starts to slide, not when it arrives.
	 *  OldPlace is 0 the first time.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rank Row")
	void OnPlaceChanged(int32 NewPlace, int32 OldPlace);

	UPROPERTY(BlueprintReadOnly, Category = "Rank Row", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Rank Row", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LiveryStripe;

	UPROPERTY(BlueprintReadOnly, Category = "Rank Row", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Emblem;

private:
	void Apply(const FText& Name, const FLinearColor& Livery, UObject* DriverEmblem, bool bInIsPlayer);

	TWeakObjectPtr<URaceParticipantComponent> Participant;

	int32 Place = 0;
	bool bIsPlayer = false;
};
