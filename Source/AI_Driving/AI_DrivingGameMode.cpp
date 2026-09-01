// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI_DrivingGameMode.h"
#include "AI_DrivingPlayerController.h"

AAI_DrivingGameMode::AAI_DrivingGameMode()
{
	PlayerControllerClass = AAI_DrivingPlayerController::StaticClass();
}
