#include "ChaosVehicleInputAdapter.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/Actor.h"
#include "RacingAIModule.h"

UChaosVehicleInputAdapter::UChaosVehicleInputAdapter()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UChaosVehicleInputAdapter::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		Movement = Owner->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
	}

	if (!Movement)
	{
		UE_LOG(LogRacingAI, Warning,
			TEXT("%s: ChaosWheeledVehicleMovementComponent를 찾지 못했습니다. AI 입력이 적용되지 않습니다."),
			*GetNameSafe(GetOwner()));

		return;
	}

	// Chaos는 기본적으로 컨트롤러가 붙은 로컬 폰의 입력만 처리합니다
	// (UChaosVehicleMovementComponent::UpdateState의 bProcessLocally 판정).
	// AI 차량은 보통 아무도 빙의하지 않으므로 이 요구를 꺼야 입력이 실제로 들어갑니다.
	// 끄지 않으면 AI 차량이 전혀 움직이지 않습니다.
	if (bDisableControllerRequirement)
	{
		Movement->SetRequiresControllerForInputs(false);
	}

	// bReverseAsBrake는 아케이드 조작용으로, 브레이크 입력이 들어오는 순간 목표 기어를
	// -1로 바꿉니다. AI는 감속을 위해 브레이크를 상시 사용하므로 켜 두면 의도치 않게
	// 후진합니다. 후진은 SetReverseGear로만 명시적으로 하도록 끕니다.
	if (bDisableReverseAsBrake)
	{
		Movement->bReverseAsBrake = false;
	}

	Movement->SetUseAutomaticGears(bUseAutomaticGears);
}

void UChaosVehicleInputAdapter::ApplySteering_Implementation(float Value)
{
	if (Movement)
	{
		Movement->SetSteeringInput(FMath::Clamp(Value, -1.f, 1.f));
	}
}

void UChaosVehicleInputAdapter::ApplyThrottle_Implementation(float Value)
{
	if (Movement)
	{
		Movement->SetThrottleInput(FMath::Clamp(Value, 0.f, 1.f));
	}
}

void UChaosVehicleInputAdapter::ApplyBrake_Implementation(float Value)
{
	if (Movement)
	{
		Movement->SetBrakeInput(FMath::Clamp(Value, 0.f, 1.f));
	}
}

void UChaosVehicleInputAdapter::SetReverseGear_Implementation(bool bReverse)
{
	if (!Movement || bReversing == bReverse)
	{
		return;
	}

	bReversing = bReverse;

	// 후진 중에는 자동 변속을 꺼야 합니다. 켜 둔 채로 기어를 -1로 두면
	// 다음 프레임에 곧바로 전진 기어로 되돌아갑니다.
	Movement->SetUseAutomaticGears(bReverse ? false : bUseAutomaticGears);
	Movement->SetTargetGear(bReverse ? -1 : 1, true);
}
