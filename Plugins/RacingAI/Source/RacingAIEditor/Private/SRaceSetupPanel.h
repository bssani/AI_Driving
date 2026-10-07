#pragma once

#include "Widgets/SCompoundWidget.h"

class UWorld;

/** 플레이 전에 GridSpawner의 AI 구성을 프리셋 버튼 하나로 바꾸는 패널입니다. */
class SRaceSetupPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRaceSetupPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:
	UWorld* GetEditorWorld() const;
	bool CanChangeSettings() const;
	FReply ApplyPreset(int32 PresetIndex);

	/** 마지막 버튼의 결과(성공 또는 실패 이유)입니다. */
	FText Status;
};
