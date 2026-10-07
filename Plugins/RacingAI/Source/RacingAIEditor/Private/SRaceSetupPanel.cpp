#include "SRaceSetupPanel.h"

#include "Editor.h"
#include "RaceGridSpawner.h"
#include "RaceSetupEditorActions.h"
#include "RaceSetupPresetSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FPresetButton
{
	const TCHAR* Label;
	FRaceSetupAIPreset URaceSetupPresetSettings::* Preset;
};

// 버튼 이름과 적용할 프리셋을 한곳에 묶어, 순서를 바꿔도 이름과 내용이 어긋나지 않게 합니다.
const FPresetButton PresetButtons[] =
{
	{ TEXT("Easy"), &URaceSetupPresetSettings::Easy },
	{ TEXT("Medium"), &URaceSetupPresetSettings::Medium },
	{ TEXT("Difficult"), &URaceSetupPresetSettings::Difficult },
};
}

void SRaceSetupPanel::Construct(const FArguments& Args)
{
	const TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PresetButtons); ++Index)
	{
		const bool bLast = Index == UE_ARRAY_COUNT(PresetButtons) - 1;
		Buttons->AddSlot()
			.FillWidth(1.f)
			.Padding(0.f, 0.f, bLast ? 0.f : 4.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString(PresetButtons[Index].Label))
				.IsEnabled(this, &SRaceSetupPanel::CanChangeSettings)
				.OnClicked(this, &SRaceSetupPanel::ApplyPreset, Index)
			];
	}

	ChildSlot
	[
		SNew(SBorder).Padding(16.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("AI 구성 프리셋")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				Buttons
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]
				{
					return CanChangeSettings() ? Status : FText::FromString(TEXT("플레이를 종료한 뒤 설정하세요."));
				})
			]
		]
	];
}

UWorld* SRaceSetupPanel::GetEditorWorld() const
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

bool SRaceSetupPanel::CanChangeSettings() const
{
	return RaceSetupEditor::CanEdit(GetEditorWorld());
}

FReply SRaceSetupPanel::ApplyPreset(int32 PresetIndex)
{
	const FPresetButton& Button = PresetButtons[PresetIndex];
	const FRaceSetupAIPreset& Preset = GetDefault<URaceSetupPresetSettings>()->*Button.Preset;

	ARaceGridSpawner* Grid = RaceSetupEditor::FindGridSpawner(GetEditorWorld(), Status);
	if (Grid && RaceSetupEditor::ApplyAIPreset(*Grid, Preset.Profiles, Status))
	{
		Status = FText::FromString(FString::Printf(TEXT("%s 적용: %s — 맵을 저장하세요."), Button.Label, *Grid->GetActorLabel()));
	}
	return FReply::Handled();
}
