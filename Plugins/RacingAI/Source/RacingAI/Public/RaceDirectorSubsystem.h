#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RacingAITypes.h"
#include "RaceDirectorSubsystem.generated.h"

class ARacingSpline;
class URaceParticipantComponent;
class URacingAIComponent;

/**
 * 앞선 차량 한 대에 대한 정보입니다.
 *
 * 원본 에셋은 이 정보를 얻기 위해 갱신마다 라인트레이스 10회와 액터 이름 문자열 비교를
 * 수행했습니다. 참가자가 몇 대뿐이라면 스플라인 진행도를 직접 비교하는 편이 정확하고
 * 훨씬 쌉니다. 트레이스가 한 번도 필요하지 않습니다.
 */
USTRUCT(BlueprintType)
struct RACINGAI_API FRacerAhead
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	TWeakObjectPtr<URaceParticipantComponent> Participant;

	/** 트랙을 따라 잰 전방 간격 (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float Gap = 0.f;

	/** 상대의 좌우 오프셋 (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float LateralOffset = 0.f;

	/** 상대의 전방 속도 (cm/s) */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float ForwardSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	bool bIsPlayer = false;
};

/**
 * 레이스 전체를 조율하는 월드 서브시스템입니다.
 *
 * 각 AI가 독립적으로 월드를 훑는 대신, 여기서 한 번에 판단합니다.
 * - 모든 참가자의 트랙 진행도와 순위 갱신
 * - 러버밴딩(따라잡기 보정) 계수 산출
 * - 추월 차선 중재. 두 AI가 같은 쪽으로 동시에 튀는 사고를 막습니다
 * - AI 주행 갱신을 프레임에 분산 실행
 */
UCLASS()
class RACINGAI_API URaceDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 월드 컨텍스트에서 Director를 얻습니다. 없으면 nullptr */
	static URaceDirectorSubsystem* Get(const UObject* WorldContext);

	//--------------------------------------------------------------------------
	// UTickableWorldSubsystem
	//--------------------------------------------------------------------------

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	//--------------------------------------------------------------------------
	// 등록
	//--------------------------------------------------------------------------

	void RegisterParticipant(URaceParticipantComponent* Participant);
	void UnregisterParticipant(URaceParticipantComponent* Participant);

	/** 등록된 모든 참가자를 순위 순으로 반환합니다 */
	UFUNCTION(BlueprintPure, Category = "Racing AI")
	TArray<URaceParticipantComponent*> GetParticipantsByPosition() const;

	/** 사람이 조종하는 참가자를 반환합니다. 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "Racing AI")
	URaceParticipantComponent* GetPlayerParticipant() const;

	//--------------------------------------------------------------------------
	// 트랙
	//--------------------------------------------------------------------------

	/**
	 * 사용할 레이싱 라인입니다.
	 *
	 * 비어 있으면 첫 Tick에서 월드의 ARacingSpline을 한 번만 찾아 설정합니다.
	 * 원본 에셋처럼 매 갱신 GetAllActorsOfClass를 돌리지 않습니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void SetTrack(ARacingSpline* InTrack);

	/** 사용 중인 트랙을 반환합니다. 아직 정해지지 않았으면 이때 한 번 찾습니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	ARacingSpline* GetTrack();

	//--------------------------------------------------------------------------
	// 레이스 진행
	//--------------------------------------------------------------------------

	/** 모든 AI를 대기 상태로 둡니다. 그리드 정렬 후 카운트다운 동안 사용합니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void HoldAtGrid();

	/** 레이스를 시작합니다. 대기 중인 AI가 일제히 출발합니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void StartRace();

	UFUNCTION(BlueprintPure, Category = "Racing AI")
	bool IsRaceStarted() const { return bRaceStarted; }

	//--------------------------------------------------------------------------
	// AI가 사용하는 질의
	//--------------------------------------------------------------------------

	/** 지정 참가자의 앞에 있는 가장 가까운 차량을 찾습니다 */
	FRacerAhead FindRacerAhead(const URaceParticipantComponent* Self, float ScanDistance) const;

	//--------------------------------------------------------------------------
	// 설정
	//--------------------------------------------------------------------------

	/**
	 * 한 프레임에 주행 갱신할 AI의 최대 수입니다.
	 *
	 * 1이면 AI 3대가 90fps에서 각각 30Hz로 갱신되고, 프레임당 비용은 항상 한 대분으로
	 * 균일합니다. 원본처럼 각자 타이머를 돌리면 같은 프레임에 몰릴 수 있습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI", meta = (ClampMin = "1"))
	int32 MaxAIUpdatesPerFrame = 1;

	/** 분산 실행에도 불구하고 보장할 최소 갱신 주기 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI", meta = (ClampMin = "0.01", Units = "s"))
	float MaxUpdateInterval = 0.05f;

	/** 러버밴딩을 전체적으로 켜고 끕니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	bool bRubberBandingEnabled = true;

	/** 디버그 표시를 켭니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Debug")
	bool bDrawDebug = false;

private:
	/** 진행도를 갱신하고 순위를 매깁니다 */
	void UpdateProgressAndPositions(float DeltaTime);

	/** 각 AI의 러버밴딩 계수를 산출합니다 */
	void UpdateRubberBanding();

	/** 각 AI의 목표 차선 오프셋을 배정합니다 */
	void ArbitrateOvertaking();

	/** AI 주행 갱신을 분산 실행합니다 */
	void AdvanceAI(float DeltaTime);

	void DrawDebug() const;

	/** 월드에서 ARacingSpline을 한 번 찾습니다 */
	void ResolveTrackIfNeeded();

	UPROPERTY(Transient)
	TObjectPtr<ARacingSpline> Track = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URaceParticipantComponent>> Participants;

	/** Participants 중 AI인 것만 추린 캐시입니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URacingAIComponent>> AIComponents;

	/** 분산 실행용 라운드로빈 커서 */
	int32 AdvanceCursor = 0;

	bool bRaceStarted = false;
	bool bTrackResolved = false;
};
