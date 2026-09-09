#include "RaceParticipantComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "RaceDirectorSubsystem.h"
#include "RacingSpline.h"
#include "RacingVehicleInput.h"
#include "Components/PrimitiveComponent.h"

URaceParticipantComponent::URaceParticipantComponent()
{
	// 진행도 갱신은 Director가 일괄 처리합니다. 평소에는 틱이 필요 없고,
	// 출발 전 잠금이 걸린 동안에만 켭니다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
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

	// 사람의 입력을 막습니다. 이것만으로는 폰에 바인딩된 조작만 끊깁니다.
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (bLocked)
			{
				Pawn->DisableInput(PC);

				// 붙잡는 틱은 그 프레임의 입력이 처리된 뒤에 돌아야 합니다. 컨트롤러가
				// 먼저 틱하도록 걸어 두면 순서가 프레임마다 흔들리지 않습니다.
				PrimaryComponentTick.AddPrerequisite(PC, PC->PrimaryActorTick);
			}
			else
			{
				Pawn->EnableInput(PC);
				PrimaryComponentTick.RemovePrerequisite(PC, PC->PrimaryActorTick);
			}
		}
	}

	if (bLocked)
	{
		LockedTransform = Owner->GetActorTransform();
		SetComponentTickEnabled(true);
		HoldVehicleStill();
	}
	else
	{
		SetComponentTickEnabled(false);

		if (UObject* Target = ResolveVehicleInputTarget())
		{
			IRacingVehicleInput::Execute_ApplyBrake(Target, 0.f);
		}
	}
}

UObject* URaceParticipantComponent::ResolveVehicleInputTarget()
{
	if (VehicleInputTarget.IsValid())
	{
		return VehicleInputTarget.Get();
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (Owner->GetClass()->ImplementsInterface(URacingVehicleInput::StaticClass()))
	{
		VehicleInputTarget = Owner;

		return Owner;
	}

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetClass()->ImplementsInterface(URacingVehicleInput::StaticClass()))
		{
			VehicleInputTarget = Component;

			return Component;
		}
	}

	return nullptr;
}

void URaceParticipantComponent::HoldVehicleStill()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 인터페이스가 있으면 정식 경로로 눌러 둡니다. 엔진이 헛돌지 않아 소리도 맞습니다.
	if (UObject* Target = ResolveVehicleInputTarget())
	{
		IRacingVehicleInput::Execute_ApplySteering(Target, 0.f);
		IRacingVehicleInput::Execute_ApplyThrottle(Target, 0.f);
		IRacingVehicleInput::Execute_ApplyBrake(Target, 1.f);
	}

	// 그리고 물리를 직접 붙잡습니다. 인터페이스가 없는 차량도, 컨트롤러 쪽에 바인딩된
	// 스티어링 휠도, 여기서는 예외가 없습니다.
	UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent());

	if (!Root || !Root->IsSimulatingPhysics())
	{
		return;
	}

	// 아래로 떨어지는 것은 그대로 둡니다. 속도를 통째로 0으로 만들면 중력까지 지워져,
	// 차가 스폰된 높이에 그대로 떠 있게 됩니다. 실제로 그렇게 만들어 놓고 재 보니 잠긴 차가
	// 72cm 공중에 있었습니다. 위로 튀어 오르는 것만 막고, 앞뒤 좌우로 나가는 것을 막습니다.
	const FVector Velocity = Root->GetPhysicsLinearVelocity();

	Root->SetPhysicsLinearVelocity(FVector(0.f, 0.f, FMath::Min(Velocity.Z, 0.f)));
	Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	// 속도를 눌러도 한 스텝 안에서는 조금씩 밀립니다. 쌓여서 출발선을 넘기 전에 되돌립니다.
	// 높이는 재지도 되돌리지도 않습니다. 가라앉는 중인 차를 다시 들어 올리게 됩니다.
	const FVector Current = Owner->GetActorLocation();
	const FVector Locked = LockedTransform.GetLocation();

	if (FVector::DistSquared2D(Current, Locked) > FMath::Square(LockedDriftTolerance))
	{
		FTransform Restore = LockedTransform;
		Restore.SetLocation(FVector(Locked.X, Locked.Y, Current.Z));

		Owner->SetActorTransform(Restore, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void URaceParticipantComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bInputLocked)
	{
		HoldVehicleStill();
	}
	else
	{
		SetComponentTickEnabled(false);
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
