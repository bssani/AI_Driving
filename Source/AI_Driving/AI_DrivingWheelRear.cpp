// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI_DrivingWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UAI_DrivingWheelRear::UAI_DrivingWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}