// Copyright Epic Games, Inc. All Rights Reserved.

#include "RaceRankBoardWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RaceDirectorSubsystem.h"
#include "RaceParticipantComponent.h"
#include "RaceRankPlaceWidget.h"
#include "RaceRankRowWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaceRankBoard, Log, All);

namespace
{
	/** Closer than this and a row is where it is going. FInterpTo only gets there in the limit */
	constexpr float SettledDistance = 0.25f;
}

void URaceRankBoardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// runs again on every change made in the designer, so the preview follows along
	if (IsDesignTime())
	{
		BuildPreview();
	}
}

void URaceRankBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// hidden until there is a field. See the class comment for why this is not Collapsed
	if (!IsDesignTime())
	{
		SetRenderOpacity(0.f);
	}
}

void URaceRankBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsDesignTime())
	{
		return;
	}

	RefreshCountdown -= InDeltaTime;

	if (RefreshCountdown <= 0.f)
	{
		RefreshCountdown = RefreshInterval;
		RefreshStandings();
	}

	for (FRaceRankRow& Row : Rows)
	{
		if (Row.CurrentY == Row.TargetY || !Row.Row)
		{
			continue;
		}

		Row.CurrentY = FMath::FInterpTo(Row.CurrentY, Row.TargetY, InDeltaTime, SlideSpeed);

		if (FMath::Abs(Row.TargetY - Row.CurrentY) < SettledDistance)
		{
			Row.CurrentY = Row.TargetY;
		}

		Row.Row->SetRenderTranslation(FVector2D(0.f, Row.CurrentY));
	}
}

void URaceRankBoardWidget::RefreshStandings()
{
	const URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this);

	if (!Director || !RowColumn)
	{
		return;
	}

	// read once: it sorts the whole field each time it is asked
	const TArray<URaceParticipantComponent*> Ordered = Director->GetParticipantsByPosition();

	if (!IsBuiltFor(Ordered))
	{
		// new rows are laid out in the order just read, so each starts where it belongs
		BuildRows(Ordered);
		return;
	}

	TArray<float> SlotTops;
	GetSlotTops(SlotTops);

	for (FRaceRankRow& Row : Rows)
	{
		const int32 Place = Ordered.IndexOfByKey(Row.Row->GetParticipant());

		if (Place == INDEX_NONE)
		{
			continue;
		}

		Row.TargetY = SlotTops[Place] - SlotTops[Row.SlotIndex];
		Row.Row->SetPlace(Place + 1);
	}
}

bool URaceRankBoardWidget::IsBuiltFor(const TArray<URaceParticipantComponent*>& Ordered) const
{
	if (Ordered.Num() != Rows.Num())
	{
		return false;
	}

	for (const FRaceRankRow& Row : Rows)
	{
		const URaceParticipantComponent* Participant = Row.Row ? Row.Row->GetParticipant() : nullptr;

		if (!Participant || !Ordered.Contains(Participant))
		{
			return false;
		}
	}

	return true;
}

void URaceRankBoardWidget::BuildRows(const TArray<URaceParticipantComponent*>& Ordered)
{
	ClearColumns();

	for (URaceParticipantComponent* Participant : Ordered)
	{
		if (!Participant)
		{
			continue;
		}

		const int32 SlotIndex = Rows.Num();
		URaceRankRowWidget* Row = AddSlot(SlotIndex);

		if (!Row)
		{
			break;
		}

		Row->AssignParticipant(Participant);
		Row->SetPlace(SlotIndex + 1);
	}

	SetRenderOpacity(Rows.IsEmpty() ? 0.f : 1.f);

	UE_LOG(LogRaceRankBoard, Log, TEXT("%s: laid out for %d cars."), *GetClass()->GetName(), Rows.Num());
}

void URaceRankBoardWidget::BuildPreview()
{
	if (!RowColumn)
	{
		return;
	}

	ClearColumns();

	for (int32 Index = 0; Index < PreviewRowCount; ++Index)
	{
		URaceRankRowWidget* Row = AddSlot(Index);

		if (!Row)
		{
			break;
		}

		const FText Name = FText::Format(NSLOCTEXT("RaceRankBoard", "PreviewDriver", "Driver {0}"), Index + 1);
		const uint8 Hue = static_cast<uint8>(Index * 255 / FMath::Max(PreviewRowCount, 1));

		Row->AssignPreview(Name, FLinearColor::MakeFromHSV8(Hue, 200, 255), Index + 1 == PreviewPlayerPlace);
		Row->SetPlace(Index + 1);
	}
}

void URaceRankBoardWidget::ClearColumns()
{
	RowColumn->ClearChildren();

	if (PlaceColumn)
	{
		PlaceColumn->ClearChildren();
	}

	Rows.Reset();
}

URaceRankRowWidget* URaceRankBoardWidget::AddSlot(int32 SlotIndex)
{
	if (!RowClass)
	{
		if (!bWarnedNoRowClass && !IsDesignTime())
		{
			UE_LOG(LogRaceRankBoard, Warning, TEXT("%s: Row Class is not set, so there is nothing to show."),
				*GetClass()->GetName());
			bWarnedNoRowClass = true;
		}

		return nullptr;
	}

	// the gap goes above every slot but the first, in both columns, so they stay level
	const FMargin Gap(0.f, SlotIndex == 0 ? 0.f : RowGap, 0.f, 0.f);

	if (PlaceColumn && PlaceClass)
	{
		if (URaceRankPlaceWidget* Place = CreateWidget<URaceRankPlaceWidget>(this, PlaceClass))
		{
			Place->SetPlace(SlotIndex + 1);
			PlaceColumn->AddChildToVerticalBox(Place)->SetPadding(Gap);
		}
	}

	URaceRankRowWidget* Row = CreateWidget<URaceRankRowWidget>(this, RowClass);

	if (!Row)
	{
		return nullptr;
	}

	RowColumn->AddChildToVerticalBox(Row)->SetPadding(Gap);

	FRaceRankRow& Entry = Rows.AddDefaulted_GetRef();
	Entry.Row = Row;
	Entry.SlotIndex = SlotIndex;

	return Row;
}

void URaceRankBoardWidget::GetSlotTops(TArray<float>& OutTops) const
{
	OutTops.Reset(Rows.Num());

	// taken from the rows themselves rather than a height set on the board, so resizing the row in
	// the designer needs nothing changed here. Desired sizes are the layout's, untouched by the
	// translation the rows are carrying
	float Top = 0.f;

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (Index > 0)
		{
			Top += RowGap;
		}

		OutTops.Add(Top);

		if (const URaceRankRowWidget* Row = Rows[Index].Row)
		{
			Top += Row->GetDesiredSize().Y;
		}
	}
}
