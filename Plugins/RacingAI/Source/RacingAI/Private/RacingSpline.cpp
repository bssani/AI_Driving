#include "RacingSpline.h"

#include "Components/SplineComponent.h"
#include "RacingAIModule.h"

ARacingSpline::ARacingSpline()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);

	Spline->SetClosedLoop(true);
	Spline->SetMobility(EComponentMobility::Static);
}

void ARacingSpline::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	RebuildCurvature();
}

void ARacingSpline::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 OnConstruction으로 이미 만들어지지만, 스플라인이 코드로 생성된 경우를 위해 한 번 더 보장합니다.
	if (CurvatureLUT.Num() == 0)
	{
		RebuildCurvature();
	}

	UE_LOG(LogRacingAI, Log, TEXT("%s: 길이 %.0fcm, 닫힘=%s, 곡률 샘플 %d개"),
		*GetName(), GetLength(), IsClosed() ? TEXT("예") : TEXT("아니오"), CurvatureLUT.Num());
}

//------------------------------------------------------------------------------
// 기본 질의
//------------------------------------------------------------------------------

float ARacingSpline::GetLength() const
{
	return Spline ? Spline->GetSplineLength() : 0.f;
}

bool ARacingSpline::IsClosed() const
{
	return Spline && Spline->IsClosedLoop();
}

float ARacingSpline::WrapDistance(float Distance) const
{
	const float Length = GetLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	if (!IsClosed())
	{
		return FMath::Clamp(Distance, 0.f, Length);
	}

	// FMath::Fmod는 음수 입력에 대해 음수를 돌려주므로 한 바퀴 더해 보정합니다.
	const float Wrapped = FMath::Fmod(Distance, Length);

	return Wrapped < 0.f ? Wrapped + Length : Wrapped;
}

float ARacingSpline::GetForwardDelta(float From, float To) const
{
	const float Length = GetLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	if (!IsClosed())
	{
		return To - From;
	}

	float Delta = WrapDistance(To) - WrapDistance(From);
	if (Delta < 0.f)
	{
		Delta += Length;
	}

	return Delta;
}

float ARacingSpline::FindDistanceAtLocation(const FVector& WorldLocation) const
{
	if (!Spline)
	{
		return 0.f;
	}

	const float Key = Spline->FindInputKeyClosestToWorldLocation(WorldLocation);

	return Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
}

FVector ARacingSpline::GetLocationAtDistance(float Distance) const
{
	if (!Spline)
	{
		return FVector::ZeroVector;
	}

	return Spline->GetLocationAtDistanceAlongSpline(WrapDistance(Distance), ESplineCoordinateSpace::World);
}

FVector ARacingSpline::GetDirectionAtDistance(float Distance) const
{
	if (!Spline)
	{
		return FVector::ForwardVector;
	}

	return Spline->GetDirectionAtDistanceAlongSpline(WrapDistance(Distance), ESplineCoordinateSpace::World).GetSafeNormal();
}

FVector ARacingSpline::GetRightAtDistance(float Distance) const
{
	if (!Spline)
	{
		return FVector::RightVector;
	}

	return Spline->GetRightVectorAtDistanceAlongSpline(WrapDistance(Distance), ESplineCoordinateSpace::World).GetSafeNormal();
}

FVector ARacingSpline::GetOffsetLocationAtDistance(float Distance, float LateralOffset) const
{
	const float Clamped = FMath::Clamp(LateralOffset, -TrackHalfWidth, TrackHalfWidth);

	return GetLocationAtDistance(Distance) + GetRightAtDistance(Distance) * Clamped;
}

float ARacingSpline::GetLateralOffsetAtLocation(const FVector& WorldLocation) const
{
	const float Distance = FindDistanceAtLocation(WorldLocation);
	const FVector ToPoint = WorldLocation - GetLocationAtDistance(Distance);

	return FVector::DotProduct(ToPoint, GetRightAtDistance(Distance));
}

//------------------------------------------------------------------------------
// 결승선
//------------------------------------------------------------------------------

float ARacingSpline::GetDistanceFromFinishLine(float SplineDistance) const
{
	if (!IsClosed())
	{
		// 열린 코스는 감싸지 않습니다. 결승선 앞이면 음수가 나오며, 그 부호로
		// 아직 통과하지 않았음을 알 수 있습니다.
		return SplineDistance - FinishLineDistance;
	}

	return WrapDistance(SplineDistance - FinishLineDistance);
}

float ARacingSpline::GetSplineDistanceFromFinishOffset(float OffsetFromFinish) const
{
	return WrapDistance(FinishLineDistance + OffsetFromFinish);
}

FVector ARacingSpline::GetFinishLineLocation() const
{
	return GetLocationAtDistance(FinishLineDistance);
}

//------------------------------------------------------------------------------
// 곡률
//------------------------------------------------------------------------------

void ARacingSpline::RebuildCurvature()
{
	CurvatureLUT.Reset();

	const float Length = GetLength();
	if (!Spline || Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	LUTStep = FMath::Max(50.f, CurvatureSampleStep);
	LUTLength = Length;

	const int32 SampleCount = FMath::Max(2, FMath::CeilToInt(Length / LUTStep) + 1);
	CurvatureLUT.SetNumUninitialized(SampleCount);

	// 곡률은 접선 방향의 변화율입니다. 샘플 간격의 절반만큼 앞뒤로 떨어진 두 접선의
	// 사잇각을 그 호 길이로 나누면 됩니다. 스플라인을 두 번 미분하는 것보다
	// 노이즈에 강합니다.
	const float Half = LUTStep * 0.5f;

	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float Distance = FMath::Min(Index * LUTStep, Length);

		const FVector Before = GetDirectionAtDistance(Distance - Half);
		const FVector After = GetDirectionAtDistance(Distance + Half);

		const float Dot = FMath::Clamp(FVector::DotProduct(Before, After), -1.f, 1.f);
		const float AngleRadians = FMath::Acos(Dot);

		CurvatureLUT[Index] = AngleRadians / LUTStep;
	}
}

float ARacingSpline::GetCurvatureAtDistance(float Distance) const
{
	if (CurvatureLUT.Num() < 2 || LUTStep <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float Wrapped = WrapDistance(Distance);
	const float Position = Wrapped / LUTStep;

	const int32 Last = CurvatureLUT.Num() - 1;
	int32 Index0 = FMath::FloorToInt(Position);
	float Alpha = Position - Index0;

	if (IsClosed())
	{
		Index0 = ((Index0 % Last) + Last) % Last;
	}
	else
	{
		Index0 = FMath::Clamp(Index0, 0, Last);
		if (Index0 == Last)
		{
			return CurvatureLUT[Last];
		}
	}

	const int32 Index1 = IsClosed() ? (Index0 + 1) % Last : FMath::Min(Index0 + 1, Last);

	return FMath::Lerp(CurvatureLUT[Index0], CurvatureLUT[Index1], Alpha);
}

float ARacingSpline::GetCornerSpeedLimit(float Distance, float LateralAccel) const
{
	const float Curvature = GetCurvatureAtDistance(Distance);

	// 곡률이 사실상 0인 직선 구간에서는 제한이 없습니다. 호출부가 직선 최고속도로 자릅니다.
	if (Curvature <= 1e-7f)
	{
		return TNumericLimits<float>::Max();
	}

	return FMath::Sqrt(FMath::Max(0.f, LateralAccel) / Curvature);
}

float ARacingSpline::GetTargetSpeedAtDistance(float Distance, float LateralAccel, float BrakingDecel, float Horizon, float StraightMaxSpeed) const
{
	// 상한을 지정하지 않았어도 유한한 값을 씁니다. FLT_MAX를 그대로 돌려주면
	// 호출부에서 러버밴딩 계수를 곱하는 순간 inf가 되고, 디버그 표시도 읽을 수 없습니다.
	constexpr float UnboundedSpeed = 100000.f;   // 3600 km/h. 사실상 무제한
	const float Ceiling = StraightMaxSpeed > 0.f ? StraightMaxSpeed : UnboundedSpeed;

	if (CurvatureLUT.Num() < 2)
	{
		return Ceiling;
	}

	const float Step = FMath::Max(50.f, LUTStep);
	const float Decel = FMath::Max(1.f, BrakingDecel);

	float Target = Ceiling;

	// 전방을 훑으며, 각 지점의 코너 속도 한계를 지키려면 "지금" 얼마여야 하는지를 역산합니다.
	// v_now = sqrt(v_limit^2 + 2 * a * d) 이며, 그중 가장 낮은 값이 곧 제동 시점을 결정합니다.
	for (float Ahead = 0.f; Ahead <= Horizon; Ahead += Step)
	{
		const float Limit = GetCornerSpeedLimit(Distance + Ahead, LateralAccel);
		if (Limit >= TNumericLimits<float>::Max())
		{
			continue;
		}

		const float Allowed = FMath::Sqrt(Limit * Limit + 2.f * Decel * Ahead);

		Target = FMath::Min(Target, Allowed);
	}

	return FMath::Min(Target, Ceiling);
}
