// Copyright Cena Abachi - Youtube: Devlogerio - devloger.io@gmail.com - Publicated on 2025 - Last update 01/2026 - All Rights Reserved
#include "AdvancedCarsAndBikesSystem.h"

#define LOCTEXT_NAMESPACE "FAdvancedCarsAndBikesSystemModule"

void FAdvancedCarsAndBikesSystemModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FAdvancedCarsAndBikesSystemModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAdvancedCarsAndBikesSystemModule, AdvancedCarsAndBikesSystem)