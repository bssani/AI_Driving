#pragma once

#include "Widgets/SCompoundWidget.h"

class ARaceGridSpawner;
class UWorld;

/** 플레이 전에 GridSpawner의 AI 구성을 버튼 하나로 바꾸는 패널입니다. */
class SRaceSetupPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRaceSetupPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:
	UWorld* GetEditorWorld() const;
	bool CanChangeSettings() const;
	FText GetCurrentDifficultyText() const;
	FReply ApplyPreset(int32 PresetIndex);

	TWeakObjectPtr<ARaceGridSpawner> AppliedGrid;
	int32 AppliedPresetIndex = INDEX_NONE;

	/** 현재 난이도와 별도로 마지막 작업의 성공/오류를 표시합니다. */
	FText Status;
};
