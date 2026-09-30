// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaceRankBoardWidget.generated.h"

class UVerticalBox;
class URaceParticipantComponent;
class URaceRankPlaceWidget;
class URaceRankRowWidget;

/** One car's row, and where it is on its way to */
USTRUCT()
struct FRaceRankRow
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<URaceRankRowWidget> Row;

	/** The slot the row was laid out in. Never changes: only the translation does */
	int32 SlotIndex = 0;

	float CurrentY = 0.f;
	float TargetY = 0.f;
};

/**
 *  Live race order, one row per car, each row sliding to its new place when the order changes.
 *
 *  A vertical box cannot animate: reorder its children and each one lands in its new slot on the
 *  next frame. So the rows are never reordered. Each keeps the slot it was laid out in and is only
 *  moved on screen, by a render translation, which the layout never sees. The place numbers sit
 *  in a column of their own that does not move, so a row sliding past is the only change.
 *
 *  Everything you see is laid out in widget blueprints; this only fills them and moves them. The
 *  board's blueprint needs a Vertical Box named RowColumn, and may have one named PlaceColumn
 *  beside it. Leave both empty: they are filled with one RowClass and one PlaceClass per car. The
 *  designer fills them with made-up drivers so the board can be judged while it is laid out.
 *
 *  Rows are rebuilt whenever the field changes. The player's car joins the race after the AI
 *  does - the grid spawner adds its participant once the pawn exists - so a board built at the
 *  first sight of the field would be a row short. Until there is a field the board is fully
 *  transparent rather than collapsed: on the spectator screen a widget is only ticked when it is
 *  drawn, and a collapsed one would never be drawn to notice the field arriving.
 */
UCLASS(Abstract)
class AI_DRIVING_API URaceRankBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** One per car. A widget blueprint of RaceRankRowWidget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rank Board")
	TSubclassOf<URaceRankRowWidget> RowClass;

	/** One per place, in PlaceColumn. A widget blueprint of RaceRankPlaceWidget. Empty for no numbers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rank Board")
	TSubclassOf<URaceRankPlaceWidget> PlaceClass;

	/** Space between rows. The numbers get the same, so the two columns stay level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rank Board", meta = (ClampMin = "0.0"))
	float RowGap = 4.f;

	/** How quickly a row closes on its new place. At 8 a one-place move is all but done in 0.4 s */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rank Board", meta = (ClampMin = "0.1"))
	float SlideSpeed = 8.f;

	/**
	 *  How often the order is read, in seconds.
	 *
	 *  Not every frame: the director re-sorts the field by distance each tick, so two cars side by
	 *  side swap places many times a second and their rows would shiver on the spot. Reading less
	 *  often lets a swap that does not last pass unseen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rank Board", meta = (ClampMin = "0.0"))
	float RefreshInterval = 0.25f;

	/** How many made-up drivers the designer shows */
	UPROPERTY(EditAnywhere, Category = "Rank Board|Designer Preview", meta = (ClampMin = "0", ClampMax = "32"))
	int32 PreviewRowCount = 8;

	/** Which of them is shown as the player's row. 1 for the top one, 0 for none */
	UPROPERTY(EditAnywhere, Category = "Rank Board|Designer Preview", meta = (ClampMin = "0"))
	int32 PreviewPlayerPlace = 3;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Where the rows go. Leave it empty in the designer */
	UPROPERTY(BlueprintReadOnly, Category = "Rank Board", meta = (BindWidget))
	TObjectPtr<UVerticalBox> RowColumn;

	/** Where the place numbers go. Leave it empty in the designer, or leave it out for no numbers */
	UPROPERTY(BlueprintReadOnly, Category = "Rank Board", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlaceColumn;

private:
	/** Reads the order and points each row at its place in it */
	void RefreshStandings();

	/** Whether the rows are for exactly these cars, in whatever order */
	bool IsBuiltFor(const TArray<URaceParticipantComponent*>& Ordered) const;

	/** Lays out one row per car, top to bottom in the order given */
	void BuildRows(const TArray<URaceParticipantComponent*>& Ordered);

	/** A field of made-up drivers, so the designer shows what the board will look like */
	void BuildPreview();

	void ClearColumns();

	/** Adds the place number and the row for one slot. Null if there is no row class to make */
	URaceRankRowWidget* AddSlot(int32 SlotIndex);

	/** Where each slot's top edge sits in the row column, worked out from the rows' own heights */
	void GetSlotTops(TArray<float>& OutTops) const;

	UPROPERTY(Transient)
	TArray<FRaceRankRow> Rows;

	float RefreshCountdown = 0.f;

	bool bWarnedNoRowClass = false;
};
