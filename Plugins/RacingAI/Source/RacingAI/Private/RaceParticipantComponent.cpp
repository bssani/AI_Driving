#include "RaceParticipantComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "RaceDirectorSubsystem.h"
#include "RacingSpline.h"
#include "RacingVehicleInput.h"

URaceParticipantComponent::URaceParticipantComponent()
{
	// 진행도 갱신은 Director가 일괄 처리합니다. 컴포넌트 개별 틱은 필요 없습니다.
	PrimaryComponentTick.bCanEverTick = false;
}

void URaceParticipantComponent::BeginPlay()
{
	Super::BeginPlay();

	if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
	{
		Director->RegisterParticipant(this);
	}
}

void URaceParticipantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this))
	{
		Director->UnregisterParticipant(this);
	}

	Super::EndPlay(EndPlayReason);
}

void URaceParticipantComponent::ResetProgress(int32 InitialLap)
{
	Progress = FRaceProgress();
	Progress.Lap = InitialLap;

	PreviousDistance = 0.f;
	bHasPreviousDistance = false;

	bFinished = false;
	bDidNotFinish = false;
	FinishPosition = 0;
	FinishTimeSeconds = 0.f;
}

void URaceParticipantComponent::MarkFinished(int32 InFinishPosition, float InFinishTimeSeconds)
{
	bFinished = true;
	bDidNotFinish = false;
	FinishPosition = InFinishPosition;
	FinishTimeSeconds = InFinishTimeSeconds;
}

void URaceParticipantComponent::MarkDidNotFinish(int32 InFinishPosition)
{
	bFinished = false;
	bDidNotFinish = true;
	FinishPosition = InFinishPosition;
	FinishTimeSeconds = 0.f;
}

void URaceParticipantComponent::SetInputLocked(bool bLocked)
{
	if (bInputLocked == bLocked)
	{
		return;
	}

	bInputLocked = bLocked;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 사람의 입력을 막습니다.
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (bLocked)
			{
				Pawn->DisableInput(PC);
			}
			else
			{
				Pawn->EnableInput(PC);
			}
		}
	}

	// 입력만 막으면 경사에서 굴러갑니다. 차량이 인터페이스를 구현했다면 붙잡아 둡니다.
	UObject* Target = nullptr;

	if (Owner->GetClass()->ImplementsInterface(URacingVehicleInput::StaticClass()))
	{
		Target = Owner;
	}
	else
	{
		TArray<UActorComponent*> Components;
		Owner->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component && Component->GetClass()->ImplementsInterface(URacingVehicleInput::StaticClass()))
			{
				Target = Component;
				break;
			}
		}
	}

	if (Target)
	{
		IRacingVehicleInput::Execute_ApplyThrottle(Target, 0.f);
		IRacingVehicleInput::Execute_ApplyBrake(Target, bLocked ? 1.f : 0.f);
	}
}

int32 URaceParticipantComponent::RefreshProgress(const ARacingSpline& Track, float DeltaTime)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0;
	}

	const FVector Location = Owner->GetActorLocation();
	const float Distance = Track.FindDistanceAtLocation(Location);
	const float Length = Track.GetLength();

	// 랩과 순위는 결승선을 원점으로 잽니다. 스플라인이 시작하는 자리를 기준으로 삼으면
	// 결승선을 다른 곳에 둘 수 없고, 출발 그리드와 결승 지점이 늘 같은 곳이 됩니다.
	const float LapDistance = Track.GetDistanceFromFinishLine(Distance);

	int32 LapDelta = 0;

	// 랩 판정: 거리가 뒤로 크게 튀었다면 결승선을 넘은 것입니다. 절반 이상 되감겼을 때만
	// 인정하므로, 결승선 부근에서 조금씩 앞뒤로 흔들려도 랩이 중복 증가하지 않습니다.
	if (bHasPreviousDistance && Track.IsClosed() && Length > KINDA_SMALL_NUMBER)
	{
		const float RawDelta = LapDistance - PreviousDistance;

		if (RawDelta < -Length * 0.5f)
		{
			LapDelta = 1;
		}
		else if (RawDelta > Length * 0.5f)
		{
			// 역방향으로 결승선을 넘은 경우입니다.
			LapDelta = -1;
		}
	}

	Progress.Lap += LapDelta;

	PreviousDistance = LapDistance;
	bHasPreviousDistance = true;

	Progress.DistanceAlongSpline = Distance;
	Progress.LapDistance = LapDistance;
	Progress.TotalDistance = Progress.Lap * Length + LapDistance;
	Progress.LateralOffset = Track.GetLateralOffsetAtLocation(Location);

	const FVector Forward = Owner->GetActorForwardVector();
	Progress.ForwardSpeed = FVector::DotProduct(Owner->GetVelocity(), Forward);

	const FVector TrackDirection = Track.GetDirectionAtDistance(Distance);
	const float Dot = FMath::Clamp(FVector::DotProduct(Forward.GetSafeNormal2D(), TrackDirection.GetSafeNormal2D()), -1.f, 1.f);
	Progress.HeadingErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

	return LapDelta;
}
