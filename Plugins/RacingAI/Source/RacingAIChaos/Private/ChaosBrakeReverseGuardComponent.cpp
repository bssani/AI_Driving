#include "ChaosBrakeReverseGuardComponent.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/Pawn.h"

UChaosBrakeReverseGuardComponent::UChaosBrakeReverseGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UChaosBrakeReverseGuardComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		Movement = Owner->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
	}

	if (!Movement)
	{
		SetComponentTickEnabled(false);

		return;
	}

	// 무브먼트가 이번 프레임의 기어를 정하기 전에 플래그를 바꿔 둬야 합니다.
	Movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
}

void UChaosBrakeReverseGuardComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Movement)
	{
		return;
	}

	// 누가 모는지는 첫 틱에야 확정됩니다. 게임모드는 폰을 스폰한 뒤에 빙의시킵니다.
	if (!bOwnerChecked)
	{
		bOwnerChecked = true;

		const APawn* Pawn = Cast<APawn>(GetOwner());

		if (!Pawn || !Pawn->IsPlayerControlled())
		{
			Movement->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick);
			SetComponentTickEnabled(false);

			return;
		}
	}

	const bool bAllowReverse = Movement->GetForwardSpeed() < ReverseEngageSpeed;

	Movement->bReverseAsBrake = bAllowReverse;

	// 달리는 중에 후진 기어가 이미 들어가 있으면 앞 기어로 돌려놓습니다. 거의 선 상태에서 후진이
	// 걸린 뒤 비탈을 따라 앞으로 굴러가면 이렇게 됩니다. 그대로 두면 스로틀이 뒤로 끌어당깁니다.
	if (!bAllowReverse && Movement->GetTargetGear() < 0)
	{
		Movement->SetTargetGear(1, true);
	}
}
