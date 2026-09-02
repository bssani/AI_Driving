#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacingAITypes.h"
#include "RacingLivery.h"
#include "RaceGridSpawner.generated.h"

class APawn;
class ARacingSpline;
class UMeshComponent;
class URaceParticipantComponent;
class URacingAIProfile;

/**
 * 그리드 한 자리의 구성입니다.
 *
 * 배열에서의 순서가 곧 그리드 순서입니다. 0번이 폴 포지션이고,
 * 플레이어를 3번째에 세우고 싶으면 세 번째 항목의 Occupant를 Player로 두면 됩니다.
 */
USTRUCT(BlueprintType)
struct RACINGAI_API FRaceGridSlot
{
	GENERATED_BODY()

	/** 이 자리에 AI를 세울지, 사람이 타는 차를 옮겨 놓을지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	ERaceGridOccupant Occupant = ERaceGridOccupant::AI;

	/** 난이도 프로필. AI 자리에만 씁니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI", meta = (EditCondition = "Occupant == ERaceGridOccupant::AI"))
	TObjectPtr<URacingAIProfile> Profile = nullptr;

	/** 리더보드 표시 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	FText DriverName;

	/** 중심선 기준 좌우 오프셋 (cm). 오른쪽이 양수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	float LaneOffset = 0.f;

	/**
	 * 도색 이름입니다. 비워 두면 자동으로 배정됩니다.
	 *
	 * 스포너의 Livery Set에 있는 이름을 적으세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	FName LiveryName;

	/** 이 자리만 다른 차량을 쓰고 싶을 때 지정합니다. AI 자리에만 씁니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI", meta = (EditCondition = "Occupant == ERaceGridOccupant::AI"))
	TSubclassOf<APawn> VehicleClassOverride;
};

/**
 * 참가 차량을 그리드에 세우고 레이스를 준비합니다.
 *
 * 레벨에 이 액터 하나와 ARacingSpline 하나만 놓으면 됩니다. 차량마다 블루프린트를
 * 따로 만들 필요 없이, 스폰 시점에 주행 컴포넌트와 입력 어댑터를 붙입니다.
 *
 * 이 액터는 배치와 구성만 담당합니다. 레이스의 시작과 종료, 리셋은
 * URaceDirectorSubsystem이 관리하므로, 메인 프로젝트의 중앙 매니저는 그쪽을
 * 잡으면 됩니다. Auto Start를 끄면 이 액터는 완전히 수동적인 배치 설정이 됩니다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Race Grid Spawner"))
class RACINGAI_API ARaceGridSpawner : public AActor
{
	GENERATED_BODY()

public:
	ARaceGridSpawner();

	//--------------------------------------------------------------------------
	// 차량
	//--------------------------------------------------------------------------

	/** 스폰할 기본 차량 폰 클래스입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicles")
	TSubclassOf<APawn> VehicleClass;

	/**
	 * 스폰한 차량에 붙일 입력 어댑터 클래스입니다.
	 *
	 * Chaos 차량이면 UChaosVehicleInputAdapter를 지정하세요. 폰 자체가
	 * IRacingVehicleInput을 구현한다면 비워 두어도 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicles")
	TSubclassOf<UActorComponent> VehicleInputAdapterClass;

	/** 그리드 구성입니다. 배열 순서가 그리드 순서이고, 0번이 폴 포지션입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicles")
	TArray<FRaceGridSlot> GridSlots;

	//--------------------------------------------------------------------------
	// 배치
	//--------------------------------------------------------------------------

	/**
	 * 폴 포지션의 위치입니다. 트랙의 결승선을 기준으로 잰 거리 (cm)입니다.
	 *
	 * 0이면 결승선 바로 위에서 출발합니다. 음수를 넣으면 결승선 뒤에서 출발하므로,
	 * 출발선과 결승선이 눈에 띄게 갈라집니다. 예를 들어 -20000이면 결승선 200m 앞에서
	 * 출발해 한 바퀴를 돈 뒤 결승선을 통과합니다.
	 *
	 * 결승선 자체의 위치는 Racing Spline의 Finish Line Distance에서 정합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	float PoleDistance = 0.f;

	/** 앞뒤 그리드 간격 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "100.0"))
	float RowSpacing = 900.f;

	/** 스폰 시 지면 위로 띄울 높이 (cm). 스플라인이 노면에 붙어 있으면 필요합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	float SpawnHeight = 80.f;

	//--------------------------------------------------------------------------
	// 레이스
	//--------------------------------------------------------------------------

	/** 목표 랩 수입니다. 0이면 완주 판정 없이 계속 달립니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race", meta = (ClampMin = "0"))
	int32 TotalLaps = 2;

	/**
	 * 켜면 배치 직후 곧바로 카운트다운을 시작합니다.
	 *
	 * 기본은 꺼져 있습니다. Play를 누르면 차들이 그리드에 서서 대기만 하고,
	 * 중앙 매니저가 Director의 Start Countdown 또는 Restart Race를 부를 때
	 * 3, 2, 1 카운트다운이 돌고 출발합니다. 플레이어 조작도 그때 풀립니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race")
	bool bAutoStart = false;

	/**
	 * 레이스 제한 시간 (초). 0이면 무제한입니다.
	 *
	 * 행사장에서는 반드시 넣으세요. 손님이 사고로 멈추면 완주 조건이 충족되지 않아
	 * 세션이 끝나지 않습니다. 2분 체험이면 120~150 정도가 적당합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race", meta = (ClampMin = "0.0", Units = "s"))
	float RaceTimeLimitSeconds = 0.f;

	/** 출발까지의 대기 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race", meta = (ClampMin = "0.0", Units = "s"))
	float CountdownSeconds = 3.f;

	/**
	 * 그리드에 Player 자리가 없을 때, 사람이 타는 차를 있는 자리에서 참가자로 등록합니다.
	 *
	 * Player 자리를 두면 이 옵션은 무시됩니다. 등록되지 않으면 러버밴딩의 기준점이
	 * 없어 AI가 보정 없이 달립니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race")
	bool bAutoTagPlayerPawn = true;

	//--------------------------------------------------------------------------
	// Director 기본값
	//
	// Director는 월드 서브시스템이라 레벨에 배치할 수 없습니다. 튜닝에 자주 만지는
	// 값만 여기에 노출하고 그리드를 세울 때 한 번 전달합니다. 중앙 매니저가
	// 나중에 Director에서 직접 바꿔도 됩니다.
	//--------------------------------------------------------------------------

	/** 레이싱 라인, 각 AI의 목표 차선, 순위와 목표 속도를 화면에 그립니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	bool bDrawDebug = false;

	/** 러버밴딩(따라잡기 보정)을 켭니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	bool bRubberBanding = true;

	/** 한 프레임에 주행 갱신할 AI의 최대 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director", meta = (ClampMin = "1"))
	int32 MaxAIUpdatesPerFrame = 1;

	/** 플레이어가 완주하면 레이스를 끝냅니다. 끄면 전원이 완주해야 끝납니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	bool bEndRaceWhenPlayerFinishes = true;

	//--------------------------------------------------------------------------
	// 조작 키
	//
	// 이 액터가 직접 키를 받습니다. 차량 폰이 아니므로 출발 전 플레이어 입력 잠금과
	// 무관하게 동작합니다. 중앙 매니저가 생기기 전까지 이 키로 시험할 수 있고,
	// 행사장에서는 운영자용 시작 키로 그대로 써도 됩니다.
	//--------------------------------------------------------------------------

	/** 아래 키 바인딩을 사용합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls")
	bool bEnableControlKeys = true;

	/** 대기 중일 때 누르면 카운트다운이 시작됩니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (EditCondition = "bEnableControlKeys"))
	FKey StartRaceKey;

	/**
	 * 그리드로 되돌리고 대기 상태로 둡니다. 출발시키지는 않습니다.
	 *
	 * 행사장에서 가장 자주 쓰게 되는 키입니다. 레이스가 끝나면 눌러서 차를 제자리로
	 * 돌려놓고, 다음 손님이 자리를 잡는 동안 기다렸다가 출발 키를 누르면 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (EditCondition = "bEnableControlKeys"))
	FKey ResetRaceKey;

	/** 그리드로 되돌리고 곧바로 카운트다운까지 시작합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls", meta = (EditCondition = "bEnableControlKeys"))
	FKey RestartRaceKey;

	//--------------------------------------------------------------------------
	// 도색
	//--------------------------------------------------------------------------

	/** 사용할 도색 모음입니다. 비우면 도색을 건드리지 않습니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	TObjectPtr<URacingLiverySet> LiverySet = nullptr;

	/** 슬롯에 도색 이름이 비어 있으면 무작위로 배정합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	bool bRandomizeLiveries = true;

	/** 같은 레이스에 같은 도색이 겹치지 않게 합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	bool bAvoidDuplicateLiveries = true;

	/**
	 * 도색을 바를 메시 컴포넌트 이름입니다. 비우면 루트 메시를 씁니다.
	 *
	 * 차량이 IRacingVehicleLivery를 구현했다면 이 설정은 쓰이지 않습니다.
	 * 그쪽이 우선입니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	FName LiveryMeshComponentName;

	/** 도색을 바를 머티리얼 슬롯 인덱스입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery", meta = (ClampMin = "0"))
	int32 LiveryMaterialSlot = 0;

	//--------------------------------------------------------------------------
	// 무작위
	//--------------------------------------------------------------------------

	/** 0이면 매 실행마다 다른 결과, 그 외에는 항상 같은 결과가 나옵니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Randomization")
	int32 RandomSeed = 0;

	/** 슬롯을 그리드 자리에 무작위로 배치합니다. 슬롯 구성 자체는 바뀌지 않습니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Randomization")
	bool bShuffleGridOrder = false;

	/**
	 * 난이도를 Profile Pool에서 무작위로 뽑습니다.
	 *
	 * 기본은 꺼져 있습니다. 행사에서는 매 회차 난이도 구성이 같아야 체험 편차가
	 * 작습니다. 무작위로 두면 어떤 손님은 상급 AI만 셋을 만나게 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Randomization")
	bool bRandomizeProfiles = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Randomization", meta = (EditCondition = "bRandomizeProfiles"))
	TArray<TObjectPtr<URacingAIProfile>> ProfilePool;

	//--------------------------------------------------------------------------
	// 조작
	//--------------------------------------------------------------------------

	/**
	 * 차량을 스폰해 그리드에 세우고 Director에 자리를 등록합니다.
	 *
	 * BeginPlay에서 자동으로 한 번 호출됩니다. 이미 세워져 있다면 다시 세우지 않고,
	 * 재시작은 Director의 Reset Race를 쓰세요.
	 */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void BuildGrid();

	/** 대기 중이면 카운트다운을 시작합니다. 키 바인딩과 같은 동작입니다 */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void StartRace();

	/** 그리드로 되돌리고 대기 상태로 둡니다. 출발시키지 않습니다 */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void ResetRace();

	/** 그리드로 되돌리고 곧바로 카운트다운을 시작합니다 */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void RestartRace();

	/** 스폰된 AI 차량들입니다 */
	UFUNCTION(BlueprintPure, Category = "Race")
	TArray<APawn*> GetSpawnedVehicles() const;

protected:
	virtual void BeginPlay() override;

private:
	/** 한 자리를 처리합니다 */
	void PlaceSlot(ARacingSpline& Track, const FRaceGridSlot& Slot, int32 GridIndex, FRandomStream& Stream);

	/** AI 차량을 스폰합니다 */
	APawn* SpawnAIVehicle(const FRaceGridSlot& Slot, const FTransform& Transform, int32 GridIndex, FRandomStream& Stream);

	/** 사람이 타는 차를 이 자리로 옮깁니다. 아직 없으면 다시 시도합니다 */
	void PlacePlayer(const FRaceGridSlot& Slot, const FTransform& Transform, int32 GridIndex);
	void RetryPlacePlayer();

	/** Player 자리가 없을 때 플레이어를 제자리에서 등록만 합니다 */
	void TagPlayerInPlace();

	/** 컨트롤러가 준비되면 조작 키를 붙입니다 */
	void SetupControlKeys();

	FTimerHandle ControlKeyTimer;
	int32 ControlKeyAttempts = 0;

	/** 슬롯의 도색을 결정해 차량에 적용하고 참가자에 기록합니다 */
	void ResolveAndApplyLivery(AActor& Vehicle, URaceParticipantComponent& Participant, const FRaceGridSlot& Slot, FRandomStream& Stream);

	/** 도색을 바를 메시를 찾습니다 */
	UMeshComponent* FindLiveryMesh(AActor& Vehicle) const;

	/** 그리드 자리의 스플라인 거리와 시작 랩을 구합니다 */
	void ComputeGridDistance(const ARacingSpline& Track, int32 GridIndex, float& OutDistance, int32& OutInitialLap) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APawn>> SpawnedVehicles;

	/** 이미 사용한 도색 이름. 중복 회피에 씁니다 */
	TArray<FName> UsedLiveryNames;

	/** 플레이어 자리를 아직 채우지 못했을 때 보관해 둡니다 */
	FRaceGridSlot PendingPlayerSlot;
	FTransform PendingPlayerTransform = FTransform::Identity;
	int32 PendingPlayerGridIndex = INDEX_NONE;
	bool bHasPendingPlayerSlot = false;

	FTimerHandle PlayerPlacementTimer;
	int32 PlayerPlacementAttempts = 0;

	bool bGridBuilt = false;
};
