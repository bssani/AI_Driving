#include "RaceParticipantComponent.h"

#include "GameFramework/Actor.h"
#include "RaceDirectorSubsystem.h"
#include "RacingSpline.h"

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

void URaceParticipantComponent::ResetProgress()
{
	Progress = FRaceProgress();
	PreviousDistance = 0.f;
	bHasPreviousDistance = false;
	bFinished = false;
}

void URaceParticipantComponent::RefreshProgress(const ARacingSpline& Track, float DeltaTime)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector Location = Owner->GetActorLocation();
	const float Distance = Track.FindDistanceAtLocation(Location);
	const float Length = Track.GetLength();

	// 랩 판정: 거리가 뒤로 크게 튀었다면 시작선을 넘은 것입니다. 절반 이상 되감겼을 때만
	// 인정하므로, 시작선 부근에서 조금씩 앞뒤로 흔들려도 랩이 중복 증가하지 않습니다.
	if (bHasPreviousDistance && Track.IsClosed() && Length > KINDA_SMALL_NUMBER)
	{
		const float RawDelta = Distance - PreviousDistance;

		if (RawDelta < -Length * 0.5f)
		{
			++Progress.Lap;
		}
		else if (RawDelta > Length * 0.5f)
		{
			// 역방향으로 시작선을 넘은 경우입니다.
			--Progress.Lap;
		}
	}

	PreviousDistance = Distance;
	bHasPreviousDistance = true;

	Progress.DistanceAlongSpline = Distance;
	Progress.TotalDistance = Progress.Lap * Length + Distance;
	Progress.LateralOffset = Track.GetLateralOffsetAtLocation(Location);

	const FVector Forward = Owner->GetActorForwardVector();
	Progress.ForwardSpeed = FVector::DotProduct(Owner->GetVelocity(), Forward);

	const FVector TrackDirection = Track.GetDirectionAtDistance(Distance);
	const float Dot = FMath::Clamp(FVector::DotProduct(Forward.GetSafeNormal2D(), TrackDirection.GetSafeNormal2D()), -1.f, 1.f);
	Progress.HeadingErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
}
