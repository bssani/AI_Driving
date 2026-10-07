#include "RaceSetupEditorActions.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RaceGridSpawner.h"
#include "RacingAIProfile.h"
#include "ScopedTransaction.h"

namespace RaceSetupEditor
{
bool CanEdit(const UWorld* World)
{
	return GEditor && World && World->WorldType == EWorldType::Editor && !GEditor->IsPlaySessionInProgress();
}

ARaceGridSpawner* FindGridSpawner(UWorld* World, FText& Error)
{
	if (!CanEdit(World))
	{
		Error = FText::FromString(TEXT("플레이를 종료한 뒤 설정하세요."));
		return nullptr;
	}

	TArray<ARaceGridSpawner*> Selected;
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		if (ARaceGridSpawner* Grid = Cast<ARaceGridSpawner>(*It); Grid && Grid->GetWorld() == World)
		{
			Selected.Add(Grid);
		}
	}
	if (Selected.Num() == 1)
	{
		return Selected[0];
	}
	if (Selected.Num() > 1)
	{
		Error = FText::FromString(TEXT("GridSpawner 하나만 선택하세요."));
		return nullptr;
	}

	TArray<ARaceGridSpawner*> Candidates;
	for (TActorIterator<ARaceGridSpawner> It(World); It; ++It)
	{
		Candidates.Add(*It);
	}
	if (Candidates.Num() == 1)
	{
		return Candidates[0];
	}

	Error = FText::FromString(Candidates.IsEmpty()
		? TEXT("현재 맵에 GridSpawner가 없습니다.")
		: TEXT("대상 GridSpawner 하나를 선택하세요."));
	return nullptr;
}

bool ApplyAIPreset(ARaceGridSpawner& Grid, const TArray<TSoftObjectPtr<URacingAIProfile>>& Profiles, FText& Error)
{
	if (!CanEdit(Grid.GetWorld()))
	{
		Error = FText::FromString(TEXT("프리셋은 플레이 실행 전에만 적용할 수 있습니다."));
		return false;
	}
	if (Profiles.IsEmpty())
	{
		Error = FText::FromString(TEXT("프리셋의 AI 프로파일 목록이 비어 있습니다."));
		return false;
	}

	// 바꾸기 전에 전부 불러 봅니다. 중간에 실패해 슬롯 절반만 바뀌는 일이 없게 합니다.
	TArray<URacingAIProfile*> Loaded;
	for (const TSoftObjectPtr<URacingAIProfile>& Profile : Profiles)
	{
		URacingAIProfile* Resolved = Profile.LoadSynchronous();
		if (!Resolved)
		{
			Error = FText::FromString(TEXT("프리셋 프로파일을 불러올 수 없습니다. Project Settings > Game > Race Setup Presets를 확인하세요."));
			return false;
		}
		Loaded.Add(Resolved);
	}

	const bool bHasAI = Grid.GridSlots.ContainsByPredicate([](const FRaceGridSlot& Slot)
	{
		return Slot.Occupant == ERaceGridOccupant::AI;
	});
	if (!bHasAI)
	{
		Error = FText::FromString(TEXT("GridSpawner에 AI 슬롯이 없습니다."));
		return false;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("RacingAIEditor", "ApplyPreset", "Apply race AI preset"));
	Grid.Modify();

	int32 AIIndex = 0;
	for (FRaceGridSlot& Slot : Grid.GridSlots)
	{
		if (Slot.Occupant != ERaceGridOccupant::AI)
		{
			continue;
		}
		Slot.Profile = Loaded[AIIndex++ % Loaded.Num()];
	}

	// 선택한 구성을 무작위 풀에서 다시 덮어쓰지 않게 합니다.
	Grid.bRandomizeProfiles = false;

	Grid.PostEditChange();
	Grid.MarkPackageDirty();
	Error = FText::GetEmpty();
	return true;
}
}
