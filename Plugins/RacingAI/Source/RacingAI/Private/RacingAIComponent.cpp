#include "RacingAIComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "RacingAIModule.h"
#include "RacingAIProfile.h"
#include "RacingSpline.h"
#include "RacingVehicleInput.h"

URacingAIComponent::URacingAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bIsPlayer = false;
}

void URacingAIComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveVehicleInput();

	CurrentLaneOffset = BaseLaneOffset;
	TargetLaneOffset = BaseLaneOffset;
}

const URacingAIProfile& URacingAIComponent::GetEffectiveProfile() const
{
	if (Profile)
	{
		return *Profile;
	}

	if (!FallbackProfile)
	{
		FallbackProfile = NewObject<URacingAIProfile>(const_cast<URacingAIComponent*>(this));
	}

	return *FallbackProfile;
}

void URacingAIComponent::ResolveVehicleInput()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 폰 자신이 인터페이스를 구현한 경우가 가장 흔합니다.
	if (Owner->GetClass()->ImplementsInterface(URacingVehicleInput::StaticClass()))
	{
		VehicleInput.SetObject(Owner);
		VehicleInput.SetInterface(Cast<IRacingVehicleInput>(Owner));

		return;
	}

	// 아니면 어댑터 컴포넌트를 찾습니다. Chaos 차량은 이 경로를 씁니다.
	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetClass()->ImplementsInterface(URacingVehicleInput::StaticClass()))
		{
			VehicleInput.SetObject(Component);
			VehicleInput.SetInterface(Cast<IRacingVehicleInput>(Component));

			return;
		}
	}

	UE_LOG(LogRacingAI, Warning,
		TEXT("%s: IRacingVehicleInput 구현체를 찾지 못했습니다. 폰에 인터페이스를 구현하거나 어댑터 컴포넌트를 붙여 주세요."),
		*Owner->GetName());
}

void URacingAIComponent::ApplyInputs(float Steering, float Throttle, float Brake)
{
	CurrentSteering = Steering;
	CurrentThrottle = Throttle;
	CurrentBrake = Brake;

	if (UObject* Target = VehicleInput.GetObject())
	{
		IRacingVehicleInput::Execute_ApplySteering(Target, Steering);
		IRacingVehicleInput::Execute_ApplyThrottle(Target, Throttle);
		IRacingVehicleInput::Execute_ApplyBrake(Target, Brake);
	}
}

void URacingAIComponent::Configure(URacingAIProfile* InProfile, float InLaneOffset, const FText& InDisplayName)
{
	Profile = InProfile;
	BaseLaneOffset = InLaneOffset;
	DisplayName = InDisplayName;

	CurrentLaneOffset = InLaneOffset;
	TargetLaneOffset = InLaneOffset;

	ResolveVehicleInput();
}

void URacingAIComponent::EnterWaiting()
{
	State = ERacingAIState::Waiting;
	StuckTimer = 0.f;
	ReverseTimer = 0.f;

	if (UObject* Target = VehicleInput.GetObject())
	{
		IRacingVehicleInput::Execute_SetReverseGear(Target, false);
	}

	ApplyInputs(0.f, 0.f, 1.f);
}

void URacingAIComponent::BeginRacing()
{
	if (State == ERacingAIState::Waiting)
	{
		State = ERacingAIState::Racing;
		StuckTimer = 0.f;
	}
}

//------------------------------------------------------------------------------
// 주행
//------------------------------------------------------------------------------

void URacingAIComponent::Advance(const ARacingSpline& Track, float DeltaTime)
{
	if (DeltaTime <= KINDA_SMALL_NUMBER || !GetOwner())
	{
		return;
	}

	// 런타임에 스폰된 경우 어댑터가 이 컴포넌트보다 늦게 붙었을 수 있으므로 늦게 한 번 더 찾습니다.
	if (!VehicleInput.GetObject())
	{
		ResolveVehicleInput();
	}

	if (State == ERacingAIState::Waiting)
	{
		ApplyInputs(0.f, 0.f, 1.f);

		return;
	}

	if (UpdateRecovery(Track, DeltaTime))
	{
		return;
	}

	const URacingAIProfile& P = GetEffectiveProfile();

	// 차선 이동은 급격하지 않게. 목표가 바뀌어도 실제 오프셋은 서서히 따라갑니다.
	const float MaxLaneStep = P.LateralOffsetRate * DeltaTime;
	CurrentLaneOffset = FMath::Clamp(TargetLaneOffset, CurrentLaneOffset - MaxLaneStep, CurrentLaneOffset + MaxLaneStep);

	const float Steering = ComputeSteering(Track, DeltaTime);

	float Throttle = 0.f;
	float Brake = 0.f;
	ComputeSpeedControl(Track, DeltaTime, Throttle, Brake);

	ApplyInputs(Steering, Throttle, Brake);
}

float URacingAIComponent::ComputeSteering(const ARacingSpline& Track, float DeltaTime)
{
	const URacingAIProfile& P = GetEffectiveProfile();
	const AActor* Owner = GetOwner();

	const float Speed = FMath::Abs(Progress.ForwardSpeed);

	// 전방 주시 거리는 거리가 아니라 시간으로 잡습니다. 이래야 저속과 고속에서 모두 안정적입니다.
	const float Lookahead = FMath::Clamp(Speed * P.LookaheadSeconds, P.MinLookahead, P.MaxLookahead);

	const FVector TargetLocation = Track.GetOffsetLocationAtDistance(
		Progress.DistanceAlongSpline + Lookahead, CurrentLaneOffset);

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector ToTarget = (TargetLocation - OwnerLocation);
	const FVector ToTargetFlat = FVector(ToTarget.X, ToTarget.Y, 0.f);

	const float Distance = ToTargetFlat.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return CurrentSteering;
	}

	// pure pursuit: 목표점을 지나는 원호의 곡률은 2 * sin(alpha) / Ld 입니다.
	// alpha는 차량 전방과 목표 방향의 사잇각입니다.
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTargetDir = ToTargetFlat / Distance;

	const float CosAlpha = FMath::Clamp(FVector::DotProduct(Forward, ToTargetDir), -1.f, 1.f);
	const float SinAlpha = FVector::CrossProduct(Forward, ToTargetDir).Z;

	const float RequiredCurvature = 2.f * SinAlpha / Distance;
	const float MaxCurvature = 1.f / FMath::Max(1.f, P.MinTurnRadius);

	float DesiredSteering = FMath::Clamp(RequiredCurvature / MaxCurvature, -1.f, 1.f) * P.SteeringGain;

	// 목표점이 뒤에 있으면(스핀 등) 곡률 공식이 방향을 잃습니다. 그때는 최대 조향으로 돌려세웁니다.
	if (CosAlpha < 0.f)
	{
		DesiredSteering = SinAlpha >= 0.f ? 1.f : -1.f;
	}

	DesiredSteering = FMath::Clamp(DesiredSteering, -1.f, 1.f);

	// 레이트 리밋. 한 프레임 만에 풀락으로 꺾이면 VR에서 즉시 눈에 띕니다.
	const float MaxStep = P.MaxSteeringRate * DeltaTime;

	return FMath::Clamp(DesiredSteering, CurrentSteering - MaxStep, CurrentSteering + MaxStep);
}

void URacingAIComponent::ComputeSpeedControl(const ARacingSpline& Track, float DeltaTime, float& OutThrottle, float& OutBrake)
{
	const URacingAIProfile& P = GetEffectiveProfile();
	const float Speed = Progress.ForwardSpeed;

	// 전방을 얼마나 볼지: 현재 속도에서 완전히 멈출 수 있는 거리에 여유를 더합니다.
	const float BrakingDistance = (Speed * Speed) / (2.f * FMath::Max(1.f, P.BrakingDecel));
	const float Horizon = FMath::Clamp(BrakingDistance * 1.5f, 1000.f, 20000.f);

	float Target = Track.GetTargetSpeedAtDistance(
		Progress.DistanceAlongSpline, P.LateralAccelBudget, P.BrakingDecel, Horizon, P.MaxSpeed);

	Target *= P.SpeedMargin;

	// 러버밴딩. Director가 산출한 계수를 여기서 한 번만 곱합니다.
	Target *= SpeedScale;

	// 앞차를 고려하기 전의 속도를 남겨 둡니다. Director의 추월 판단이 이 값을 기준으로
	// 삼아야, 앞차 때문에 멈춘 순간 판정이 뒤집히는 일이 없습니다.
	FreeTargetSpeed = Target;

	if (RacerAhead.bValid)
	{
		// 좌우로 충분히 벌어져 있으면 같은 차선이 아니므로 간격을 지킬 이유가 없습니다.
		// 이 판정이 없으면 정지한 앞차 옆에 나란히 서도 목표 속도가 0으로 묶여
		// 추월을 끝낼 수 없습니다.
		const float LateralGap = FMath::Abs(RacerAhead.LateralOffset - Progress.LateralOffset);
		const bool bSameLane = LateralGap < P.PassingLateralClearance;

		const float Clearance = RacerAhead.bIsPlayer ? P.FollowGap + P.PlayerExtraClearance : P.FollowGap;

		if (bSameLane && RacerAhead.Gap < Clearance)
		{
			// 간격이 좁을수록 앞차 속도에 가깝게, 아주 가까우면 그보다 느리게 맞춥니다.
			const float Ratio = FMath::Clamp(RacerAhead.Gap / FMath::Max(1.f, Clearance), 0.f, 1.f);
			const float Matched = FMath::Lerp(RacerAhead.ForwardSpeed * 0.85f, RacerAhead.ForwardSpeed, Ratio);

			Target = FMath::Min(Target, FMath::Max(0.f, Matched));

			// 멈춘 차는 조향을 해도 옆으로 가지 못합니다. 추월하려면 굴러가야 하므로
			// 추돌 하한 바깥에서는 최소 속도를 유지해 비켜설 여지를 만듭니다.
			if (bIntendToPass && RacerAhead.Gap > P.HardStopGap)
			{
				Target = FMath::Max(Target, FMath::Min(P.MinOvertakeSpeed, FreeTargetSpeed));
			}
		}
	}

	TargetSpeed = Target;

	const float Error = Target - Speed;

	if (Error >= 0.f)
	{
		OutThrottle = FMath::Clamp(Error * P.SpeedControlGain, 0.f, 1.f);
		OutBrake = 0.f;
	}
	else
	{
		OutThrottle = 0.f;
		OutBrake = FMath::Clamp(-Error * P.SpeedControlGain, 0.f, 1.f);
	}
}

//------------------------------------------------------------------------------
// 복구
//------------------------------------------------------------------------------

bool URacingAIComponent::UpdateRecovery(const ARacingSpline& Track, float DeltaTime)
{
	const URacingAIProfile& P = GetEffectiveProfile();
	AActor* Owner = GetOwner();

	// 후진 중이면 타이머가 다 될 때까지 유지합니다.
	if (State == ERacingAIState::Reversing)
	{
		ReverseTimer -= DeltaTime;

		if (ReverseTimer <= 0.f)
		{
			State = ERacingAIState::Racing;
			StuckTimer = 0.f;

			if (UObject* Target = VehicleInput.GetObject())
			{
				IRacingVehicleInput::Execute_SetReverseGear(Target, false);
			}
		}
		else
		{
			// 후진하며 반대로 꺾어야 원래 진행 방향으로 코를 돌릴 수 있습니다.
			ApplyInputs(ReverseSteerSign, 1.f, 0.f);

			return true;
		}
	}

	// 전복 판정. 프레임 수가 아니라 경과 시간으로 셉니다.
	const FRotator Rotation = Owner->GetActorRotation();
	const bool bFlipped = FMath::Abs(Rotation.Roll) > P.FlippedAngleDegrees
		|| FMath::Abs(Rotation.Pitch) > P.FlippedAngleDegrees;

	FlippedTimer = bFlipped ? FlippedTimer + DeltaTime : 0.f;

	const bool bOffTrack = P.OffTrackRespawnDistance > 0.f
		&& FMath::Abs(Progress.LateralOffset) > P.OffTrackRespawnDistance;

	if (FlippedTimer >= P.FlippedTimeToRespawn || bOffTrack)
	{
		RespawnOnTrack(Track);

		return true;
	}

	// 스턱 판정. 느린데 앞이 막혀 있으면 후진으로 전환합니다.
	if (FMath::Abs(Progress.ForwardSpeed) < P.StuckSpeedThreshold)
	{
		StuckTimer += DeltaTime;

		if (StuckTimer >= P.StuckTimeToReverse)
		{
			State = ERacingAIState::Reversing;
			ReverseTimer = P.ReverseDuration;

			// 트랙 중심선의 반대쪽으로 꺾어 빠져나옵니다.
			ReverseSteerSign = Progress.LateralOffset >= 0.f ? 1.f : -1.f;
			StuckTimer = 0.f;

			if (UObject* Target = VehicleInput.GetObject())
			{
				IRacingVehicleInput::Execute_SetReverseGear(Target, true);
			}

			ApplyInputs(ReverseSteerSign, 1.f, 0.f);

			return true;
		}
	}
	else
	{
		StuckTimer = 0.f;
	}

	return false;
}

void URacingAIComponent::RespawnOnTrack(const ARacingSpline& Track)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const float Distance = Progress.DistanceAlongSpline;

	const FVector Location = Track.GetOffsetLocationAtDistance(Distance, BaseLaneOffset) + FVector(0.f, 0.f, 150.f);
	const FRotator Rotation = Track.GetDirectionAtDistance(Distance).Rotation();

	Owner->SetActorTransform(FTransform(Rotation, Location), false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	FlippedTimer = 0.f;
	StuckTimer = 0.f;
	CurrentSteering = 0.f;
	State = ERacingAIState::Racing;

	if (UObject* Target = VehicleInput.GetObject())
	{
		IRacingVehicleInput::Execute_SetReverseGear(Target, false);
	}

	ApplyInputs(0.f, 0.f, 0.f);
}
