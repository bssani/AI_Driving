#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

#include "Editor.h"
#include "Editor/Transactor.h"
#include "Engine/World.h"
#include "Framework/Docking/TabManager.h"
#include "RaceGridSpawner.h"
#include "RaceSetupEditorActions.h"
#include "RaceSetupPresetSettings.h"
#include "RacingAIProfile.h"
#include "SRaceSetupPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FSetupWorld
{
	UWorld* World = nullptr;
	UWorld* PreviousEditorWorld = nullptr;

	FSetupWorld()
	{
		// 테스트의 Undo가 사용자가 하던 편집까지 되돌리지 않게 막습니다.
		GEditor->Trans->SetUndoBarrier();

		UWorld::InitializationValues Init;
		Init.CreatePhysicsScene(true).ShouldSimulatePhysics(false).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Editor, false, NAME_None, nullptr, true, ERHIFeatureLevel::SM5, &Init);
	}

	void UseAsCurrentEditorWorld()
	{
		PreviousEditorWorld = GEditor->GetEditorWorldContext().World();
		GEditor->GetEditorWorldContext().SetCurrentWorld(World);
	}

	~FSetupWorld()
	{
		// 테스트가 남긴 기록은 지웁니다. 남겨 두면 파괴된 월드의 오브젝트를 붙잡고,
		// 사용자의 다음 Ctrl+Z가 그 기록부터 되돌립니다.
		while (GEditor->Trans->CanUndo())
		{
			GEditor->Trans->Undo(false);
		}
		GEditor->Trans->RemoveUndoBarrier();

		if (PreviousEditorWorld)
		{
			GEditor->GetEditorWorldContext().SetCurrentWorld(PreviousEditorWorld);
		}
		World->DestroyWorld(false);
	}
};

void CollectWidgets(const TSharedRef<SWidget>& Root, FName Type, TArray<TSharedRef<SWidget>>& Out)
{
	if (Root->GetType() == Type)
	{
		Out.Add(Root);
	}
	FChildren* Children = Root->GetChildren();
	for (int32 Index = 0; Index < Children->Num(); ++Index)
	{
		CollectWidgets(Children->GetChildAt(Index), Type, Out);
	}
}

FString FirstText(const TSharedRef<SWidget>& Root)
{
	TArray<TSharedRef<SWidget>> Texts;
	CollectWidgets(Root, TEXT("STextBlock"), Texts);
	return Texts.IsEmpty() ? FString() : StaticCastSharedRef<STextBlock>(Texts[0])->GetText().ToString();
}

/** 상태줄은 패널의 마지막 텍스트입니다. */
FString StatusText(const TSharedRef<SWidget>& Panel)
{
	TArray<TSharedRef<SWidget>> Texts;
	CollectWidgets(Panel, TEXT("STextBlock"), Texts);
	return Texts.IsEmpty() ? FString() : StaticCastSharedRef<STextBlock>(Texts.Last())->GetText().ToString();
}


FString DifficultyText(const TSharedRef<SWidget>& Panel)
{
	TArray<TSharedRef<SWidget>> Texts;
	CollectWidgets(Panel, TEXT("STextBlock"), Texts);
	for (const TSharedRef<SWidget>& Text : Texts)
	{
		const FString Value = StaticCastSharedRef<STextBlock>(Text)->GetText().ToString();
		if (Value.StartsWith(TEXT("Current difficulty : ")))
		{
			return Value;
		}
	}
	return FString();
}

void Click(const TSharedRef<SWidget>& Button)
{
	StaticCastSharedRef<SButton>(Button)->SimulateClick();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceSetupMixedGridTest, "RacingAI.RaceSetup.MixedGridAndUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRaceSetupMixedGridTest::RunTest(const FString& Parameters)
{
	FSetupWorld Fixture;
	ARaceGridSpawner* Grid = Fixture.World->SpawnActor<ARaceGridSpawner>();
	Grid->SetFlags(RF_Transactional);
	URacingAIProfile* Rookie = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Rookie.DA_Driver_Rookie"));
	URacingAIProfile* Advanced = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Advanced.DA_Driver_Advanced"));
	URacingAIProfile* Pro = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Pro.DA_Driver_Pro"));
	if (!TestNotNull(TEXT("Rookie asset loads"), Rookie) || !TestNotNull(TEXT("Advanced asset loads"), Advanced)
		|| !TestNotNull(TEXT("Pro asset loads"), Pro))
	{
		return false;
	}

	Grid->GridSlots.SetNum(6);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		FRaceGridSlot& Slot = Grid->GridSlots[Index];
		Slot.Profile = Pro;
		Slot.DriverName = FText::FromString(FString::Printf(TEXT("Driver %d"), Index));
		Slot.LaneOffset = 100.f * Index;
		Slot.LiveryName = TEXT("KeepLivery");
	}
	Grid->GridSlots[1].Occupant = ERaceGridOccupant::Player;
	Grid->GridSlots[5].Occupant = ERaceGridOccupant::Player;
	Grid->bRandomizeProfiles = true;

	FText Error;
	// 실패했는데 아래에서 Undo하면 테스트와 무관한 기록을 되돌리게 되므로 여기서 멈춥니다.
	if (!TestTrue(TEXT("Mixed preset applies"), RaceSetupEditor::ApplyAIPreset(*Grid, { Rookie, Advanced }, Error)))
	{
		return false;
	}
	TestEqual(TEXT("First AI is Rookie"), Grid->GridSlots[0].Profile.Get(), Rookie);
	TestEqual(TEXT("Player profile stays unchanged"), Grid->GridSlots[1].Profile.Get(), Pro);
	TestEqual(TEXT("Second AI is Advanced, skipping Player"), Grid->GridSlots[2].Profile.Get(), Advanced);
	TestEqual(TEXT("Third AI repeats Rookie"), Grid->GridSlots[3].Profile.Get(), Rookie);
	TestEqual(TEXT("Fourth AI repeats Advanced"), Grid->GridSlots[4].Profile.Get(), Advanced);
	TestEqual(TEXT("Driver identity stays unchanged"), Grid->GridSlots[2].DriverName.ToString(), FString(TEXT("Driver 2")));
	TestEqual(TEXT("Lane stays unchanged"), Grid->GridSlots[2].LaneOffset, 200.f);
	TestEqual(TEXT("Livery stays unchanged"), Grid->GridSlots[2].LiveryName, FName(TEXT("KeepLivery")));
	TestFalse(TEXT("Random profiles cannot replace selected mix"), Grid->bRandomizeProfiles);

	TestTrue(TEXT("Preset transaction can be undone"), GEditor->UndoTransaction(false));
	TestEqual(TEXT("Undo restores old AI profile"), Grid->GridSlots[0].Profile.Get(), Pro);
	TestTrue(TEXT("Undo restores random profile policy"), Grid->bRandomizeProfiles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceSetupInvalidPresetTest, "RacingAI.RaceSetup.InvalidPresetIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRaceSetupInvalidPresetTest::RunTest(const FString& Parameters)
{
	FSetupWorld Fixture;
	ARaceGridSpawner* Grid = Fixture.World->SpawnActor<ARaceGridSpawner>();
	URacingAIProfile* Rookie = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Rookie.DA_Driver_Rookie"));
	URacingAIProfile* Pro = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Pro.DA_Driver_Pro"));
	Grid->GridSlots.SetNum(2);
	Grid->GridSlots[0].Profile = Pro;
	Grid->GridSlots[1].Profile = Pro;
	Grid->bRandomizeProfiles = true;

	FText Error;
	TestFalse(TEXT("Null profile rejects whole preset"),
		RaceSetupEditor::ApplyAIPreset(*Grid, { Rookie, TSoftObjectPtr<URacingAIProfile>() }, Error));
	TestEqual(TEXT("Failed preset preserves first slot"), Grid->GridSlots[0].Profile.Get(), Pro);
	TestEqual(TEXT("Failed preset preserves second slot"), Grid->GridSlots[1].Profile.Get(), Pro);
	TestTrue(TEXT("Failed preset preserves random mode"), Grid->bRandomizeProfiles);
	TestFalse(TEXT("Failure explains missing profile"), Error.IsEmpty());
	TestFalse(TEXT("Empty preset rejects"), RaceSetupEditor::ApplyAIPreset(*Grid, {}, Error));

	Fixture.World->WorldType = EWorldType::PIE;
	TestFalse(TEXT("Preset does not mutate PIE world"), RaceSetupEditor::ApplyAIPreset(*Grid, { Rookie }, Error));
	TestEqual(TEXT("PIE rejection preserves old profile"), Grid->GridSlots[0].Profile.Get(), Pro);
	Fixture.World->WorldType = EWorldType::Editor;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceSetupPanelTest, "RacingAI.RaceSetup.PanelControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRaceSetupPanelTest::RunTest(const FString& Parameters)
{
	FSetupWorld Fixture;
	Fixture.UseAsCurrentEditorWorld();
	ARaceGridSpawner* Grid = Fixture.World->SpawnActor<ARaceGridSpawner>();
	Grid->SetFlags(RF_Transactional);
	Grid->GridSlots.SetNum(4);

	const TSharedRef<SRaceSetupPanel> Panel = SNew(SRaceSetupPanel);
	TArray<TSharedRef<SWidget>> Buttons;
	CollectWidgets(Panel, TEXT("SButton"), Buttons);
	if (!TestEqual(TEXT("Only three preset buttons"), Buttons.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("First button is Easy"), FirstText(Buttons[0]), FString(TEXT("Easy")));
	TestEqual(TEXT("Second button is Medium"), FirstText(Buttons[1]), FString(TEXT("Medium")));
	TestEqual(TEXT("Third button is Difficult"), FirstText(Buttons[2]), FString(TEXT("Difficult")));
	TestEqual(TEXT("Difficulty initially has no applied preset"), DifficultyText(Panel), FString(TEXT("Current difficulty : NOT SET")));
	TestTrue(TEXT("Race Setup editor tab is registered"),
		FGlobalTabmanager::Get()->HasTabSpawner(TEXT("RacingAI.RaceSetup")));

	URaceSetupPresetSettings* Settings = GetMutableDefault<URaceSetupPresetSettings>();
	URacingAIProfile* Rookie = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Rookie.DA_Driver_Rookie"));
	URacingAIProfile* Advanced = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Advanced.DA_Driver_Advanced"));
	URacingAIProfile* Pro = LoadObject<URacingAIProfile>(nullptr, TEXT("/RacingAI/Profiles/DA_Driver_Pro.DA_Driver_Pro"));
	URacingAIProfile* CustomEasy = DuplicateObject<URacingAIProfile>(Rookie, GetTransientPackage());
	CustomEasy->LookaheadSeconds = 0.67f;
	using FProfileList = TArray<TSoftObjectPtr<URacingAIProfile>>;
	TGuardValue<FProfileList> EasyGuard(Settings->Easy.Profiles, FProfileList{ CustomEasy, Rookie, Rookie, Advanced });
	TGuardValue<FProfileList> MediumGuard(Settings->Medium.Profiles, FProfileList{ Rookie, Advanced, Advanced, Pro });
	TGuardValue<FProfileList> DifficultGuard(Settings->Difficult.Profiles, FProfileList{ Advanced, Pro, Pro, Pro });

	Click(Buttons[0]);
	TestEqual(TEXT("Easy button applies configured fourth AI"), Grid->GridSlots[3].Profile.Get(), Advanced);
	TestEqual(TEXT("Easy uses a user-created profile"), Grid->GridSlots[0].Profile.Get(), CustomEasy);
	TestEqual(TEXT("Custom profile tuning is preserved"), Grid->GridSlots[0].Profile->LookaheadSeconds, 0.67f);
	TestEqual(TEXT("Easy updates current difficulty"), DifficultyText(Panel), FString(TEXT("Current difficulty : EASY")));
	Click(Buttons[1]);
	TestEqual(TEXT("Medium button applies configured fourth AI"), Grid->GridSlots[3].Profile.Get(), Pro);
	TestEqual(TEXT("Medium updates current difficulty"), DifficultyText(Panel), FString(TEXT("Current difficulty : MEDIUM")));
	Click(Buttons[2]);
	TestEqual(TEXT("Difficult button applies configured first AI"), Grid->GridSlots[0].Profile.Get(), Advanced);
	TestTrue(TEXT("Status line reports the applied preset"), StatusText(Panel).StartsWith(TEXT("Difficult 적용")));
	TestEqual(TEXT("Difficult updates current difficulty"), DifficultyText(Panel), FString(TEXT("Current difficulty : DIFFICULT")));
	{
		TGuardValue<FProfileList> InvalidPreset(Settings->Easy.Profiles, FProfileList{});
		Click(Buttons[0]);
		TestFalse(TEXT("Invalid preset reports an error"), StatusText(Panel).IsEmpty());
		TestEqual(TEXT("Failed preset preserves current difficulty"), DifficultyText(Panel), FString(TEXT("Current difficulty : DIFFICULT")));
	}
	TestTrue(TEXT("Undo returns to Medium"), GEditor->UndoTransaction(false));
	TestEqual(TEXT("Difficulty reflects Undo"), DifficultyText(Panel), FString(TEXT("Current difficulty : MEDIUM")));
	Grid->GridSlots[0].Profile = CustomEasy;
	TestEqual(TEXT("Manual slot edits invalidate the difficulty"), DifficultyText(Panel), FString(TEXT("Current difficulty : CUSTOM")));
	Click(Buttons[2]);

	// GridSpawner가 둘인데 아무것도 선택하지 않았으면 바꾸지 않고, 그 이유가 상태줄에 보여야 합니다.
	Fixture.World->SpawnActor<ARaceGridSpawner>();
	Click(Buttons[0]);
	TestEqual(TEXT("Ambiguous grid leaves first AI alone"), Grid->GridSlots[0].Profile.Get(), Advanced);
	TestEqual(TEXT("Status line explains the ambiguous grid"), StatusText(Panel), FString(TEXT("대상 GridSpawner 하나를 선택하세요.")));
	return true;
}
#endif
