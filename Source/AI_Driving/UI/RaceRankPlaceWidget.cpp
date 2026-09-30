// Copyright Epic Games, Inc. All Rights Reserved.

#include "RaceRankPlaceWidget.h"
#include "Components/TextBlock.h"

void URaceRankPlaceWidget::SetPlace(int32 InPlace)
{
	Place = InPlace;

	if (PlaceText)
	{
		PlaceText->SetText(FText::AsNumber(Place));
	}

	OnPlaceSet(Place);
}
