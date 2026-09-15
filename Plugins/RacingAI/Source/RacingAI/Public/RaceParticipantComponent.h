#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RacingAITypes.h"
#include "RaceParticipantComponent.generated.h"

class ARacingSpline;

/**
 * 레이스 참가자 표식입니다. 플레이어 차량과 AI 차량 모두에 붙습니다.
 *
 * 이 컴포넌트가 있는 액터는 Director에 자동 등록되고, Director가 매 프레임 트랙
 * 진행도를 갱신합니다. 그래서 랩 수와 순위는 별도 시스템 없이 여기서 바로 얻어집니다.
 *
 * AI에게는 URacingAIComponent가 이 클래스를 상속해 주행 두뇌를 얹습니다.
 * 플레이어 차량에는 이 컴포넌트를 그대로 붙이면 됩니다.
 */
UCLASS(ClassGroup = "Racing AI", meta = (BlueprintSpawnableComponent, DisplayName = "Race Participant"))
class RACINGAI_API URaceParticipantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URaceParticipantComponent();

	/**
	 * 사람이 조종하는 차량인지 여부입니다.
	 *
	 * 러버밴딩의 기준점이 되고, AI가 더 넓게 비켜 주는 대상이 됩니다.
	 * 플레이어 폰에 붙일 때 반드시 켜 주세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	bool bIsPlayer = false;

	/** 리더보드에 표시할 이름입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	FText DisplayName;

	/** 이 차량에 적용된 도색 이름입니다. UI에서 같은 색으로 표시할 때 씁니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	FName LiveryName;

	/** 이 차량을 대표하는 색입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	FLinearColor LiveryColor = FLinearColor::White;

	/** 현재 트랙 진행 상태입니다. Director가 갱신합니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	FRaceProgress Progress;

	//--------------------------------------------------------------------------
	// 완주
	//--------------------------------------------------------------------------

	/** 완주 여부입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Finish")
	bool bFinished = false;

	/** 완주 순서입니다. 1부터 시작하며 미완주는 0 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Finish")
	int32 FinishPosition = 0;

	/** 출발부터 완주까지 걸린 시간 (초). 미완주는 0 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Finish")
	float FinishTimeSeconds = 0.f;

	/**
	 * 완주하지 못한 채 레이스가 끝났는지 여부입니다.
	 *
	 * 제한 시간이 지나 종료되면 남은 참가자는 진행도 순으로 등수만 받고
	 * 이 값이 켜집니다. 결과 화면에서 완주자와 구분해 표시하세요.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Finish")
	bool bDidNotFinish = false;

	/** 미완주로 표시합니다. Director가 호출합니다 */
	void MarkDidNotFinish(int32 InFinishPosition);

	//--------------------------------------------------------------------------
	// Director가 호출합니다
	//--------------------------------------------------------------------------

	/**
	 * 트랙 진행도를 갱신하고 이번 프레임의 랩 변화량을 반환합니다.
	 *
	 * 랩 증가는 스플라인 거리가 한 바퀴 가까이 되감겼는지로 판정합니다.
	 * 정지선 트리거 액터가 따로 필요하지 않습니다.
	 */
	int32 RefreshProgress(const ARacingSpline& Track, float DeltaTime);

	/**
	 * 진행도를 초기화합니다. 그리드 배치 직후에 호출합니다.
	 *
	 * InitialLap은 보통 0이지만, 시작선 뒤쪽에 정렬된 차량은 -1을 받습니다.
	 * 그래야 시작선을 넘는 순간 0랩이 되어 그리드 순서와 순위가 일치합니다.
	 * 이 값을 넣지 않으면 뒷줄 차량의 누적 거리가 한 바퀴만큼 부풀어
	 * 출발하자마자 1위로 표시되고 러버밴딩도 반대로 걸립니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void ResetProgress(int32 InitialLap = 0);

	/** 완주로 표시합니다. Director가 호출합니다 */
	void MarkFinished(int32 InFinishPosition, float InFinishTimeSeconds);

	/**
	 * 사람의 조작을 잠그거나 풉니다.
	 *
	 * 출발 신호 전에 플레이어가 먼저 튀어나가지 못하게 합니다.
	 *
	 * 폰의 입력을 끄는 것만으로는 부족합니다. APawn::DisableInput은 bInputEnabled를 내릴 뿐이고,
	 * 엔진이 입력 스택을 세울 때 빠지는 것은 그 폰의 입력 컴포넌트 하나입니다
	 * (APlayerController::BuildInputStack). 플레이어 컨트롤러 자신의 입력 컴포넌트와
	 * EnableInput으로 밀어 넣은 컴포넌트는 그대로 남습니다. 그래서 스티어링 휠처럼
	 * 컨트롤러 쪽에 바인딩된 조작은 잠금을 그냥 통과합니다.
	 *
	 * 그래서 잠긴 동안에는 매 틱 차량을 직접 붙잡습니다. 누가 어느 경로로 스로틀을 넣든
	 * 결과가 같아집니다.
	 *
	 * AI에는 호출하지 않습니다. AI는 대기 상태가 따로 있습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void SetInputLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Racing AI")
	bool IsInputLocked() const { return bInputLocked; }

	/**
	 * 잠긴 차가 이만큼 (cm) 밀리면 제자리로 되돌립니다. 좌우 앞뒤만 재며 높이는 보지 않습니다.
	 *
	 * 속도를 0으로 눌러도 물리는 한 스텝 안에서 조금씩 밀어냅니다. 실측 2.4cm/s이므로
	 * 카운트다운이 길면 쌓여서 출발선을 넘습니다. 작게 둘수록 자주, 대신 눈에 띄지 않게
	 * 되돌립니다. 크게 두면 한 번에 크게 튀어 그 순간이 보입니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI", meta = (ClampMin = "0.5", Units = "cm"))
	float LockedDriftTolerance = 5.f;

	//--------------------------------------------------------------------------
	// 레이스 종료 정지
	//--------------------------------------------------------------------------

	/**
	 * 차를 일정한 감속으로 세운 다음 잠급니다. 레이스가 끝났을 때 플레이어에게 씁니다.
	 *
	 * 곧바로 SetInputLocked(true)를 부르면 달리던 차가 그 자리에서 멈춥니다. VR에서 시속
	 * 150 km로 달리다 한 프레임 만에 서면 멀미를 넘어 불쾌감이 됩니다. 그래서 속도의 상한을
	 * Deceleration만큼씩 낮춰 가며 누르고, 거의 섰을 때 잠급니다.
	 *
	 * 세우는 동안 핸들은 살려 둡니다. 결승 지점이 커브일 수 있고, 핸들까지 막으면 곧게
	 * 미끄러져 벽에 박습니다. 스로틀을 밟아도 상한을 넘지 못하므로 조작 경로와 무관하게 섭니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void StopAndHold(float Deceleration = 600.f);

	/** StopAndHold로 세우는 중인지 */
	UFUNCTION(BlueprintPure, Category = "Racing AI")
	bool IsStopping() const { return bStopping; }

	//--------------------------------------------------------------------------
	// 트랙 위로 되돌리기
	//--------------------------------------------------------------------------

	/**
	 * 가장 가까운 트랙 위로 차를 옮겨 세웁니다. 벽에 박혀 못 나올 때 키 하나로 부르는 용도입니다.
	 *
	 * 지금 자리에서 줄을 따라 가장 가까운 지점을 찾고, 좌우로는 원래 있던 쪽을 유지하되
	 * 벽에서 떨어지도록 도로 안쪽으로 당깁니다. 방향은 트랙 진행 방향으로 맞추고 속도는 지웁니다.
	 * 높이는 그 자리 노면을 찾아 맞춥니다.
	 *
	 * 출발 전처럼 조작이 잠겨 있으면 옮기지 않고 false를 돌려줍니다. 그리드를 흐트러뜨리지 않기 위해서입니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	bool RespawnOnTrack();

	/**
	 * RespawnOnTrack이 옮길 때 좌우 위치를 도로 반폭의 몇 배 안으로 당길지입니다.
	 *
	 * 0이면 항상 가운데, 1이면 원래 좌우 위치 그대로입니다. 벽에 붙은 차를 벽 옆에 다시 놓으면
	 * 곧바로 또 박으므로 반쯤 안으로 당깁니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Respawn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RespawnLaneFraction = 0.5f;

	/** 옮긴 차의 바퀴 바닥을 노면에서 얼마나 띄워 놓을지입니다 (cm). 0이면 바퀴가 노면에 박힐 수 있습니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Respawn", meta = (ClampMin = "0.0", Units = "cm"))
	float RespawnClearance = 30.f;

	/**
	 * 트랙의 지정한 자리에 차를 놓습니다. RespawnOnTrack과 AI 재배치가 함께 씁니다.
	 *
	 * 스플라인 높이가 노면과 같다는 보장이 없어서, 그 자리 위에서 아래로 노면을 찾아 높이를
	 * 정합니다. 노면을 못 찾으면 스플라인보다 150cm 위에 놓습니다.
	 */
	bool PlaceOnTrack(const ARacingSpline& Track, float SplineDistance, float LateralOffset);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 잠긴 동안 차량을 붙잡아 둡니다 */
	void HoldVehicleStill();

	/** 소유 액터나 그 컴포넌트에서 IRacingVehicleInput 구현체를 찾습니다 */
	UObject* ResolveVehicleInputTarget();

	/** StopAndHold 중에 속도 상한을 낮추고 누릅니다 */
	void UpdateStopping(float DeltaTime);

	/** 플레이어 컨트롤러가 먼저 틱하도록 걸거나 풉니다. 그 프레임의 입력이 처리된 뒤에 눌러야 합니다 */
	void SetControllerTickPrerequisite(bool bEnable);

	/** 직전 프레임의 스플라인 거리. 랩 판정에 씁니다 */
	float PreviousDistance = 0.f;

	/** 최초 갱신에서는 랩 판정을 건너뜁니다 */
	bool bHasPreviousDistance = false;

	/** 사람 조작이 잠겨 있는지 */
	bool bInputLocked = false;

	/** 잠근 순간의 위치. 밀려나면 여기로 되돌립니다 */
	FTransform LockedTransform = FTransform::Identity;

	/** 매 틱 다시 찾지 않기 위해 캐시합니다 */
	TWeakObjectPtr<UObject> VehicleInputTarget;

	/** StopAndHold로 세우는 중인지, 지금 허용하는 속도(cm/s)와 그것을 낮추는 감속도 */
	bool bStopping = false;
	float StoppingAllowedSpeed = 0.f;
	float StoppingDeceleration = 0.f;
};
