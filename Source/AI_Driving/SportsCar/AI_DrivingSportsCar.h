// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI_DrivingPawn.h"
#include "AI_DrivingSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class AAI_DrivingSportsCar : public AAI_DrivingPawn
{
	GENERATED_BODY()
	
public:

	AAI_DrivingSportsCar();
};
