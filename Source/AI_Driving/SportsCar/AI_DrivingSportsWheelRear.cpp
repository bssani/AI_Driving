// Copyright Epic Games, Inc. All Rights Reserved.


#include "AI_DrivingSportsWheelRear.h"

UAI_DrivingSportsWheelRear::UAI_DrivingSportsWheelRear()
{
	WheelRadius = 40.f;
	WheelWidth = 40.0f;
	FrictionForceMultiplier = 4.0f;
	SlipThreshold = 100.0f;
	SkidThreshold = 100.0f;
	MaxSteerAngle = 0.0f;
	MaxHandBrakeTorque = 6000.0f;
}