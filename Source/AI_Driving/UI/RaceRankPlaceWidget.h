// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaceRankPlaceWidget.generated.h"

class UTextBlock;

/**
 *  One position number on the rank board, in the column that stays put while the rows slide.
 *
 *  Lay it out in a widget blueprint. A Text Block named PlaceText gets the number; leave it out
 *  and write the number yourself in OnPlaceSet - "P1", "1st", a trophy for the leader.
 *
 *  Keep it the same height as a row, or the numbers drift away from the rows they name.
 */
UCLASS(Abstract)
class AI_DRIVING_API URaceRankPlaceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Which place this is. Called by the board once, right after it makes the widget */
	void SetPlace(int32 InPlace);

	/** 1 for the leader */
	UFUNCTION(BlueprintPure, Category = "Rank Place")
	int32 GetPlace() const { return Place; }

protected:
	/** After the number has been set. 1 for the leader */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rank Place")
	void OnPlaceSet(int32 InPlace);

	UPROPERTY(BlueprintReadOnly, Category = "Rank Place", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaceText;

private:
	int32 Place = 0;
};
