#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChaosBrakeReverseGuardComponent.generated.h"

class UChaosWheeledVehicleMovementComponent;

/**
 * 달리는 중에 브레이크를 밟아도 후진 기어가 들어가지 않게 합니다. 사람이 모는 차에 붙입니다.
 *
 * Chaos 차량의 bReverseAsBrake는 기본으로 켜져 있고, 켜져 있으면 브레이크 입력이 들어오는 순간
 * 목표 기어를 -1로 바꿉니다. 속도를 확인하는 조건은 엔진 코드에서 주석 처리되어 있어 시속 150 km로
 * 달리는 중에도 곧바로 후진 기어가 됩니다(UChaosVehicleMovementComponent::UpdateState).
 * 차는 여전히 앞으로 굴러가므로 바퀴가 후진 기어비로 엔진을 돌리고, 후진 기어비는 5단의 4배쯤이라
 * RPM이 치솟습니다. 엔진음이 RPM을 따라가므로 브레이크를 밟는 순간 엔진이 크게 웁니다.
 *
 * 그 기능을 통째로 끄면 소리는 해결되지만, 벽에 박힌 차를 빼려고 브레이크를 길게 눌러 후진하는
 * 조작도 함께 사라집니다. 그래서 거의 섰을 때만 켜 둡니다. 달리는 동안 브레이크는 그냥 브레이크이고,
 * 멈춘 뒤에도 계속 누르면 예전처럼 후진합니다.
 *
 * AI 차에는 효과가 없습니다. AI는 대기 중에 브레이크를 누르고 있어서, 멈춘 상태에서 이 기능이 켜지면
 * 그리드에서 뒤로 갑니다. 그래서 폰을 사람이 조종하지 않으면 첫 틱에 스스로 꺼집니다. AI와 플레이어가
 * 같은 차량 블루프린트를 써도 안심하고 붙여 두면 됩니다.
 */
UCLASS(ClassGroup = "Racing AI", meta = (BlueprintSpawnableComponent, DisplayName = "Brake Reverse Guard"))
class RACINGAICHAOS_API UChaosBrakeReverseGuardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UChaosBrakeReverseGuardComponent();

	/**
	 * 앞으로 이 속도(cm/s)보다 느릴 때만 브레이크로 후진할 수 있습니다.
	 *
	 * 200이면 약 7 km/h입니다. 크게 둘수록 그 속도에서 후진 기어가 들어가 다시 RPM이 뜁니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brake Reverse Guard", meta = (ClampMin = "0.0"))
	float ReverseEngageSpeed = 200.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;

	/** 사람이 모는 차인지 확인했는지. 빙의는 BeginPlay보다 늦게 끝나므로 첫 틱에 봅니다 */
	bool bOwnerChecked = false;
};
