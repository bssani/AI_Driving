// Copyright Cena Abachi - Youtube: Devlogerio - devloger.io@gmail.com - Publicated on 2025 - Last update 01/2026 - All Rights Reserved
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAdvancedCarsAndBikesSystemModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
