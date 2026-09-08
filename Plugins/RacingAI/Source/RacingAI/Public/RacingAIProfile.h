#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RacingAIProfile.generated.h"

/**
 * 드라이버 난이도 프로필입니다.
 *
 * 원본 에셋은 난이도를 스로틀 배율 하나(0.75 / 0.85 / 0.95)로만 표현했습니다.
 * 그러면 "느린 AI"가 만들어질 뿐 "덜 능숙한 AI"는 만들어지지 않습니다.
 * 여기서는 전방 주시 시간, 횡가속 예산, 제동 여유, 추월 적극성까지 함께 묶어
 * 주행 스타일 자체가 달라지도록 했습니다.
 */
UCLASS(BlueprintType)
class RACINGAI_API URacingAIProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 에디터와 디버그 표시에 쓰는 이름입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	//--------------------------------------------------------------------------
	// 조향
	//--------------------------------------------------------------------------

	/**
	 * 전방 주시 시간 (초).
	 *
	 * 목표점까지의 거리를 현재 속도 x 이 값으로 잡습니다. 거리가 아니라 시간으로
	 * 두는 것이 핵심입니다. 원본 에셋은 거리를 cm/s 값에 잘못 사상해 사실상 5m로
	 * 고정돼 있었고, 그래서 고속에서 조향이 진동했습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.15", ClampMax = "3.0", Units = "s"))
	float LookaheadSeconds = 0.8f;

	/** 전방 주시 거리의 하한 (cm). 정지 상태에서도 목표점이 있어야 합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "100.0"))
	float MinLookahead = 400.f;

	/** 전방 주시 거리의 상한 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "500.0"))
	float MaxLookahead = 4000.f;

	/**
	 * 초당 최대 조향 변화량 (0~1 스케일).
	 *
	 * 2.5면 풀락까지 0.4초가 걸립니다. VR에서 옆 차가 한 프레임 만에 핸들을 꺾으면
	 * 바로 눈에 띄기 때문에 반드시 필요합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.5"))
	float MaxSteeringRate = 2.5f;

	/**
	 * 차량의 최소 회전 반경 (cm).
	 *
	 * 조향을 각도가 아니라 곡률로 계산하기 위해 필요합니다. pure pursuit이 요구하는
	 * 원호 곡률을 이 차량이 낼 수 있는 최대 곡률(1 / 이 값)로 나누면 곧바로 -1~1
	 * 조향 입력이 나옵니다. 각도를 임의 구간에 사상하던 원본보다 물리적으로 맞습니다.
	 *
	 * 재는 법: 조향을 끝까지 꺾고 저속으로 한 바퀴 돌린 뒤 그 원의 반지름을 봅니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "100.0"))
	float MinTurnRadius = 600.f;

	/** 조향 출력 최종 배율. 차량이 과하게 반응하면 낮춥니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float SteeringGain = 1.f;

	/** 차선 변경 속도 (cm/s). 급격한 차선 이동을 막습니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "10.0"))
	float LateralOffsetRate = 300.f;

	/**
	 * 방해가 없을 때 돌아갈 자리. 스플라인 중심 기준이며 오른쪽이 양수입니다.
	 *
	 * 그리드 차선은 출발할 때 서 있던 자리일 뿐 달리고 싶은 자리가 아닙니다. 그것을 끝까지
	 * 붙들면 왼쪽에서 출발한 차는 가운데가 비어 있어도 경기 내내 왼쪽 연석만 따라갑니다.
	 * 코스에 빠른 라인이 따로 있으면 그쪽으로 옮기세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering")
	float RacingLineOffset = 0.f;

	/**
	 * 출발 후 그리드 차선에서 라인으로 옮겨 가는 데 걸리는 시간 (초).
	 *
	 * 0이면 출발 신호와 동시에 전부 한 줄로 몰려 서로를 받습니다. 세로로 벌어질 틈을 준 뒤
	 * 모이도록 몇 초를 둡니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steering", meta = (ClampMin = "0.0"))
	float GridLaneHoldSeconds = 5.f;

	//--------------------------------------------------------------------------
	// 속도
	//--------------------------------------------------------------------------

	/**
	 * 코너에서 허용할 횡가속 (cm/s^2).
	 *
	 * 목표 속도를 sqrt(횡가속 x 곡률반경)으로 구합니다. 이 값 하나가
	 * "코너를 얼마나 과감하게 도는가"를 결정하며, 난이도 차이가 가장 크게 드러납니다.
	 * 1200 cm/s^2 은 약 1.2G 입니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "200.0"))
	float LateralAccelBudget = 1100.f;

	/** 제동 감속도 (cm/s^2). 코너 진입 제동 시점을 역산하는 데 씁니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "200.0"))
	float BrakingDecel = 900.f;

	/** 목표 속도에 곱하는 여유 계수. 1보다 작으면 보수적으로 탑니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.5", ClampMax = "1.2"))
	float SpeedMargin = 0.95f;

	/** 직선에서의 최고 속도 (cm/s). 0이면 제한 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0"))
	float MaxSpeed = 0.f;

	/** 속도 오차를 스로틀/브레이크로 바꾸는 비례 이득 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0001"))
	float SpeedControlGain = 0.004f;

	//--------------------------------------------------------------------------
	// 추월
	//--------------------------------------------------------------------------

	/** 앞차를 인지하기 시작하는 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtaking", meta = (ClampMin = "0.0"))
	float OvertakeScanDistance = 3000.f;

	/** 추월 시 중심선에서 비켜설 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtaking", meta = (ClampMin = "0.0"))
	float OvertakeLateralOffset = 250.f;

	/** 앞차가 이 비율보다 느릴 때만 추월을 시도합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtaking", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float OvertakeSpeedRatio = 0.95f;

	/** 같은 차선의 앞차와 유지할 최소 간격 (cm). 이보다 가까우면 감속합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtaking", meta = (ClampMin = "0.0"))
	float FollowGap = 700.f;

	/**
	 * 다른 차선으로 인정하는 좌우 간격 (cm).
	 *
	 * 앞차와의 좌우 거리가 이 값을 넘으면 같은 차선이 아니라고 보고 간격 유지를 풀어
	 * 그대로 지나갑니다. 이 판정이 없으면 앞차가 정지해 있을 때 목표 속도가 0으로
	 * 묶여, 옆으로 비켜서도 영원히 추월을 끝내지 못합니다.
	 *
	 * 차 폭보다 조금 넉넉하게 잡으세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtaking", meta = (ClampMin = "50.0"))
	float PassingLateralClearance = 240.f;

	/**
	 * 추월을 시도할 때 유지할 최소 전진 속도 (cm/s).
	 *
	 * 멈춘 차는 조향을 해도 옆으로 가지 못합니다. 비켜설 여지를 만들기 위해
	 * 아주 낮은 속도로라도 계속 굴러가게 합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtaking", meta = (ClampMin = "0.0"))
	float MinOvertakeSpeed = 650.f;

	/**
	 * 같은 차선에서 절대 더 좁히지 않는 간격 (cm).
	 *
	 * 이보다 가까우면 추월 의사와 무관하게 정지합니다. 추돌 방지용 하한입니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overtaking", meta = (ClampMin = "50.0"))
	float HardStopGap = 420.f;

	//--------------------------------------------------------------------------
	// 러버밴딩
	//--------------------------------------------------------------------------

	/**
	 * 러버밴딩 강도. 0이면 끔.
	 *
	 * 행사장에서는 이 값이 사실상 체감 난이도를 결정합니다. 일반 관람객이
	 * 전력으로 달리는 AI를 따라잡을 수는 없기 때문에, 보정이 없으면 30초 만에
	 * 혼자 빈 트랙을 달리게 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber Banding", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CatchUpStrength = 0.6f;

	/** 뒤처졌을 때 허용할 최대 속도 상향 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber Banding", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MaxSpeedUp = 0.15f;

	/** 앞섰을 때 허용할 최대 속도 하향 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber Banding", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MaxSlowDown = 0.20f;

	/** 이 거리 안에서는 보정하지 않습니다 (cm). 접전 구간을 흔들지 않기 위한 데드밴드 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber Banding", meta = (ClampMin = "0.0"))
	float NeutralGap = 1500.f;

	/** 보정이 최대치에 도달하는 격차 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rubber Banding", meta = (ClampMin = "100.0"))
	float FullEffectGap = 12000.f;

	//--------------------------------------------------------------------------
	// 플레이어 배려
	//--------------------------------------------------------------------------

	/**
	 * 플레이어에게 추가로 벌리는 여유 거리 (cm).
	 *
	 * AI끼리는 몸싸움을 해도 되지만 플레이어는 다릅니다. VR에서 옆에서 들이받히면
	 * 경쟁이 아니라 멀미 요인이 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Courtesy", meta = (ClampMin = "0.0"))
	float PlayerExtraClearance = 150.f;

	/** 플레이어를 배려하기 시작하는 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Courtesy", meta = (ClampMin = "0.0"))
	float PlayerYieldDistance = 1500.f;

	//--------------------------------------------------------------------------
	// 복구
	//--------------------------------------------------------------------------

	/** 이 속도 미만이면 (cm/s) 스턱 판정 타이머가 돕니다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0"))
	float StuckSpeedThreshold = 60.f;

	/** 스턱으로 판정하기까지의 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.2", Units = "s"))
	float StuckTimeToReverse = 1.5f;

	/**
	 * 출발 직후 스턱 판정을 유예하는 시간 (초).
	 *
	 * 정지 상태에서 가속하는 동안에는 속도가 StuckSpeedThreshold를 넘기까지
	 * 시간이 걸립니다. 이 유예가 없으면 출발 신호마다 모든 AI가 갇힌 것으로
	 * 오인되어 잠깐 후진합니다. 무거운 차일수록 넉넉히 주세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0", Units = "s"))
	float LaunchGraceSeconds = 3.f;

	/** 후진을 유지하는 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.2", Units = "s"))
	float ReverseDuration = 1.2f;

	/**
	 * 후진 탈출을 몇 번까지 시도할지입니다.
	 *
	 * 벽이나 다른 차에 정면으로 막히면 후진했다가 다시 같은 곳으로 돌진하기를
	 * 반복하며 영영 못 빠져나옵니다. 이 횟수를 넘기면 트랙 위로 재배치해
	 * 무한 반복을 끊습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "1"))
	int32 MaxStuckAttempts = 2;

	/** 전복으로 판정할 롤/피치 각도 (도) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "20.0", ClampMax = "180.0"))
	float FlippedAngleDegrees = 70.f;

	/** 전복 상태를 이 시간 이상 유지하면 재배치합니다 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.5", Units = "s"))
	float FlippedTimeToRespawn = 2.5f;

	/** 트랙에서 이 거리 이상 벗어나면 재배치합니다 (cm). 0이면 끔 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0"))
	float OffTrackRespawnDistance = 4000.f;
};
