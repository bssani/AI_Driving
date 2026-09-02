#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacingSpline.generated.h"

class USplineComponent;

/**
 * 레이싱 라인이자 트랙의 기준선입니다.
 *
 * 스플라인 하나가 코스 중심선을 나타내고, 좌우 오프셋은 이 선을 기준으로 표현합니다.
 * 원본 에셋은 차선 오프셋을 월드 Y축에 더했기 때문에 트랙이 월드 X와 나란한 구간에서만
 * 제대로 동작했습니다. 여기서는 항상 스플라인의 우측 벡터를 기준으로 삼습니다.
 *
 * 곡률은 BeginPlay에 한 번 계산해 LUT로 들고 있습니다. 그 덕에 코너 진입 제동 지점을
 * 자동으로 계산할 수 있고, 원본처럼 BP_HighSpeedBrakePoint를 손으로 찍을 필요가 없습니다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Racing Spline"))
class RACINGAI_API ARacingSpline : public AActor
{
	GENERATED_BODY()

public:
	ARacingSpline();

	//--------------------------------------------------------------------------
	// 구성
	//--------------------------------------------------------------------------

	/** 코스 중심선입니다. 서킷이면 Closed Loop을 켜 주세요 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Racing Spline")
	TObjectPtr<USplineComponent> Spline;

	/** 중심선에서 좌우로 허용되는 폭 (cm). 추월 오프셋이 이 값 안으로 제한됩니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing Spline", meta = (ClampMin = "50.0"))
	float TrackHalfWidth = 500.f;

	/**
	 * 결승선의 스플라인 거리 (cm)입니다.
	 *
	 * 랩은 이 지점을 통과할 때 올라가고, 순위도 이 지점을 기준으로 잽니다.
	 * 0이면 스플라인이 시작하는 자리가 곧 결승선입니다.
	 *
	 * 출발 그리드는 스포너의 Pole Distance로 따로 잡습니다. 그 값이 이 결승선을
	 * 기준으로 한 상대 거리이므로, 둘을 다르게 두면 출발 지점과 결승 지점이 갈라집니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing Spline", meta = (ClampMin = "0.0"))
	float FinishLineDistance = 0.f;

	/** 곡률 LUT의 샘플 간격 (cm). 촘촘할수록 정확하지만 메모리를 더 씁니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing Spline", meta = (ClampMin = "50.0", ClampMax = "1000.0"))
	float CurvatureSampleStep = 200.f;

	//--------------------------------------------------------------------------
	// 기본 질의
	//--------------------------------------------------------------------------

	/** 스플라인 전체 길이 (cm) */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetLength() const;

	/** 닫힌 서킷인지 여부 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	bool IsClosed() const;

	/** 거리를 [0, Length) 범위로 감쌉니다. 열린 코스면 클램프합니다 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float WrapDistance(float Distance) const;

	/**
	 * 두 지점 사이의 전방 거리를 구합니다.
	 *
	 * 닫힌 서킷에서 시작선을 넘나드는 경우를 처리하기 위해 필요합니다.
	 * 결과는 항상 [0, Length) 이며 From에서 To까지 진행 방향으로 잰 거리입니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetForwardDelta(float From, float To) const;

	/** 월드 위치에 가장 가까운 스플라인 거리를 찾습니다 (cm) */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float FindDistanceAtLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	FVector GetLocationAtDistance(float Distance) const;

	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	FVector GetDirectionAtDistance(float Distance) const;

	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	FVector GetRightAtDistance(float Distance) const;

	/**
	 * 중심선에서 좌우로 비켜난 목표 위치를 구합니다.
	 *
	 * Offset은 오른쪽이 양수이며 TrackHalfWidth 안으로 클램프됩니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	FVector GetOffsetLocationAtDistance(float Distance, float LateralOffset) const;

	/** 월드 위치의 좌우 오프셋을 구합니다. 오른쪽이 양수 (cm) */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetLateralOffsetAtLocation(const FVector& WorldLocation) const;

	//--------------------------------------------------------------------------
	// 결승선
	//--------------------------------------------------------------------------

	/**
	 * 결승선을 기준으로 다시 잰 거리입니다. 닫힌 서킷에서는 [0, 길이) 범위입니다.
	 *
	 * 랩 판정과 순위는 이 값으로 합니다. 스플라인 원점이 아니라 결승선이 기준이어야
	 * 출발 지점과 결승 지점을 따로 둘 수 있습니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetDistanceFromFinishLine(float SplineDistance) const;

	/** 결승선 기준 상대 거리를 절대 스플라인 거리로 되돌립니다 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetSplineDistanceFromFinishOffset(float OffsetFromFinish) const;

	/** 결승선의 월드 위치입니다 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	FVector GetFinishLineLocation() const;

	//--------------------------------------------------------------------------
	// 곡률과 속도
	//--------------------------------------------------------------------------

	/** 해당 지점의 곡률 (1/cm). 직선이면 0에 가깝습니다 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetCurvatureAtDistance(float Distance) const;

	/**
	 * 해당 지점을 통과할 수 있는 최대 속도입니다 (cm/s).
	 *
	 * v = sqrt(LateralAccel / curvature) 입니다. 곡률이 0이면 무한대가 되므로
	 * 호출하는 쪽에서 직선 최고속도로 클램프해야 합니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetCornerSpeedLimit(float Distance, float LateralAccel) const;

	/**
	 * 지금 내야 할 목표 속도를 구합니다 (cm/s).
	 *
	 * 전방 Horizon 구간을 훑으면서, 각 지점의 코너 속도 한계에 도달하려면 지금 얼마여야
	 * 하는지를 v = sqrt(v_limit^2 + 2 * decel * d) 로 역산해 그중 최솟값을 취합니다.
	 * 이것이 코너 진입 제동 지점을 자동으로 만들어 냅니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Racing Spline")
	float GetTargetSpeedAtDistance(float Distance, float LateralAccel, float BrakingDecel, float Horizon, float StraightMaxSpeed) const;

	//--------------------------------------------------------------------------
	// LUT
	//--------------------------------------------------------------------------

	/** 곡률 LUT를 다시 계산합니다. 스플라인을 런타임에 바꿨다면 호출하세요 */
	UFUNCTION(BlueprintCallable, Category = "Racing Spline")
	void RebuildCurvature();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	/** CurvatureSampleStep 간격으로 샘플링한 곡률 (1/cm) */
	TArray<float> CurvatureLUT;

	/** LUT를 만들 때 사용한 실제 간격. 런타임에 CurvatureSampleStep이 바뀌어도 안전하게 읽기 위함 */
	float LUTStep = 200.f;

	/** LUT를 만들 때의 스플라인 길이 */
	float LUTLength = 0.f;
};
