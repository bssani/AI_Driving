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
	 * 잠긴 차가 이만큼 (cm) 밀리면 제자리로 되돌립니다.
	 *
	 * 속도를 0으로 눌러도 물리는 한 스텝 안에서 조금씩 밀어냅니다. 카운트다운이 길면
	 * 그 조금이 쌓여 출발선을 넘습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI", meta = (ClampMin = "1.0", Units = "cm"))
	float LockedDriftTolerance = 30.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 잠긴 동안 차량을 붙잡아 둡니다 */
	void HoldVehicleStill();

	/** 소유 액터나 그 컴포넌트에서 IRacingVehicleInput 구현체를 찾습니다 */
	UObject* ResolveVehicleInputTarget();

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
};
