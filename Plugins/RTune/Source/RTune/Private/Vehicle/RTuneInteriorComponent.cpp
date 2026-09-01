//Copyright 2025 P.Kallisto 


#include "Vehicle/RTuneInteriorComponent.h"

URTuneInteriorComponent::URTuneInteriorComponent()
{
}

void URTuneInteriorComponent::BeginPlay()
{
	Super::BeginPlay();
	BaseRotation = GetRelativeRotation();

}

void URTuneInteriorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URTuneInteriorComponent::Update(float deltaTime, float inRPM, float inSpeed, float inSteering)
{
	float value = 0.f;
	switch (AnimationType)
	{
	case ERTuneAnimationType::Steering:
		value = inSteering;
		break;
	case ERTuneAnimationType::RPM:
		value = inRPM;
		break;
	case ERTuneAnimationType::Speed:
		value = inSpeed;
		break;
	}

	FRotator newRotator = FRotator::ZeroRotator;
	float d = bInvert ? -1.f : 1.f;

	float remap = FMath::GetMappedRangeValueClamped(InputRange, OutputRange, d*value);


	if (bUseSmoothing)
	{
		Rotation =FMath::FInterpTo(Rotation, remap, deltaTime, SmoothingSpeed);
	}
	else
	{
		Rotation = remap;
	}

	switch (RotationAxis)
	{
	case ERotatorAxis::Yaw:
		newRotator.Yaw = Rotation;
		newRotator.Pitch = BaseRotation.Pitch;
		newRotator.Roll = BaseRotation.Roll;
		break;
	case ERotatorAxis::Roll:
		newRotator.Yaw = BaseRotation.Yaw;
		newRotator.Pitch = BaseRotation.Pitch;
		newRotator.Roll = Rotation;
		break;
	case ERotatorAxis::Pitch:
		newRotator.Yaw = BaseRotation.Yaw;
		newRotator.Pitch = Rotation;
		newRotator.Roll = BaseRotation.Roll;
		break;
	}



	SetRelativeRotation(newRotator);
}
