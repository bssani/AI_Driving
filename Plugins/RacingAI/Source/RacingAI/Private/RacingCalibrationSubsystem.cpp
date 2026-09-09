#include "RacingCalibrationSubsystem.h"

#include "RacingAIModule.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** 1 G를 cm/s^2로 */
	constexpr float GravityCmS2 = 980.665f;

	/** 0-100 km/h의 100 km/h를 cm/s로 */
	constexpr float HundredKmh = 2777.78f;

	FString FormatAccel(float AccelCmS2)
	{
		return FString::Printf(TEXT("%.0f cm/s^2 (%.2f G)"), AccelCmS2, AccelCmS2 / GravityCmS2);
	}

	void ShowReport(const FString& Report)
	{
		UE_LOG(LogRacingAI, Log, TEXT("%s"), *Report);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Yellow, Report);
		}
	}
}

AActor* URacingCalibrationSubsystem::ResolveSubject()
{
	if (Subject.IsValid())
	{
		return Subject.Get();
	}

	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Subject = Pawn;

		return Pawn;
	}

	return nullptr;
}

void URacingCalibrationSubsystem::StartMeasuring()
{
	Subject = nullptr;

	TopSpeed = 0.f;
	PeakLateralAccel = 0.f;
	SpeedAtPeakLateral = 0.f;
	RadiusAtPeakLateral = 0.f;
	TightestRadius = 0.f;
	PeakBrakingDecel = 0.f;
	PeakForwardAccel = 0.f;
	LaunchElapsed = 0.f;
	LaunchTime = 0.f;
	bLaunchTiming = false;
	PreviousForwardSpeed = 0.f;
	bHasPreviousSpeed = false;

	bMeasuring = true;

	UE_LOG(LogRacingAI, Log, TEXT("계측 시작. 직선 풀 스로틀, 풀 브레이크, 풀락 선회를 차례로 해 주세요."));
}

void URacingCalibrationSubsystem::StopMeasuring()
{
	if (!bMeasuring)
	{
		return;
	}

	bMeasuring = false;

	ShowReport(BuildReport());
}

void URacingCalibrationSubsystem::Tick(float DeltaTime)
{
	if (!bMeasuring || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	AActor* Actor = ResolveSubject();
	if (!Actor)
	{
		return;
	}

	const FVector Forward = Actor->GetActorForwardVector();
	const FVector Velocity = Actor->GetVelocity();
	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	const float Speed = FMath::Abs(ForwardSpeed);

	TopSpeed = FMath::Max(TopSpeed, Speed);

	// 앞뒤 가속. 제동 성능은 프로파일의 BrakingDecel에 그대로 들어갑니다.
	if (bHasPreviousSpeed)
	{
		const float Accel = (ForwardSpeed - PreviousForwardSpeed) / DeltaTime;

		if (Accel > 0.f)
		{
			PeakForwardAccel = FMath::Max(PeakForwardAccel, Accel);
		}
		else if (ForwardSpeed > MinCorneringSpeed)
		{
			// 정지 직전 한두 프레임은 감속이 과장되므로 어느 정도 달리는 동안만 봅니다.
			PeakBrakingDecel = FMath::Max(PeakBrakingDecel, -Accel);
		}
	}

	PreviousForwardSpeed = ForwardSpeed;
	bHasPreviousSpeed = true;

	// 0-100 km/h. 거의 멈춘 상태에서 움직이기 시작할 때만 셉니다.
	if (!bLaunchTiming && LaunchTime <= 0.f && ForwardSpeed > 10.f && ForwardSpeed < 200.f)
	{
		bLaunchTiming = true;
		LaunchElapsed = 0.f;
	}
	else if (bLaunchTiming)
	{
		LaunchElapsed += DeltaTime;

		if (ForwardSpeed >= HundredKmh)
		{
			LaunchTime = LaunchElapsed;
			bLaunchTiming = false;
		}
		else if (ForwardSpeed < 5.f)
		{
			// 다시 멈췄으면 이번 시도는 없던 것으로 합니다.
			bLaunchTiming = false;
		}
	}

	// 선회. 요레이트를 알면 반경과 횡가속이 바로 나옵니다. 궤적에 원을 맞추는 것보다
	// 정확하고 한 바퀴를 다 돌 필요도 없습니다. a = v * omega, r = v / omega 입니다.
	const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());

	if (!Root || Speed < MinCorneringSpeed)
	{
		return;
	}

	// 월드의 Z가 아니라 차체의 위쪽을 씁니다. 뱅크가 있는 코너에서 값이 새지 않습니다.
	const FVector AngularVelocityDeg = Root->GetPhysicsAngularVelocityInDegrees();
	const float YawRate = FMath::Abs(FMath::DegreesToRadians(
		FVector::DotProduct(AngularVelocityDeg, Actor->GetActorUpVector())));

	if (YawRate <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float LateralAccel = Speed * YawRate;
	const float Radius = Speed / YawRate;

	if (LateralAccel > PeakLateralAccel)
	{
		PeakLateralAccel = LateralAccel;
		SpeedAtPeakLateral = Speed;
		RadiusAtPeakLateral = Radius;
	}

	if (TightestRadius <= 0.f || Radius < TightestRadius)
	{
		TightestRadius = Radius;
	}
}

FString URacingCalibrationSubsystem::BuildReport() const
{
	TArray<FString> Lines;

	Lines.Add(TEXT("=== 차량 계측 결과 ==="));
	Lines.Add(FString::Printf(TEXT("  최고 속도        %.0f km/h"), TopSpeed * 0.036f));

	Lines.Add(LaunchTime > 0.f
		? FString::Printf(TEXT("  0-100 km/h       %.2f 초"), LaunchTime)
		: FString(TEXT("  0-100 km/h       측정 안 됨 (정지에서 100 km/h까지 한 번 밟아 주세요)")));

	Lines.Add(PeakForwardAccel > 0.f
		? FString::Printf(TEXT("  최대 가속        %s"), *FormatAccel(PeakForwardAccel))
		: FString(TEXT("  최대 가속        측정 안 됨")));

	Lines.Add(PeakBrakingDecel > 0.f
		? FString::Printf(TEXT("  최대 제동 감속   %s"), *FormatAccel(PeakBrakingDecel))
		: FString(TEXT("  최대 제동 감속   측정 안 됨 (달리다가 한 번 세게 밟아 주세요)")));

	if (PeakLateralAccel <= 0.f)
	{
		Lines.Add(TEXT("  최대 횡가속      측정 안 됨 (넓은 곳에서 풀락으로 몇 바퀴 돌아 주세요)"));
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("선회를 재지 않으면 프로파일 권장값을 낼 수 없습니다."));

		return FString::Join(Lines, TEXT("\n"));
	}

	Lines.Add(FString::Printf(TEXT("  최대 횡가속      %s  (%.0f km/h, 반경 %.1f m)"),
		*FormatAccel(PeakLateralAccel), SpeedAtPeakLateral * 0.036f, RadiusAtPeakLateral * 0.01f));
	Lines.Add(FString::Printf(TEXT("  최소 선회 반경   %.1f m"), TightestRadius * 0.01f));

	Lines.Add(TEXT(""));
	Lines.Add(TEXT("=== 프로파일 권장값 ==="));

	// 이 비율은 템플릿 차량에서 난이도가 잘 갈렸던 지점입니다. 차가 달라져도 "한계의 몇 %"라는
	// 뜻은 그대로이므로 출발점으로 씁니다. 주행을 보고 조정하세요.
	const TCHAR* Names[] = { TEXT("Rookie  "), TEXT("Advanced"), TEXT("Pro     ") };
	const float Ratios[] = { 0.46f, 0.58f, 0.70f };

	for (int32 Index = 0; Index < 3; ++Index)
	{
		Lines.Add(FString::Printf(TEXT("  %s  LateralAccelBudget %.0f   BrakingDecel %.0f   MaxSpeed %.0f"),
			Names[Index],
			PeakLateralAccel * Ratios[Index],
			PeakBrakingDecel > 0.f ? PeakBrakingDecel * Ratios[Index] : 0.f,
			TopSpeed * FMath::Lerp(0.85f, 1.f, Ratios[Index])));
	}

	Lines.Add(FString::Printf(TEXT("  MinTurnRadius (세 프로파일 공통)  %.0f"), TightestRadius));
	Lines.Add(TEXT("  MinTurnRadius는 난이도가 아니라 차량 특성입니다. 저속 회전 반경이 아니라"));
	Lines.Add(TEXT("  주행 속도대에서 풀락으로 그린 원의 반지름을 넣어야 합니다."));

	return FString::Join(Lines, TEXT("\n"));
}

TStatId URacingCalibrationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URacingCalibrationSubsystem, STATGROUP_Tickables);
}

bool URacingCalibrationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

//------------------------------------------------------------------------------
// 콘솔 명령
//------------------------------------------------------------------------------

static FAutoConsoleCommandWithWorldAndArgs GRacingCalibrateCommand(
	TEXT("racing.Calibrate"),
	TEXT("차량 한계를 잽니다. 1로 시작하고 0으로 결과를 봅니다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			URacingCalibrationSubsystem* Calibration = World ? World->GetSubsystem<URacingCalibrationSubsystem>() : nullptr;

			if (!Calibration)
			{
				return;
			}

			const bool bStart = Args.Num() == 0 ? !Calibration->IsMeasuring() : (FCString::Atoi(*Args[0]) != 0);

			if (bStart)
			{
				Calibration->StartMeasuring();
			}
			else
			{
				Calibration->StopMeasuring();
			}
		}));

static FAutoConsoleCommandWithWorld GRacingCalibrateReportCommand(
	TEXT("racing.CalibrateReport"),
	TEXT("측정을 멈추지 않고 지금까지의 결과만 봅니다."),
	FConsoleCommandWithWorldDelegate::CreateStatic(
		[](UWorld* World)
		{
			if (URacingCalibrationSubsystem* Calibration = World ? World->GetSubsystem<URacingCalibrationSubsystem>() : nullptr)
			{
				ShowReport(Calibration->BuildReport());
			}
		}));
