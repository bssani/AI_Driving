#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RacingCalibrationSubsystem.generated.h"

/**
 * 차량의 실제 한계를 재서 프로파일 값으로 바꿔 주는 계측기입니다.
 *
 * 프로파일의 숫자는 전부 특정 차량에서 잰 값입니다. `LateralAccelBudget`은 그 차가 낼 수
 * 있었던 횡가속의 몇 %이고, `MinTurnRadius`는 주행 속도대에서 풀락으로 그린 원의 반지름이며,
 * `BrakingDecel`은 실측 제동 감속입니다. 차를 바꾸면 이 셋이 전부 다른 차의 값이 됩니다.
 *
 * 그때 나타나는 증상은 두 가지고, 둘 다 "AI가 이상하다"로 보입니다.
 * - 실제보다 낮게 잡혀 있으면 AI가 코너에서 기어갑니다.
 * - `MinTurnRadius`가 실제보다 크면 조향 게인이 모자라 코너 바깥으로 밀려 벽을 긁습니다.
 *
 * 그래서 추측하지 말고 잽니다. 차를 직접 몰기만 하면 됩니다. 조작 방식과 무관하므로
 * 스티어링 휠로 재도 되고, 차량 구현이 무엇이든 상관없습니다.
 *
 * @code
 * racing.Calibrate 1     // 재기 시작
 * // 직선에서 풀 스로틀 -> 최고 속도와 0-100
 * // 풀 브레이크        -> 제동 감속
 * // 넓은 곳에서 풀락으로 몇 바퀴 -> 횡가속 한계와 유효 반경
 * racing.Calibrate 0     // 결과와 프로파일 권장값 출력
 * @endcode
 */
UCLASS()
class RACINGAI_API URacingCalibrationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 측정을 시작합니다. 이전 기록은 지워집니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Calibration")
	void StartMeasuring();

	/** 측정을 끝내고 결과를 로그와 화면에 출력합니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Calibration")
	void StopMeasuring();

	UFUNCTION(BlueprintPure, Category = "Racing AI|Calibration")
	bool IsMeasuring() const { return bMeasuring; }

	/** 지금까지의 결과를 문자열로 만듭니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Calibration")
	FString BuildReport() const;

	//--------------------------------------------------------------------------
	// UTickableWorldSubsystem
	//--------------------------------------------------------------------------

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickable() const override { return bMeasuring; }

private:
	/** 재는 대상. 지정하지 않으면 로컬 플레이어의 폰입니다 */
	TWeakObjectPtr<AActor> Subject;

	bool bMeasuring = false;

	/** 이 속도 아래에서는 선회 반경이 의미 없이 작게 나옵니다 (cm/s) */
	static constexpr float MinCorneringSpeed = 1000.f;

	float TopSpeed = 0.f;
	float PeakLateralAccel = 0.f;
	float SpeedAtPeakLateral = 0.f;
	float RadiusAtPeakLateral = 0.f;
	float TightestRadius = 0.f;
	float PeakBrakingDecel = 0.f;
	float PeakForwardAccel = 0.f;

	/** 0-100 km/h 계측. 정지에서 출발할 때만 셉니다 */
	float LaunchElapsed = 0.f;
	float LaunchTime = 0.f;
	bool bLaunchTiming = false;

	float PreviousForwardSpeed = 0.f;
	bool bHasPreviousSpeed = false;

	AActor* ResolveSubject();
};
