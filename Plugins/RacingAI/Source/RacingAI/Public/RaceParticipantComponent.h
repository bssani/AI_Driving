#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RacingAITypes.h"
#include "RaceParticipantComponent.generated.h"

class ARacingSpline;

/**
 * 레이스 참가자 표식입니다. 플레이어 차량과 AI 차량 모두에 붙입니다.
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

	/** 현재 트랙 진행 상태입니다. Director가 갱신합니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	FRaceProgress Progress;

	/** 완주 여부입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	bool bFinished = false;

	/**
	 * 트랙 진행도를 갱신합니다. Director가 매 프레임 호출합니다.
	 *
	 * 랩 증가는 스플라인 거리가 한 바퀴 가까이 되감겼는지로 판정합니다.
	 * 정지선 트리거 액터가 따로 필요하지 않습니다.
	 */
	void RefreshProgress(const ARacingSpline& Track, float DeltaTime);

	/** 진행도를 현재 위치 기준으로 초기화합니다. 그리드 배치 후 호출하세요 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void ResetProgress();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 직전 프레임의 스플라인 거리. 랩 판정에 씁니다 */
	float PreviousDistance = 0.f;

	/** 최초 갱신에서는 랩 판정을 건너뜁니다 */
	bool bHasPreviousDistance = false;
};
