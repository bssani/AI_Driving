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

		// 스턱 타이머를 음수에서 시작해 출발 가속 구간을 유예합니다. 이게 없으면
		// 정지 상태의 속도가 판정 문턱을 넘기 전에 타이머가 먼저 차서, 출발 신호마다
		// 모든 AI가 갇힌 것으로 오인되어 뒤로 움찔합니다.
		StuckTimer = -GetEffectiveProfile().LaunchGraceSeconds;
	}
}

void URacingAIComponent::EnterFinished()
{
	State = ERacingAIState::Finished;
	StuckTimer = 0.f;
	ReverseTimer = 0.f;

	if (UObject* Target = VehicleInput.GetObject())
	{
		IRacingVehicleInput::Execute_SetReverseGear(Target, false);
	}
}

void URacingAIComponent::ResetForNewRace(float InLaneOffset)
{
	BaseLaneOffset = InLaneOffset;
	CurrentLaneOffset = InLaneOffset;
	TargetLaneOffset = InLaneOffset;

	CurrentSteering = 0.f;
	CurrentThrottle = 0.f;
	CurrentBrake = 0.f;
	TargetSpeed = 0.f;
	FreeTargetSpeed = 0.f;
	SpeedScale = 1.f;

	StuckTimer = 0.f;
	ReverseTimer = 0.f;
	FlippedTimer = 0.f;
	StuckAttempts = 0;
	LastProgressDistance = 0.f;
	bHasProgressReference = false;
	TimeSinceAdvance = 0.f;

	RacerAhead = FRacerAhead();
	BlockingRacer = FRacerAhead();
	bIntendToPass = false;

	EnterWaiting();
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

	if (State == ERacingAIState::Finished)
	{
		// 완주 후에도 레이싱 라인은 계속 따라갑니다. 조향을 놓으면 결승선 직후
		// 트랙 밖으로 흘러나가 다음 회차의 그리드 리셋 전까지 이상하게 서 있게 됩니다.
		const float FinishSteering = ComputeSteering(Track, DeltaTime);
		const float FinishBrake = FMath::Abs(Progress.ForwardSpeed) > 50.f ? 0.4f : 1.f;

		ApplyInputs(FinishSteering, 0.f, FinishBrake);
		TargetSpeed = 0.f;

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

	// 같은 차선에서 진로를 막는 차량에 대한 제동입니다. Director가 속도에 맞춰 넓힌
	// 범위로 찾아 주므로, 여기서는 "지금 속도로 저 차 속도까지 줄일 수 있는가"만 봅니다.
	if (BlockingRacer.bValid)
	{
		const float Clearance = BlockingRacer.bIsPlayer ? P.FollowGap + P.PlayerExtraClearance : P.FollowGap;

		// 앞차에 붙기 전까지 쓸 수 있는 여유 거리입니다.
		const float Room = FMath::Max(0.f, BlockingRacer.Gap - Clearance);
		const float TheirSpeed = FMath::Max(0.f, BlockingRacer.ForwardSpeed);

		// v = sqrt(u^2 + 2*a*d). 코너 제동 시점을 역산할 때와 같은 식입니다.
		// 앞차가 서 있으면 u가 0이 되어 여유 거리를 다 쓰고 정확히 멈춥니다.
		const float SafeSpeed = FMath::Sqrt(TheirSpeed * TheirSpeed + 2.f * P.BrakingDecel * Room);

		Target = FMath::Min(Target, SafeSpeed);

		// 멈춘 차는 조향을 해도 옆으로 가지 못합니다. 추월하려면 굴러가야 하므로
		// 추돌 하한 바깥에서는 최소 속도를 유지해 비켜설 여지를 만듭니다.
		if (bIntendToPass && BlockingRacer.Gap > P.HardStopGap)
		{
			Target = FMath::Max(Target, FMath::Min(P.MinOvertakeSpeed, FreeTargetSpeed));
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

	// 트랙 폭에 비례해서 본다. 절대 거리로 두면 넓은 시험장에서 잡은 값이 좁은 서킷에서는
	// 아무 때도 걸리지 않아, 벽에 붙어 못 나오는 차를 구조하지 못한다.
	const float OffTrackLimit = Track.TrackHalfWidth * P.OffTrackRespawnWidthScale;

	const bool bOffTrack = P.OffTrackRespawnWidthScale > 0.f
		&& FMath::Abs(Progress.LateralOffset) > OffTrackLimit;

	if (FlippedTimer >= P.FlippedTimeToRespawn || bOffTrack)
	{
		RespawnOnTrack(Track);

		return true;
	}

	// 갇힘 판정. 얼마나 빠른가가 아니라 트랙을 따라 얼마나 나아가는가로 본다.
	//
	// 속도로 재면 벽을 긁는 차를 놓친다. 바퀴가 돌고 차체가 벽을 따라 미끄러지면 속도계는
	// 멀쩡한 값을 내지만 결승선에는 조금도 가까워지지 않고, 그대로 경기가 끝난다.
	//
	// 거리 차가 아니라 속도로 재는 이유는 갱신 주기 때문이다. AI는 20Hz로 판단하므로 한 번에
	// 나아가는 거리가 주기에 따라 달라진다. 거리로 문턱을 두면 그 문턱이 곧 최저 속도가 되어
	// 멀쩡히 달리는 차가 갇힌 것으로 잡힌다.
	float AlongTrackSpeed = P.StuckProgressSpeed;

	if (bHasProgressReference && DeltaTime > KINDA_SMALL_NUMBER)
	{
		const float Delta = Progress.TotalDistance - LastProgressDistance;
		const float Length = Track.GetLength();

		// 랩 계수가 튀거나 재배치로 순간이동하면 거리 차가 트랙 길이만큼 뛴다. 진행으로도
		// 정지로도 읽으면 안 되므로 이번 판정은 건너뛴다.
		if (Length <= KINDA_SMALL_NUMBER || FMath::Abs(Delta) < Length * 0.5f)
		{
			AlongTrackSpeed = Delta / DeltaTime;
		}
	}

	LastProgressDistance = Progress.TotalDistance;
	bHasProgressReference = true;

	if (AlongTrackSpeed < P.StuckProgressSpeed)
	{
		StuckTimer += DeltaTime;

		if (StuckTimer >= P.StuckTimeToReverse)
		{
			// 후진을 되풀이해도 못 빠져나오는 경우가 있습니다. 벽이나 다른 차에
			// 정면으로 막히면 물러났다가 같은 곳으로 다시 돌진하기를 반복합니다.
			// 몇 번 시도해 보고 안 되면 트랙 위로 재배치해 고리를 끊습니다.
			if (++StuckAttempts > FMath::Max(1, P.MaxStuckAttempts))
			{
				UE_LOG(LogRacingAI, Log, TEXT("%s: 후진 탈출 %d회 실패, 트랙 위로 재배치합니다."),
					*GetNameSafe(GetOwner()), StuckAttempts - 1);

				RespawnOnTrack(Track);

				return true;
			}

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
		StuckAttempts = 0;
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

	const URacingAIProfile& P = GetEffectiveProfile();

	// 갇힌 자리 그대로, 그것도 그리드 차선으로 되돌리고 있었다. 그리드 차선은 출발할 때 서
	// 있던 자리일 뿐이어서 좁은 트랙에서는 도로 밖일 수 있고, 그러면 같은 벽으로 돌아가 같은
	// 자리에 다시 갇힌다. 달리던 라인 위로 돌려놓는다.
	const float Distance = Progress.DistanceAlongSpline + P.RespawnAheadDistance;

	const FVector Location = Track.GetOffsetLocationAtDistance(Distance, P.RacingLineOffset) + FVector(0.f, 0.f, 150.f);
	const FRotator Rotation = Track.GetDirectionAtDistance(Distance).Rotation();

	Owner->SetActorTransform(FTransform(Rotation, Location), false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	FlippedTimer = 0.f;
	StuckAttempts = 0;

	// 순간이동한 뒤의 거리 차는 진행이 아니므로 기준을 버린다
	bHasProgressReference = false;
	CurrentSteering = 0.f;
	State = ERacingAIState::Racing;

	// 재배치 직후에도 속도가 0이라 곧바로 갇힘으로 오인됩니다. 출발과 같은 유예를 줍니다.
	StuckTimer = -GetEffectiveProfile().LaunchGraceSeconds;

	if (UObject* Target = VehicleInput.GetObject())
	{
		IRacingVehicleInput::Execute_SetReverseGear(Target, false);
	}

	ApplyInputs(0.f, 0.f, 0.f);
}
