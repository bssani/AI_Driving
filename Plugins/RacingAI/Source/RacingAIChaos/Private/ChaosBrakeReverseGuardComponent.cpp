#include "ChaosBrakeReverseGuardComponent.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "RaceParticipantComponent.h"

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
		// 기존 BP에 붙인 컴포넌트와 C++ 기본 컴포넌트가 함께 있을 수 있습니다.
		// 둘이 중립/변속 설정을 저장하고 복구하면 서로의 임시 설정을 원래 값으로 오인합니다.
		if (Owner->FindComponentByClass<UChaosBrakeReverseGuardComponent>() != this)
		{
			SetComponentTickEnabled(false);
			return;
		}
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

void UChaosBrakeReverseGuardComponent::ReleaseNeutralHold()
{
	if (bNeutralHoldActive && Movement)
	{
		Movement->SetUseAutomaticGears(bSavedAutomaticGears);
		Movement->bReverseAsBrake = bSavedReverseAsBrake;
		Movement->SetTargetGear(1, true);
	}
	bNeutralHoldActive = false;
}

void UChaosBrakeReverseGuardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseNeutralHold();
	if (Movement)
	{
		Movement->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick);
	}
	Super::EndPlay(EndPlayReason);
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

	const URaceParticipantComponent* Participant = GetOwner()->FindComponentByClass<URaceParticipantComponent>();
	if (Participant && Participant->IsInputLocked())
	{
		if (!bNeutralHoldActive)
		{
			bSavedAutomaticGears = Movement->GetUseAutoGears();
			bSavedReverseAsBrake = Movement->bReverseAsBrake;
			bNeutralHoldActive = true;
		}
		// 양쪽 기능 모두 중립을 덮어쓸 수 있어 꺼 둡니다. 스로틀은 실제 엔진에 전달됩니다.
		Movement->bReverseAsBrake = false;
		Movement->SetUseAutomaticGears(false);
		Movement->SetTargetGear(0, true);
		if (!Participant->IsPreStartRevvingAllowed())
		{
			// 어댑터 없이 PC에 직접 페달이 연결된 차도 종료 후에는 스로틀을 차단합니다.
			Movement->SetThrottleInput(0.f);
		}
		return;
	}

	if (bNeutralHoldActive)
	{
		ReleaseNeutralHold();
		// 출발 프레임에는 남아 있던 브레이크 입력이 다시 후진을 고르지 않게 합니다.
		Movement->bReverseAsBrake = false;
		return;
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
