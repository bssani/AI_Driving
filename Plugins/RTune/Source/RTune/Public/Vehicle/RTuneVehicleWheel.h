//Copyright 2024 P.Kallisto 

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "RTuneVehicleWheel.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (RTune), meta = (BlueprintSpawnableComponent))
class RTUNE_API URTuneVehicleWheel : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	URTuneVehicleWheel();

	bool IsLeftWheel();

private:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTune|Animation System", meta = (AllowPrivateAccess = "true"))
		bool bRotateWheel = false;

};
