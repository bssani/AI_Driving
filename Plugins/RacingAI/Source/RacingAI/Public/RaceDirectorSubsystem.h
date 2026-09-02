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
 * 그리드 한 자리의 배치 정보입니다.
 *
 * 스포너가 스폰 직후 Director에 넘겨 두고, 레이스를 다시 시작할 때 Director가
 * 이 정보로 차량을 제자리에 되돌립니다. 레벨을 다시 로드하지 않고 반복 세션을
 * 돌리려면 이 기록이 필요합니다.
 */
USTRUCT(BlueprintType)
struct RACINGAI_API FRaceGridPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	TWeakObjectPtr<URaceParticipantComponent> Participant;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	FTransform Transform = FTransform::Identity;

	/** 시작선 뒤에 정렬된 차량은 -1입니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	int32 InitialLap = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI")
	float LaneOffset = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRaceSimpleSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRaceCountdownSignature, int32, SecondsRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRaceLapSignature, URaceParticipantComponent*, Participant, int32, Lap);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRaceFinishSignature, URaceParticipantComponent*, Participant, int32, FinishPosition);

/**
 * 레이스 전체를 조율하는 월드 서브시스템입니다.
 *
 * 각 AI가 독립적으로 월드를 훑는 대신, 여기서 한 번에 판단합니다.
 * - 모든 참가자의 트랙 진행도와 순위 갱신
 * - 러버밴딩(따라잡기 보정) 계수 산출
 * - 추월 차선 중재. 두 AI가 같은 쪽으로 동시에 튀는 사고를 막습니다
 * - AI 주행 갱신을 프레임에 분산 실행
 * - 레이스 수명주기(카운트다운, 시작, 완주, 리셋)와 그에 대한 이벤트 방송
 *
 * 메인 프로젝트의 중앙 매니저는 이 서브시스템만 잡으면 됩니다.
 * 제어는 함수 호출로, 통지는 델리게이트 구독으로 이루어집니다.
 *
 * @code
 * URaceDirectorSubsystem* Director = GetWorld()->GetSubsystem<URaceDirectorSubsystem>();
 * Director->OnRacerFinished.AddDynamic(this, &AMyManager::HandleRacerFinished);
 * Director->SetTotalLaps(2);
 * Director->StartCountdown(3.f);
 * @endcode
 */
UCLASS()
class RACINGAI_API URaceDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 월드 컨텍스트에서 Director를 얻습니다. 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "Racing AI", meta = (WorldContext = "WorldContext"))
	static URaceDirectorSubsystem* Get(const UObject* WorldContext);

	//--------------------------------------------------------------------------
	// 이벤트 — 메인 프로젝트가 구독합니다
	//--------------------------------------------------------------------------

	/** 카운트다운이 시작될 때 */
	UPROPERTY(BlueprintAssignable, Category = "Racing AI|Events")
	FRaceSimpleSignature OnCountdownStarted;

	/** 카운트다운 남은 초가 바뀔 때마다. 마지막에 0으로 한 번 더 옵니다 */
	UPROPERTY(BlueprintAssignable, Category = "Racing AI|Events")
	FRaceCountdownSignature OnCountdownTick;

	/** 출발 신호 */
	UPROPERTY(BlueprintAssignable, Category = "Racing AI|Events")
	FRaceSimpleSignature OnRaceStarted;

	/** 참가자가 한 바퀴를 완료할 때마다 */
	UPROPERTY(BlueprintAssignable, Category = "Racing AI|Events")
	FRaceLapSignature OnLapCompleted;

	/** 참가자 한 명이 완주할 때마다 */
	UPROPERTY(BlueprintAssignable, Category = "Racing AI|Events")
	FRaceFinishSignature OnRacerFinished;

	/** 레이스가 끝났을 때. 최종 순위는 GetParticipantsByPosition으로 읽습니다 */
	UPROPERTY(BlueprintAssignable, Category = "Racing AI|Events")
	FRaceSimpleSignature OnRaceFinished;

	/** 그리드로 되돌려졌을 때 */
	UPROPERTY(BlueprintAssignable, Category = "Racing AI|Events")
	FRaceSimpleSignature OnRaceReset;

	//--------------------------------------------------------------------------
	// 레이스 제어 — 메인 프로젝트가 호출합니다
	//--------------------------------------------------------------------------

	/** 목표 랩 수를 정합니다. 0이면 완주 판정 없이 계속 달립니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Race")
	void SetTotalLaps(int32 InTotalLaps);

	UFUNCTION(BlueprintPure, Category = "Racing AI|Race")
	int32 GetTotalLaps() const { return TotalLaps; }

	/** 카운트다운을 시작하고, 끝나면 자동으로 출발시킵니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Race")
	void StartCountdown(float Seconds);

	/** 카운트다운 없이 즉시 출발시킵니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Race")
	void StartRace();

	/** 레이스를 중단합니다. 모든 AI가 정지합니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Race")
	void AbortRace();

	/**
	 * 모든 참가자를 그리드로 되돌리고 기록을 초기화합니다.
	 *
	 * 레벨을 다시 로드하지 않고 다음 세션을 시작할 수 있게 하는 함수입니다.
	 * 행사장처럼 짧은 레이스를 연속으로 돌릴 때 이것이 없으면 매 회차마다
	 * 레벨 로드를 기다려야 합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Race")
	void ResetRace();

	/**
	 * 그리드로 되돌리고 곧바로 카운트다운을 시작합니다.
	 *
	 * ResetRace와 StartCountdown을 한 번에 부르는 것과 같습니다. 다음 손님을 받을 때
	 * 이 함수 하나면 됩니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Race")
	void RestartRace(float CountdownSeconds = 3.f);

	/**
	 * 출발 전 플레이어의 조작을 잠급니다.
	 *
	 * 켜 두면 대기·카운트다운 동안 사람이 먼저 출발하지 못하고, 출발 신호와 동시에
	 * 풀립니다. AI는 이 설정과 무관하게 항상 대기합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Race")
	bool bLockPlayerUntilStart = true;

	/** 모든 AI를 그리드에서 정지 대기시킵니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Race")
	void HoldAtGrid();

	UFUNCTION(BlueprintPure, Category = "Racing AI|Race")
	ERaceState GetRaceState() const { return RaceState; }

	UFUNCTION(BlueprintPure, Category = "Racing AI|Race")
	bool IsRaceStarted() const { return RaceState == ERaceState::Racing; }

	/** 출발 이후 경과 시간 (초). 시작 전이면 0 */
	UFUNCTION(BlueprintPure, Category = "Racing AI|Race")
	float GetRaceElapsedSeconds() const;

	//--------------------------------------------------------------------------
	// 참가자
	//--------------------------------------------------------------------------

	void RegisterParticipant(URaceParticipantComponent* Participant);
	void UnregisterParticipant(URaceParticipantComponent* Participant);

	/**
	 * 참가자를 순위 순으로 반환합니다.
	 *
	 * 완주자가 먼저 완주 순서대로, 그 뒤에 미완주자가 누적 거리 순으로 옵니다.
	 * 완주해서 멈춰 선 차가 아직 달리는 차보다 뒤로 밀리지 않게 하기 위함입니다.
	 */
	UFUNCTION(BlueprintPure, Category = "Racing AI")
	TArray<URaceParticipantComponent*> GetParticipantsByPosition() const;

	UFUNCTION(BlueprintPure, Category = "Racing AI")
	URaceParticipantComponent* GetPlayerParticipant() const;

	//--------------------------------------------------------------------------
	// 그리드 — 스포너가 등록합니다
	//--------------------------------------------------------------------------

	/** 한 참가자의 그리드 자리를 기록합니다. ResetRace가 이 자리로 되돌립니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI|Grid")
	void RegisterGridPlacement(URaceParticipantComponent* Participant, const FTransform& Transform, int32 InitialLap, float LaneOffset);

	UFUNCTION(BlueprintCallable, Category = "Racing AI|Grid")
	void ClearGridPlacements();

	//--------------------------------------------------------------------------
	// 트랙
	//--------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void SetTrack(ARacingSpline* InTrack);

	/** 사용 중인 트랙을 반환합니다. 아직 정해지지 않았으면 이때 한 번 찾습니다 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	ARacingSpline* GetTrack();

	//--------------------------------------------------------------------------
	// AI가 사용하는 질의
	//--------------------------------------------------------------------------

	/**
	 * 지정 참가자의 앞에 있는 가장 가까운 차량을 찾습니다.
	 *
	 * MaxLateralSeparation이 0 이상이면 좌우로 그만큼 안쪽에 있는 차량만 봅니다.
	 * 즉 "같은 차선에서 내 진로를 막고 있는 차"를 찾을 때 씁니다. 이 필터가 없으면
	 * 옆 차선의 더 가까운 차 한 대가 그 뒤 같은 차선 차량을 가려 버립니다.
	 */
	FRacerAhead FindRacerAhead(const URaceParticipantComponent* Self, float ScanDistance, float MaxLateralSeparation = -1.f) const;

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

	/** 플레이어가 완주하면 레이스를 끝냅니다. 끄면 전원이 완주해야 끝납니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Race")
	bool bEndRaceWhenPlayerFinishes = true;

	/**
	 * 레이스 제한 시간 (초). 0이면 무제한입니다.
	 *
	 * 이 시간이 지나면 완주하지 못한 참가자를 진행도 순으로 등수만 매기고 종료합니다.
	 * 행사장에서는 반드시 넣으세요. 손님이 사고로 멈추면 완주 조건이 영영 충족되지
	 * 않아 세션이 끝나지 않습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Race", meta = (ClampMin = "0.0", Units = "s"))
	float RaceTimeLimitSeconds = 0.f;

	/** 완주한 AI를 서서히 멈춥니다. 끄면 계속 주행합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Race")
	bool bStopAIAfterFinish = true;

	/**
	 * 화면 상태 표시에 덧붙일 조작 안내입니다.
	 *
	 * 그리드 스포너가 자기 키 설정을 여기에 써 넣습니다. Director는 키를 모르지만
	 * 상태를 아는 쪽이라, 안내를 한 줄로 모아 보여 주기 위해 문자열로 받습니다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Racing AI|Debug")
	FString ControlHint;

	/** 디버그 표시를 켭니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Debug")
	bool bDrawDebug = false;

	//--------------------------------------------------------------------------
	// UTickableWorldSubsystem
	//--------------------------------------------------------------------------

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void UpdateProgressAndPositions(float DeltaTime);
	void UpdateRubberBanding();
	void ArbitrateOvertaking();
	void AdvanceAI(float DeltaTime);
	void DrawDebug() const;
	void ResolveTrackIfNeeded();

	/** 완주 판정을 하고 필요한 이벤트를 방송합니다 */
	void CheckForFinishers();

	/** 레이스를 종료 상태로 만듭니다 */
	void ConcludeRace();

	/** 카운트다운 타이머 콜백 */
	void TickCountdown();

	UPROPERTY(Transient)
	TObjectPtr<ARacingSpline> Track = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URaceParticipantComponent>> Participants;

	/** Participants 중 AI인 것만 추린 캐시입니다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URacingAIComponent>> AIComponents;

	/** 그리드 자리 기록. ResetRace가 사용합니다 */
	UPROPERTY(Transient)
	TArray<FRaceGridPlacement> GridPlacements;

	/** 분산 실행용 라운드로빈 커서 */
	int32 AdvanceCursor = 0;

	ERaceState RaceState = ERaceState::Idle;
	int32 TotalLaps = 0;
	int32 FinishedCount = 0;
	float RaceStartTimeSeconds = 0.f;
	int32 CountdownRemaining = 0;

	FTimerHandle CountdownTimer;

	bool bTrackResolved = false;
};
