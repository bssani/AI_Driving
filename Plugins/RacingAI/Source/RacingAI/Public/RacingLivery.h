#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/Interface.h"
#include "RacingLivery.generated.h"

class UMaterialInterface;

/**
 * 차량 도색 하나입니다.
 *
 * 이름을 갖고 있으므로 스폰하는 쪽에서 "Racing Red"처럼 지정해 부를 수 있고,
 * UI나 리더보드에도 그대로 쓸 수 있습니다.
 */
USTRUCT(BlueprintType)
struct RACINGAI_API FRacingLivery
{
	GENERATED_BODY()

	/** 코드와 그리드 설정에서 부르는 이름입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	FName Name;

	/** 화면에 보여 줄 이름입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	FText DisplayName;

	/** 차체에 적용할 머티리얼입니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	/**
	 * 이 도색을 대표하는 색입니다.
	 *
	 * 머티리얼을 읽을 수 없는 곳(미니맵 아이콘, 순위표 색점, 관중용 화면)에서
	 * 같은 차를 같은 색으로 표시하기 위해 별도로 들고 있습니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	FLinearColor TintColor = FLinearColor::White;
};

/**
 * 도색 모음입니다.
 *
 * 어떤 도색이 있는지만 들고 있고, 그것을 차량 어디에 어떻게 바르는지는 모릅니다.
 * 메시 구조를 아는 순간 AI 플러그인이 차량 구현에 묶이기 때문입니다.
 */
UCLASS(BlueprintType)
class RACINGAI_API URacingLiverySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Livery")
	TArray<FRacingLivery> Liveries;

	/** 이름으로 찾습니다. 없으면 false */
	UFUNCTION(BlueprintCallable, Category = "Livery")
	bool FindLivery(FName InName, FRacingLivery& OutLivery) const;

	/**
	 * 무작위로 하나 고릅니다.
	 *
	 * ExcludeNames에 든 이름은 건너뜁니다. 같은 레이스에 같은 색 차가 두 대 서면
	 * 관중도 플레이어도 헷갈리기 때문에, 호출하는 쪽에서 이미 쓴 이름을 넘깁니다.
	 * 남은 후보가 없으면 제외 목록을 무시하고 전체에서 고릅니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Livery")
	bool PickRandomLivery(int32 Seed, const TArray<FName>& ExcludeNames, FRacingLivery& OutLivery) const;
};

UINTERFACE(BlueprintType, meta = (DisplayName = "Racing Vehicle Livery"))
class RACINGAI_API URacingVehicleLivery : public UInterface
{
	GENERATED_BODY()
};

/**
 * 차량이 자기 도색을 스스로 적용하고 싶을 때 구현합니다.
 *
 * 구현하지 않아도 됩니다. 그 경우 그리드 스포너가 지정된 메시의 지정된 슬롯에
 * 머티리얼을 직접 적용합니다. 다만 차체가 여러 메시로 나뉘어 있거나 데칼,
 * 번호판, 팀 로고까지 같이 바꿔야 한다면 이 인터페이스를 구현하는 편이 낫습니다.
 *
 * 구현되어 있으면 스포너는 기본 적용을 건너뛰고 이 함수만 호출합니다.
 */
class RACINGAI_API IRacingVehicleLivery
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Racing AI|Livery")
	void ApplyLivery(const FRacingLivery& Livery);
	virtual void ApplyLivery_Implementation(const FRacingLivery& Livery) {}
};
