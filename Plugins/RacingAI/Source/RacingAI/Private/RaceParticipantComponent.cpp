#include "RaceParticipantComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "RaceDirectorSubsystem.h"
#include "RacingSpline.h"
#include "RacingVehicleInput.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "RacingAIModule.h"
#include "PhysicsEngine/BodyInstance.h"

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
	SetInputLocked(false);
	const AActor* Owner = GetOwner();

	UE_LOG(LogRacingAI, Log, TEXT("%s: 레이스 참가자 해제 (%s)"),
		*GetNameSafe(Owner), *UEnum::GetValueAsString(EndPlayReason));

	// 차는 남아 있는데 참가자만 사라지면 레이스가 그 차를 잊습니다. 순위도, 완주도, 끝났을 때
	// 세우기도 그 차에는 더 이상 닿지 않는데 아무 오류도 나지 않습니다. 원인을 찾을 수 있게 남깁니다.
	if (EndPlayReason == EEndPlayReason::Destroyed && Owner && !Owner->IsActorBeingDestroyed())
	{
		UE_LOG(LogRacingAI, Warning, TEXT("%s: 차는 남아 있는데 레이스 참가자 컴포넌트가 파괴됩니다. 이 차는 이제 레이스에서 빠집니다."),
			*GetNameSafe(Owner));

#if !UE_BUILD_SHIPPING
		FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
#endif
	}

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

void URaceParticipantComponent::SetInputLocked(bool bLocked, bool bAllowPreStartRevving)
{
	bStopping = false;
	const bool bWasHardLocked = bInputLocked && !bPreStartRevvingAllowed;
	// 반복 호출도 그리드 재배치 후의 새 위치에 제약을 걸어야 합니다.
	SetPhysicsHold(false);
	bInputLocked = bLocked;
	bPreStartRevvingAllowed = bLocked && bAllowPreStartRevving;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 출발 전에는 페달을 받습니다. 종료 후에만 폰 입력도 막습니다.
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (bLocked && !bPreStartRevvingAllowed && Pawn->InputEnabled())
			{
				Pawn->DisableInput(PC);
				bPawnInputDisabledByHold = true;
			}
			else if ((!bLocked || bPreStartRevvingAllowed) && bPawnInputDisabledByHold)
			{
				Pawn->EnableInput(PC);
				bPawnInputDisabledByHold = false;
			}
		}
	}

	SetControllerTickPrerequisite(bLocked);
	if (bWasHardLocked && (!bLocked || bPreStartRevvingAllowed))
	{
		if (UObject* Target = ResolveVehicleInputTarget())
		{
			IRacingVehicleInput::Execute_ApplyBrake(Target, 0.f);
		}
	}

	if (bLocked)
	{
		LockedTransform = Owner->GetActorTransform();
		SetComponentTickEnabled(true);
		SetPhysicsHold(true);
		HoldVehicleStill();
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

void URaceParticipantComponent::SetPhysicsHold(bool bHold)
{
	if (!bHold)
	{
		if (UPrimitiveComponent* Root = HeldPhysicsRoot.Get())
		{
			if (FBodyInstance* Body = Root->GetBodyInstance())
			{
				Body->bLockXTranslation = (SavedAxisLocks & 1) != 0;
				Body->bLockYTranslation = (SavedAxisLocks & 2) != 0;
				Body->bLockZTranslation = (SavedAxisLocks & 4) != 0;
				Body->bLockXRotation = (SavedAxisLocks & 8) != 0;
				Body->bLockYRotation = (SavedAxisLocks & 16) != 0;
				Body->bLockZRotation = (SavedAxisLocks & 32) != 0;
				Body->SetDOFLock(static_cast<EDOFMode::Type>(SavedDOFMode));
			}
		}
		HeldPhysicsRoot.Reset();
		return;
	}

	if (HeldPhysicsRoot.IsValid()) return;
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	FBodyInstance* Body = Root && Root->IsSimulatingPhysics() ? Root->GetBodyInstance() : nullptr;
	if (!Body || !Body->IsValidBodyInstance()) return;

	HeldPhysicsRoot = Root;
	SavedDOFMode = static_cast<uint8>(Body->DOFMode.GetValue());
	SavedAxisLocks = (Body->bLockXTranslation ? 1 : 0) | (Body->bLockYTranslation ? 2 : 0)
		| (Body->bLockZTranslation ? 4 : 0) | (Body->bLockXRotation ? 8 : 0)
		| (Body->bLockYRotation ? 16 : 0) | (Body->bLockZRotation ? 32 : 0);
	Body->bLockXTranslation = true;
	Body->bLockYTranslation = true;
	Body->bLockXRotation = true;
	Body->bLockYRotation = true;
	Body->bLockZRotation = true;
	// Z의 기존 설정은 유지합니다. 기본 차량은 높이가 자유로워 노면에 정착합니다.
	Body->SetDOFLock(EDOFMode::SixDOF);
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

	// 종료 후 잠금은 입력을 지우지만, 출발 대기는 실제 엔진을 중립에서 돌립니다.
	if (UObject* Target = !bPreStartRevvingAllowed ? ResolveVehicleInputTarget() : nullptr)
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

	SetPhysicsHold(true);
	// 중력과 서스펜션은 높이를 정착시킵니다. 수평/회전은 물리 스텝 안에서도 제약됩니다.
	const FVector Velocity = Root->GetPhysicsLinearVelocity();

	Root->SetPhysicsLinearVelocity(FVector(0.f, 0.f, Velocity.Z));
	Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	// 유효한 물리 바디가 없는 구현체만 기존 위치 보정을 사용합니다.
	const FVector Current = Owner->GetActorLocation();
	const FVector Locked = LockedTransform.GetLocation();

	if (!HeldPhysicsRoot.IsValid() && FVector::DistSquared2D(Current, Locked) > FMath::Square(LockedDriftTolerance))
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
	else if (bStopping)
	{
		UpdateStopping(DeltaTime);
	}
	else
	{
		SetControllerTickPrerequisite(false);
		SetComponentTickEnabled(false);
	}
}

void URaceParticipantComponent::SetControllerTickPrerequisite(bool bEnable)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	if (!PC)
	{
		return;
	}

	// 붙잡는 틱은 그 프레임의 입력이 처리된 뒤에 돌아야 합니다. 컨트롤러가
	// 먼저 틱하도록 걸어 두면 순서가 프레임마다 흔들리지 않습니다.
	if (bEnable)
	{
		PrimaryComponentTick.AddPrerequisite(PC, PC->PrimaryActorTick);
	}
	else
	{
		PrimaryComponentTick.RemovePrerequisite(PC, PC->PrimaryActorTick);
	}
}

//------------------------------------------------------------------------------
// 레이스 종료 정지
//------------------------------------------------------------------------------

void URaceParticipantComponent::StopAndHold(float Deceleration)
{
	const AActor* Owner = GetOwner();

	if (!Owner || bInputLocked)
	{
		return;
	}

	StoppingDeceleration = FMath::Max(1.f, Deceleration);

	// 이미 세우는 중이면 감속도만 바꿉니다. 허용 속도를 지금 속도로 되돌리면 서다 말고 다시 풀립니다.
	if (bStopping)
	{
		return;
	}

	UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	const FVector Velocity = Root && Root->IsSimulatingPhysics() ? Root->GetPhysicsLinearVelocity() : Owner->GetVelocity();

	bStopping = true;
	StoppingAllowedSpeed = static_cast<float>(FVector(Velocity.X, Velocity.Y, 0.f).Size());

	SetControllerTickPrerequisite(true);
	SetComponentTickEnabled(true);
}

void URaceParticipantComponent::UpdateStopping(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 이 속도 아래로 떨어지면 붙잡기로 넘어갑니다. 그때 속도를 0으로 눌러도 느껴지지 않을 만큼 느립니다.
	constexpr float HoldBelowSpeed = 50.f;

	StoppingAllowedSpeed = FMath::Max(0.f, StoppingAllowedSpeed - StoppingDeceleration * DeltaTime);

	// 인터페이스가 있으면 스로틀도 놓게 합니다. 엔진이 헛돌며 우는 소리가 나지 않습니다.
	if (UObject* Target = ResolveVehicleInputTarget())
	{
		IRacingVehicleInput::Execute_ApplyThrottle(Target, 0.f);
	}

	// 속도의 크기만 누릅니다. 방향도 회전도 그대로 두므로 핸들은 계속 먹습니다.
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		if (Root->IsSimulatingPhysics())
		{
			const FVector Velocity = Root->GetPhysicsLinearVelocity();
			const FVector Flat(Velocity.X, Velocity.Y, 0.f);
			const double Speed = Flat.Size();

			if (Speed > StoppingAllowedSpeed && Speed > KINDA_SMALL_NUMBER)
			{
				const FVector Capped = Flat * (StoppingAllowedSpeed / Speed);

				Root->SetPhysicsLinearVelocity(FVector(Capped.X, Capped.Y, Velocity.Z));
			}
		}
	}

	if (StoppingAllowedSpeed <= HoldBelowSpeed)
	{
		SetInputLocked(true);
	}
}

//------------------------------------------------------------------------------
// 트랙 위로 되돌리기
//------------------------------------------------------------------------------

bool URaceParticipantComponent::RespawnOnTrack()
{
	// 출발 전에는 옮기지 않습니다. 그리드를 흐트러뜨리면 출발 순서가 엉킵니다.
	if (bInputLocked)
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	URaceDirectorSubsystem* Director = URaceDirectorSubsystem::Get(this);
	ARacingSpline* Track = Director ? Director->GetTrack() : nullptr;

	if (!Owner || !Track)
	{
		UE_LOG(LogRacingAI, Warning, TEXT("%s: 트랙을 찾지 못해 트랙 위로 옮기지 못했습니다."), *GetNameSafe(Owner));

		return false;
	}

	const FVector Location = Owner->GetActorLocation();
	const float Distance = Track->FindDistanceAtLocation(Location);

	// 원래 있던 쪽은 유지하되 벽에서 떼어 놓습니다. 벽 옆에 그대로 놓으면 곧바로 또 박습니다.
	const float Limit = Track->TrackHalfWidth * RespawnLaneFraction;
	const float Lateral = FMath::Clamp(Track->GetLateralOffsetAtLocation(Location), -Limit, Limit);

	if (!PlaceOnTrack(*Track, Distance, Lateral))
	{
		return false;
	}

	UE_LOG(LogRacingAI, Log, TEXT("%s: 트랙 위로 옮겼습니다. 스플라인 %.0fcm, 좌우 %.0fcm"),
		*GetNameSafe(Owner), Distance, Lateral);

	return true;
}

bool URaceParticipantComponent::PlaceOnTrack(const ARacingSpline& Track, float SplineDistance, float LateralOffset)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!Owner || !World)
	{
		return false;
	}

	const FVector OnTrack = Track.GetOffsetLocationAtDistance(SplineDistance, LateralOffset);
	const FRotator Facing = Track.GetDirectionAtDistance(SplineDistance).Rotation();

	// 바퀴 바닥이 액터 원점보다 얼마나 아래 있는지는 차마다 다릅니다. 원점을 노면에서 고정 높이에
	// 두면 원점이 높은 차는 바퀴가 노면에 박히고, 낮은 차는 떨어지며 튑니다.
	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	Owner->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	const double OriginAboveBottom = FMath::Max(0.0, Owner->GetActorLocation().Z - (BoundsOrigin.Z - BoundsExtent.Z));

	// 스플라인 높이가 노면과 같다는 보장이 없어, 그 자리 위에서 아래로 노면을 찾습니다.
	// 못 찾으면 스플라인보다 넉넉히 위에 놓아 떨어지게 둡니다.
	FVector Location = OnTrack + FVector(0.f, 0.f, 150.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RacingPlaceOnTrack), false, Owner);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FHitResult Hit;

	if (World->LineTraceSingleByObjectType(Hit, OnTrack + FVector(0.f, 0.f, 300.f), OnTrack - FVector(0.f, 0.f, 1200.f), Objects, Params))
	{
		Location.Z = Hit.ImpactPoint.Z + OriginAboveBottom + RespawnClearance;
	}

	SetPhysicsHold(false);
	Owner->SetActorTransform(FTransform(Facing, Location), false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	// 잠겨 있던 차라면 붙잡을 자리도 옮긴 자리입니다. 안 그러면 다음 틱에 원래 자리로 끌려갑니다.
	LockedTransform = Owner->GetActorTransform();
	if (bInputLocked) SetPhysicsHold(true);

	return true;
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
