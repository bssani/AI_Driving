#include "SRaceSetupPanel.h"

#include "Editor.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "RaceGridSpawner.h"
#include "RaceSetupEditorActions.h"
#include "RaceSetupPresetSettings.h"
#include "RacingAIProfile.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
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

bool MatchesPreset(const ARaceGridSpawner& Grid, const FRaceSetupAIPreset& Preset)
{
	if (Grid.bRandomizeProfiles || Preset.Profiles.IsEmpty())
	{
		return false;
	}

	int32 AIIndex = 0;
	for (const FRaceGridSlot& Slot : Grid.GridSlots)
	{
		if (Slot.Occupant != ERaceGridOccupant::AI)
		{
			continue;
		}
		const TSoftObjectPtr<URacingAIProfile>& Profile = Preset.Profiles[AIIndex++ % Preset.Profiles.Num()];
		if (Profile.IsNull() || FSoftObjectPath(Slot.Profile.Get()) != Profile.ToSoftObjectPath())
		{
			return false;
		}
	}
	return AIIndex > 0;
}
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
				SNew(STextBlock).Text(this, &SRaceSetupPanel::GetCurrentDifficultyText)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(SHyperlink)
				.Text(FText::FromString(TEXT("Edit presets")))
				.ToolTipText(FText::FromString(TEXT("버튼별 Profiles 목록에 직접 만든 RacingAIProfile을 지정하세요.")))
				.IsEnabled(this, &SRaceSetupPanel::CanChangeSettings)
				.OnNavigate_Lambda([]
				{
					const URaceSetupPresetSettings* Settings = GetDefault<URaceSetupPresetSettings>();
					FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings")).ShowViewer(
						Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
			[
				SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(
					TEXT("프리셋은 기본 프로필을 섞거나 직접 만든 프로필로 구성할 수 있습니다.")))
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

FText SRaceSetupPanel::GetCurrentDifficultyText() const
{
	const ARaceGridSpawner* Grid = AppliedGrid.Get();
	if (!Grid || Grid->GetWorld() != GetEditorWorld() || AppliedPresetIndex == INDEX_NONE)
	{
		return FText::FromString(TEXT("Current difficulty : NOT SET"));
	}

	const URaceSetupPresetSettings* Settings = GetDefault<URaceSetupPresetSettings>();
	int32 MatchingIndex = INDEX_NONE;
	// 같은 조합을 여러 이름에 넣었으면 마지막으로 누른 버튼 이름을 유지합니다.
	if (MatchesPreset(*Grid, Settings->*PresetButtons[AppliedPresetIndex].Preset))
	{
		MatchingIndex = AppliedPresetIndex;
	}
	else
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(PresetButtons); ++Index)
		{
			if (MatchesPreset(*Grid, Settings->*PresetButtons[Index].Preset))
			{
				if (MatchingIndex != INDEX_NONE)
				{
					MatchingIndex = INDEX_NONE;
					break;
				}
				MatchingIndex = Index;
			}
		}
	}

	// Undo나 슬롯/프리셋 수정으로 실제 구성이 달라지면 이전 이름을 표시하지 않습니다.
	const FString Difficulty = MatchingIndex == INDEX_NONE ? TEXT("CUSTOM") : FString(PresetButtons[MatchingIndex].Label).ToUpper();
	return FText::FromString(FString::Printf(TEXT("Current difficulty : %s"), *Difficulty));
}

FReply SRaceSetupPanel::ApplyPreset(int32 PresetIndex)
{
	const FPresetButton& Button = PresetButtons[PresetIndex];
	const FRaceSetupAIPreset& Preset = GetDefault<URaceSetupPresetSettings>()->*Button.Preset;

	ARaceGridSpawner* Grid = RaceSetupEditor::FindGridSpawner(GetEditorWorld(), Status);
	if (Grid && RaceSetupEditor::ApplyAIPreset(*Grid, Preset.Profiles, Status))
	{
		AppliedGrid = Grid;
		AppliedPresetIndex = PresetIndex;
		Status = FText::FromString(FString::Printf(TEXT("%s 적용: %s — 맵을 저장하세요."), Button.Label, *Grid->GetActorLabel()));
	}
	return FReply::Handled();
}
