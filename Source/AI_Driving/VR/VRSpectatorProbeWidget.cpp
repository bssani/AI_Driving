// Copyright Epic Games, Inc. All Rights Reserved.

#include "VRSpectatorProbeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

UVRSpectatorProbeWidget::UVRSpectatorProbeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

namespace
{
	/** One full-width bar pinned to the top or bottom edge, loud enough to read through a lens */
	void AddBanner(UWidgetTree& Tree, UCanvasPanel& Canvas, const FString& Message,
		const FLinearColor& Colour, bool bTop)
	{
		UBorder* Banner = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		Banner->SetBrushColor(Colour);
		Banner->SetPadding(FMargin(24.f));
		Banner->SetHorizontalAlignment(HAlign_Center);
		Banner->SetVerticalAlignment(VAlign_Center);

		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Message));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 34);
		Text->SetFont(Font);

		Banner->AddChild(Text);

		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(Banner);
		FAnchors Anchors = bTop ? FAnchors(0.f, 0.f, 1.f, 0.f) : FAnchors(0.f, 1.f, 1.f, 1.f);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(FMargin(0.f, 0.f, 0.f, 110.f));
		Slot->SetAlignment(FVector2D(0.f, bTop ? 0.f : 1.f));
	}
}

TSharedRef<SWidget> UVRSpectatorProbeWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		AddBanner(*WidgetTree, *Canvas,
			TEXT("SCREEN-SPACE UMG  -  this belongs on the monitor only"),
			FLinearColor(0.9f, 0.f, 0.6f), true);

		AddBanner(*WidgetTree, *Canvas,
			TEXT("Reading this inside the headset means screen-space UI reaches the HMD"),
			FLinearColor(0.f, 0.4f, 0.9f), false);
	}

	return Super::RebuildWidget();
}
