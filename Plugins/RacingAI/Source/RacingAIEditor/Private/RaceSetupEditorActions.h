#pragma once

#include "CoreMinimal.h"

class ARaceGridSpawner;
class URacingAIProfile;
class UWorld;

namespace RaceSetupEditor
{
	/** 에디터 월드이고 플레이 중이 아닐 때만 true입니다. */
	bool CanEdit(const UWorld* World);

	/** 선택한 GridSpawner, 선택이 없으면 맵에 하나뿐인 GridSpawner를 찾습니다. */
	ARaceGridSpawner* FindGridSpawner(UWorld* World, FText& Error);

	/** AI 슬롯의 Profile만 목록 순서대로 바꿉니다. 하나라도 문제가 있으면 아무것도 바꾸지 않습니다. */
	bool ApplyAIPreset(ARaceGridSpawner& Grid, const TArray<TSoftObjectPtr<URacingAIProfile>>& Profiles, FText& Error);
}
