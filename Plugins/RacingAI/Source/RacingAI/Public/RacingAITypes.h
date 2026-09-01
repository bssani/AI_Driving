#pragma once

#include "CoreMinimal.h"
#include "RacingAITypes.generated.h"

/**
 * AI의 주행 상태입니다.
 *
 * 상태를 명시적으로 들고 있는 이유는, 복구 동작이 일반 주행 로직을 덮어쓰는 형태로
 * 섞여 들어가면 어느 쪽이 조향을 쥐고 있는지 알 수 없게 되기 때문입니다.
 */
UENUM(BlueprintType)
enum class ERacingAIState : uint8
{
	/** 그리드 대기. 입력을 넣지 않고 정지합니다 */
	Waiting,

	/** 정상 주행 */
	Racing,

	/** 스턱 탈출을 위한 후진 */
	Reversing,

	/** 전복 등으로 스플라인 위에 재배치되는 중 */
	Respawning
};

/** 추월 시 어느 쪽으로 비킬지입니다. */
UENUM(BlueprintType)
enum class ERacingLaneSide : uint8
{
	Center,
	Left,
	Right
};

/**
 * 한 참가자의 트랙 진행 상태입니다.
 *
 * Director가 매 프레임 갱신하며, 순위 · 러버밴딩 · 추월 판단이 모두 이 값만 봅니다.
 * 원본 에셋이 라인트레이스와 액터 이름 문자열로 알아내던 정보를 여기서 한 번에 들고 있습니다.
 */
USTRUCT(BlueprintType)
struct RACINGAI_API FRaceProgress
{
	GENERATED_BODY()

	/** 스플라인 시작점 기준 이번 랩의 진행 거리 (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float DistanceAlongSpline = 0.f;

	/** 완주한 랩 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	int32 Lap = 0;

	/** 랩을 포함한 누적 진행 거리 (cm). 순위 비교는 이 값으로 합니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float TotalDistance = 0.f;

	/** 레이싱 라인 중심선으로부터의 좌우 오프셋 (cm). 오른쪽이 양수 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float LateralOffset = 0.f;

	/** 전방 속도 (cm/s). 후진 중이면 음수 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float ForwardSpeed = 0.f;

	/** 1위부터 시작하는 현재 순위 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	int32 Position = 0;

	/** 트랙 진행 방향과 차량 전방의 각도차 (도). 역주행 판정에 씁니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float HeadingErrorDegrees = 0.f;
};
