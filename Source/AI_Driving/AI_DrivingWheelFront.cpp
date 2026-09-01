// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI_DrivingWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UAI_DrivingWheelFront::UAI_DrivingWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}