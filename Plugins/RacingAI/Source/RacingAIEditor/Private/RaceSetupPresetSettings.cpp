#include "RaceSetupPresetSettings.h"

#include "RacingAIProfile.h"

URaceSetupPresetSettings::URaceSetupPresetSettings()
{
	const TSoftObjectPtr<URacingAIProfile> Rookie(FSoftObjectPath(TEXT("/RacingAI/Profiles/DA_Driver_Rookie.DA_Driver_Rookie")));
	const TSoftObjectPtr<URacingAIProfile> Advanced(FSoftObjectPath(TEXT("/RacingAI/Profiles/DA_Driver_Advanced.DA_Driver_Advanced")));
	const TSoftObjectPtr<URacingAIProfile> Pro(FSoftObjectPath(TEXT("/RacingAI/Profiles/DA_Driver_Pro.DA_Driver_Pro")));

	Easy.Profiles = { Rookie, Rookie, Rookie, Advanced };
	Medium.Profiles = { Rookie, Advanced, Advanced, Pro };
	Difficult.Profiles = { Advanced, Pro, Pro, Pro };
}
