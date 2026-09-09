#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RacingLivery.h"
#include "RacingLiveryApplierComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;

/**
 * 도색을 차량 전체에 발라 주는 컴포넌트입니다. 차량 폰에 붙이기만 하면 됩니다.
 *
 * 그리드 스포너는 차량이나 그 컴포넌트 중에 IRacingVehicleLivery 구현체가 있으면 도색을
 * 통째로 맡깁니다. 이 컴포넌트가 그 구현체라, 붙여 두면 스포너가 알아서 부릅니다.
 *
 * 해결하려는 문제는 하나입니다. 차체가 메시 여러 개로 나뉘어 있고 머티리얼 슬롯의 이름과
 * 순서가 메시마다 다르면, "3번 슬롯을 바꿔라" 같은 지시가 통하지 않습니다. 그래서 슬롯
 * 번호나 이름이 아니라 **지금 그 슬롯에 무엇이 발려 있는가**로 찾습니다. 도색 머티리얼
 * 하나만 지정해 두면 그것을 쓰는 슬롯이 어느 메시의 몇 번이든 전부 잡힙니다.
 * 인스턴스도 같이 잡힙니다. 베이스 머티리얼이 같으면 한 식구로 봅니다.
 *
 * 슬롯 구성을 모르겠으면 먼저 `racing.DumpMaterialSlots`로 찍어 보세요.
 * 메시별 슬롯 이름과 지금 발린 머티리얼이 전부 나옵니다.
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
	 * 도색 대상으로 볼 머티리얼입니다. 여기 있는 것과 베이스가 같은 슬롯을 모두 칠합니다.
	 *
	 * 슬롯 이름이 제각각일 때 쓰는 주된 방법입니다. 차체 도장 머티리얼 하나만 넣으세요.
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
