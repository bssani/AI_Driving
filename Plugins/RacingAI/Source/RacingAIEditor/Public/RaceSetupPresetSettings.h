#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RaceSetupPresetSettings.generated.h"

class URacingAIProfile;

USTRUCT()
struct FRaceSetupAIPreset
{
	GENERATED_BODY()

	/** AI 슬롯 순서로 적용하며, 슬롯이 더 많으면 목록을 반복합니다. Player 슬롯은 건너뜁니다. */
	UPROPERTY(config, EditAnywhere, Category = "AI Preset")
	TArray<TSoftObjectPtr<URacingAIProfile>> Profiles;
};

/** Race Setup 패널의 버튼이 사용할 AI 혼합 목록입니다. */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Race Setup Presets"))
class RACINGAIEDITOR_API URaceSetupPresetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URaceSetupPresetSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(config, EditAnywhere, Category = "AI Presets")
	FRaceSetupAIPreset Easy;

	UPROPERTY(config, EditAnywhere, Category = "AI Presets")
	FRaceSetupAIPreset Medium;

	UPROPERTY(config, EditAnywhere, Category = "AI Presets")
	FRaceSetupAIPreset Difficult;
};
