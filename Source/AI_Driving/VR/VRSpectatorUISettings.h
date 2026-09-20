// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Blueprint/UserWidget.h"
#include "VRSpectatorUISettings.generated.h"

/**
 *  What to put on the monitor, set once for the project.
 *
 *  Shown under Project Settings > Game > VR Spectator UI and saved to DefaultGame.ini.
 *
 *  This lives here rather than on the subsystem because a subsystem has no editor of its own:
 *  EditAnywhere on one compiles and then shows up nowhere, which means every change needs a
 *  programmer. The same mistake was already made and fixed once in the vehicle sound plugin.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "VR Spectator UI"))
class AI_DRIVING_API UVRSpectatorUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 *  Drawn on the spectator screen as soon as there is a headset and a player to draw for.
	 *
	 *  **This never reaches the headset.** It is rendered to a texture and handed to the
	 *  spectator screen as an overlay, so it is not part of the scene the driver is shown. A
	 *  widget added to the viewport does the opposite - measured, not assumed.
	 *
	 *  Leave it empty for no overlay. Soft so that a missing or renamed asset costs an empty
	 *  monitor and a log line rather than a failure to load the map.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Spectator Screen", meta = (AllowAbstract = "false"))
	TSoftClassPtr<UUserWidget> SpectatorWidgetClass;
};
