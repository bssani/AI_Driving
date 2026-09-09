#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RacingLivery.h"
#include "RacingLiveryApplierComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;

/**
 * 도색을 바를 자리 하나입니다. 메시 컴포넌트와 그 안의 슬롯 번호로 콕 집어 지정합니다.
 *
 * 차체가 문, 지붕, 보닛처럼 여러 메시로 쪼개져 있고 각 메시마다 슬롯이 여럿이면,
 * 발라야 하는 자리는 "차체 메시의 3번, 문 메시의 6번"처럼 흩어져 있습니다.
 * 이름이나 머티리얼로는 그 조합을 집어낼 수 없는 경우가 있어 직접 적을 길을 둡니다.
 *
 * 어느 자리인지 모르겠으면 `racing.DumpMaterialSlots`로 먼저 찍어 보세요.
 */
USTRUCT(BlueprintType)
struct RACINGAI_API FRacingLiverySlot
{
	GENERATED_BODY()

	/** 메시 컴포넌트 이름. 비우면 모든 메시의 같은 번호 슬롯이 대상이 됩니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	FName MeshComponentName;

	/** 머티리얼 슬롯 번호 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery", meta = (ClampMin = "0"))
	int32 SlotIndex = 0;
};

/**
 * 도색을 차량 전체에 발라 주는 컴포넌트입니다. 차량 폰에 붙이기만 하면 됩니다.
 *
 * 그리드 스포너는 차량이나 그 컴포넌트 중에 IRacingVehicleLivery 구현체가 있으면 도색을
 * 통째로 맡깁니다. 이 컴포넌트가 그 구현체라, 붙여 두면 스포너가 알아서 부릅니다.
 *
 * 해결하려는 문제는 하나입니다. 차체가 문, 지붕, 보닛처럼 메시 여러 개로 나뉘어 있고 각
 * 메시마다 슬롯이 여럿이면, 발라야 할 자리가 "차체 메시의 3번, 문 메시의 6번"처럼 흩어집니다.
 * 스포너처럼 "메시 하나의 슬롯 하나"만 지정할 수 있는 방식으로는 닿지 않습니다.
 *
 * 찾는 방법이 셋이고, 하나만 맞아도 칠합니다. 차량 구성에 따라 맞는 것을 고르세요.
 *
 * - `PaintSlots` — 메시 이름과 슬롯 번호로 콕 집습니다. 가장 확실하고, 아래 둘로 집어낼
 *   수 없을 때 씁니다
 * - `PaintMaterials` — 지금 그 슬롯에 발려 있는 것으로 찾습니다. 자리가 흩어져 있어도
 *   도장 머티리얼이 같다면 한 줄로 끝납니다. 베이스가 같으면 인스턴스도 함께 잡히므로,
 *   차체와 트림이 베이스를 공유하는 차량에서는 과하게 잡힐 수 있습니다
 * - `PaintSlotNames` — 슬롯 이름으로 찾습니다. 이름이 분명하면 이쪽이 읽기 좋습니다
 *
 * 어느 자리인지 모르겠으면 먼저 `racing.DumpMaterialSlots`로 찍어 보세요.
 * 메시별 슬롯 번호와 이름, 지금 발린 머티리얼과 그 베이스가 전부 나옵니다.
 *
 * 차량마다 자식 블루프린트를 색깔 수만큼 만드는 방법도 있지만 권하지 않습니다. 색이 늘 때마다
 * 에셋이 늘고, 차량 설정을 고칠 때마다 전부 손봐야 하며, 무엇보다 런타임에 색을 고를 수
 * 없어 그리드마다 색을 섞을 수 없습니다.
 */
UCLASS(ClassGroup = "Racing AI", meta = (BlueprintSpawnableComponent, DisplayName = "Racing Livery Applier"))
class RACINGAI_API URacingLiveryApplierComponent : public UActorComponent, public IRacingVehicleLivery
{
	GENERATED_BODY()

public:
	URacingLiveryApplierComponent();

	/**
	 * 도색을 바를 자리를 메시와 슬롯 번호로 직접 지정합니다.
	 *
	 * 가장 확실한 방법이고, 아래 두 가지로 집어낼 수 없을 때 씁니다. 차체가 문·지붕·보닛으로
	 * 쪼개져 있고 슬롯 이름도 머티리얼도 공통점이 없다면 여기에 자리를 나열하세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	TArray<FRacingLiverySlot> PaintSlots;

	/**
	 * 도색 대상으로 볼 머티리얼입니다. 여기 있는 것과 베이스가 같은 슬롯을 모두 칠합니다.
	 *
	 * 자리가 여럿이어도 지금 발린 도장 머티리얼이 같다면 이쪽이 한 줄로 끝납니다.
	 * 다만 차체와 트림이 베이스를 공유하는 차량에서는 의도하지 않은 슬롯까지 잡히므로,
	 * 그럴 때는 위의 PaintSlots나 아래의 슬롯 이름을 쓰세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	TArray<TObjectPtr<UMaterialInterface>> PaintMaterials;

	/**
	 * 도색 대상으로 볼 머티리얼 슬롯 이름입니다.
	 *
	 * 위의 머티리얼 방식으로 잡히지 않는 슬롯을 추가로 지정할 때 씁니다. 둘 중 하나만
	 * 맞아도 칠합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	TArray<FName> PaintSlotNames;

	/**
	 * 색을 넣을 머티리얼 파라미터 이름입니다. 비우면 색은 넣지 않습니다.
	 *
	 * 도색마다 머티리얼을 따로 만드는 것보다 이쪽이 낫습니다. 머티리얼 하나에 벡터
	 * 파라미터 하나만 있으면 색이 몇 개든 에셋은 그대로입니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	FName ColorParameterName = TEXT("BodyColor");

	/**
	 * 도색에 머티리얼이 지정돼 있으면 슬롯을 그것으로 교체합니다.
	 *
	 * 끄면 파라미터로 색만 바꿉니다. 데칼이나 무광/유광까지 달라지는 도색이라면 켜세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	bool bReplaceMaterial = true;

	/**
	 * 대상 메시를 직접 지정합니다. 비워 두면 액터의 모든 메시 컴포넌트를 훑습니다.
	 *
	 * 훑는 비용은 스폰할 때 한 번뿐이므로 보통 비워 두면 됩니다. 바퀴나 유리처럼
	 * 실수로 칠해지면 곤란한 메시가 같은 머티리얼을 쓴다면 여기서 좁히세요.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	TArray<FName> MeshComponentNames;

	/** 무엇을 칠했는지 로그로 남깁니다. 처음 붙일 때 켜 두면 대상을 확인하기 좋습니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Livery")
	bool bLogApplied = false;

	//--------------------------------------------------------------------------
	// IRacingVehicleLivery
	//--------------------------------------------------------------------------

	virtual void ApplyLivery_Implementation(const FRacingLivery& Livery) override;

	/** 이 액터의 메시와 슬롯 구성을 사람이 읽을 수 있게 만듭니다 */
	UFUNCTION(BlueprintCallable, Category = "Livery")
	static FString DescribeMaterialSlots(AActor* Actor);

private:
	/** 대상 메시를 모읍니다 */
	void GatherMeshes(TArray<UMeshComponent*>& OutMeshes) const;

	/** 이 슬롯이 도색 대상인지 */
	bool IsPaintSlot(const UMeshComponent& Mesh, int32 SlotIndex, FName SlotName) const;
};
