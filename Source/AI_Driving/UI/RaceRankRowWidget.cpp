// Copyright Epic Games, Inc. All Rights Reserved.

#include "RaceRankRowWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "RaceParticipantComponent.h"

namespace
{
	/** Tints either of the two widgets a designer reaches for to make a block of colour */
	void SetTint(UWidget* Widget, const FLinearColor& Colour)
	{
		if (UImage* Image = Cast<UImage>(Widget))
		{
			Image->SetColorAndOpacity(Colour);
		}
		else if (UBorder* Border = Cast<UBorder>(Widget))
		{
			Border->SetBrushColor(Colour);
		}
	}
}

void URaceRankRowWidget::AssignParticipant(URaceParticipantComponent* InParticipant)
{
	Participant = InParticipant;
	Place = 0;

	if (InParticipant)
	{
		Apply(InParticipant->DisplayName, InParticipant->LiveryColor, InParticipant->Emblem, InParticipant->bIsPlayer);
	}
}

void URaceRankRowWidget::AssignPreview(const FText& Name, const FLinearColor& Livery, bool bInIsPlayer)
{
	Participant.Reset();

	// no emblem of its own: whatever image the designer put in the row stays, so it can be judged
	Apply(Name, Livery, Emblem ? Emblem->GetBrush().GetResourceObject() : nullptr, bInIsPlayer);
}

void URaceRankRowWidget::SetPlace(int32 NewPlace)
{
	if (NewPlace == Place)
	{
		return;
	}

	const int32 OldPlace = Place;
	Place = NewPlace;

	OnPlaceChanged(NewPlace, OldPlace);
}

URaceParticipantComponent* URaceRankRowWidget::GetParticipant() const
{
	return Participant.Get();
}

void URaceRankRowWidget::Apply(const FText& Name, const FLinearColor& Livery, UObject* DriverEmblem, bool bInIsPlayer)
{
	bIsPlayer = bInIsPlayer;

	if (NameText)
	{
		NameText->SetText(Name);
	}

	SetTint(LiveryStripe, Livery);

	if (Emblem)
	{
		// only the image changes: the size and tint set in the designer stay. Hidden rather than
		// collapsed, so a car without an emblem does not get a wider name than the rest
		Emblem->SetBrushResourceObject(DriverEmblem);
		Emblem->SetVisibility(DriverEmblem ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	OnAssigned(Name, Livery, DriverEmblem, bIsPlayer);
}
