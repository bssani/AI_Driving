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
	Respawning,

	/** 완주 후 감속 중. 레이싱 라인은 계속 따라갑니다 */
	Finished
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
 * 레이스 전체의 진행 상태입니다.
 *
 * 메인 프로젝트의 중앙 매니저가 이 값과 델리게이트만 보고 결과 화면, 세션 전환,
 * UI를 처리할 수 있도록 플러그인 바깥으로 노출합니다.
 */
UENUM(BlueprintType)
enum class ERaceState : uint8
{
	/** 아직 시작 전. 그리드에 정렬만 되어 있습니다 */
	Idle,

	/** 카운트다운 진행 중 */
	Countdown,

	/** 주행 중 */
	Racing,

	/** 모든 참가자가 완주했거나 레이스가 종료되었습니다 */
	Finished,

	/** 외부에서 중단되었습니다 */
	Aborted
};

/** 그리드 한 자리에 누가 서는지입니다. */
UENUM(BlueprintType)
enum class ERaceGridOccupant : uint8
{
	/** AI 차량을 스폰합니다 */
	AI,

	/** 사람이 타는 차량을 이 자리로 옮깁니다 */
	Player
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

	/**
	 * 스플라인 시작점 기준 절대 거리 (cm)입니다.
	 *
	 * 목표점 계산, 곡률 조회처럼 트랙 위 위치가 필요한 곳에서 씁니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float DistanceAlongSpline = 0.f;

	/**
	 * 결승선 기준 이번 랩의 진행 거리 (cm)입니다.
	 *
	 * 랩 판정과 순위는 이 값으로 합니다. 결승선을 스플라인 원점이 아닌 곳에 두면
	 * DistanceAlongSpline과 달라집니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float LapDistance = 0.f;

	/**
	 * 완주한 랩 수입니다.
	 *
	 * 시작선 뒤에 정렬된 차량은 -1로 시작합니다. 그래야 시작선을 넘는 순간 0랩이 되어
	 * 그리드 순서와 순위가 일치합니다.
	 */
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
