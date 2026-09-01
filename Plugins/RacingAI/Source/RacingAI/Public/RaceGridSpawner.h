#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceGridSpawner.generated.h"

class APawn;
class ARacingSpline;
class URacingAIProfile;

/** 그리드 한 자리의 구성입니다. */
USTRUCT(BlueprintType)
struct RACINGAI_API FRaceGridSlot
{
	GENERATED_BODY()

	/** 이 자리에 앉힐 드라이버의 난이도 프로필 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	TObjectPtr<URacingAIProfile> Profile = nullptr;

	/** 리더보드 표시 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	FText DriverName;

	/** 중심선 기준 좌우 오프셋 (cm). 오른쪽이 양수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	float LaneOffset = 0.f;

	/**
	 * 이 자리만 다른 차량을 쓰고 싶을 때 지정합니다.
	 *
	 * 비워 두면 스포너의 VehicleClass를 씁니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	TSubclassOf<APawn> VehicleClassOverride;
};

/**
 * AI 차량을 그리드에 세우고 레이스를 출발시킵니다.
 *
 * 레벨에 이 액터 하나와 ARacingSpline 하나만 놓으면 테스트가 됩니다. 차량마다
 * 블루프린트를 따로 만들 필요 없이, 스폰 시점에 주행 컴포넌트와 입력 어댑터를 붙입니다.
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

	/** 그리드 구성입니다. 항목 수가 곧 AI 대수입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicles")
	TArray<FRaceGridSlot> GridSlots;

	//--------------------------------------------------------------------------
	// 배치
	//--------------------------------------------------------------------------

	/** 폴 포지션의 스플라인 거리 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0.0"))
	float PoleDistance = 0.f;

	/** 앞뒤 그리드 간격 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "100.0"))
	float RowSpacing = 900.f;

	/** 스폰 시 지면 위로 띄울 높이 (cm). 스플라인이 노면에 붙어 있으면 필요합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	float SpawnHeight = 80.f;

	//--------------------------------------------------------------------------
	// 진행
	//--------------------------------------------------------------------------

	/** 켜면 카운트다운 후 자동으로 출발합니다. 끄면 StartRace를 직접 호출하세요 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race")
	bool bAutoStart = true;

	/** 출발까지의 대기 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race", meta = (ClampMin = "0.0", Units = "s"))
	float CountdownSeconds = 3.f;

	/**
	 * 플레이어 폰에 참가자 컴포넌트를 자동으로 붙입니다.
	 *
	 * 켜 두면 플레이어의 랩과 순위가 함께 집계되고, 러버밴딩의 기준점이 생깁니다.
	 * 플레이어 폰에 이미 컴포넌트를 붙여 두었다면 끄세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race")
	bool bAutoTagPlayerPawn = true;

	//--------------------------------------------------------------------------
	// Director 설정
	//
	// Director는 월드 서브시스템이라 레벨에 배치할 수 없습니다. 튜닝에 자주 만지는
	// 값만 여기에 노출하고 BeginPlay에 한 번 전달합니다.
	//--------------------------------------------------------------------------

	/**
	 * 레이싱 라인, 각 AI의 목표 차선, 순위와 목표 속도를 화면에 그립니다.
	 *
	 * 튜닝할 때는 켜 두세요. AI가 무엇을 보고 달리는지 이것 없이는 알 수 없습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	bool bDrawDebug = false;

	/** 러버밴딩(따라잡기 보정)을 켭니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director")
	bool bRubberBanding = true;

	/** 한 프레임에 주행 갱신할 AI의 최대 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director", meta = (ClampMin = "1"))
	int32 MaxAIUpdatesPerFrame = 1;

	/** 지금 레이스를 시작합니다 */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void StartRace();

	/** 스폰된 AI 차량들입니다 */
	UFUNCTION(BlueprintPure, Category = "Race")
	TArray<APawn*> GetSpawnedVehicles() const;

protected:
	virtual void BeginPlay() override;

private:
	void SpawnGrid(ARacingSpline& Track);

	/** 플레이어 폰을 찾아 참가자로 등록합니다. 아직 없으면 다시 시도합니다 */
	void TryTagPlayerPawn();

	UPROPERTY(Transient)
	TArray<TObjectPtr<APawn>> SpawnedVehicles;

	FTimerHandle CountdownTimer;
	FTimerHandle PlayerTagTimer;

	int32 PlayerTagAttempts = 0;
};
