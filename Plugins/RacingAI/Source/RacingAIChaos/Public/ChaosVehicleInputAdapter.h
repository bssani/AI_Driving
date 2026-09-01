#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RacingVehicleInput.h"
#include "ChaosVehicleInputAdapter.generated.h"

class UChaosWheeledVehicleMovementComponent;

/**
 * AI의 제어 입력을 Chaos 차량에 꽂아 주는 어댑터입니다.
 *
 * 사용법은 폰에 이 컴포넌트를 붙이는 것이 전부입니다. Racing AI Driver 컴포넌트가
 * 소유 액터의 컴포넌트 중에서 이 어댑터를 자동으로 찾습니다.
 *
 * 코어 모듈(RacingAI)이 ChaosVehicles에 의존하지 않도록 이 클래스만 별도 모듈에
 * 격리해 두었습니다. 다른 차량 물리로 옮길 때 교체 대상은 이 파일 하나뿐입니다.
 */
UCLASS(ClassGroup = "Racing AI", meta = (BlueprintSpawnableComponent, DisplayName = "Chaos Vehicle Input Adapter"))
class RACINGAICHAOS_API UChaosVehicleInputAdapter : public UActorComponent, public IRacingVehicleInput
{
	GENERATED_BODY()

public:
	UChaosVehicleInputAdapter();

	/**
	 * 자동 변속을 쓸지 여부입니다.
	 *
	 * 끄면 후진 요청 시에만 수동으로 기어를 바꿉니다. 켜 두는 편이 무난합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	bool bUseAutomaticGears = true;

	/**
	 * Chaos의 "입력에 컨트롤러 필요" 요구를 끕니다.
	 *
	 * 기본값 true를 유지하세요. Chaos는 컨트롤러가 빙의한 로컬 폰의 입력만 처리하므로,
	 * 아무도 빙의하지 않는 AI 차량은 이걸 끄지 않으면 전혀 움직이지 않습니다.
	 * AIController를 따로 붙여 빙의시키는 방식을 쓴다면 꺼도 됩니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Chaos Quirks")
	bool bDisableControllerRequirement = true;

	/**
	 * Chaos의 "브레이크가 곧 후진" 아케이드 동작을 끕니다.
	 *
	 * 기본값 true를 유지하세요. 켜 두면 AI가 감속하려고 브레이크를 밟을 때마다
	 * 목표 기어가 -1로 바뀌어 의도치 않게 후진합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI|Chaos Quirks")
	bool bDisableReverseAsBrake = true;

	//--------------------------------------------------------------------------
	// IRacingVehicleInput
	//--------------------------------------------------------------------------

	virtual void ApplySteering_Implementation(float Value) override;
	virtual void ApplyThrottle_Implementation(float Value) override;
	virtual void ApplyBrake_Implementation(float Value) override;
	virtual void SetReverseGear_Implementation(bool bReverse) override;

protected:
	virtual void BeginPlay() override;

private:
	/** 소유 액터에서 찾은 Chaos 무브먼트 컴포넌트입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;

	/** 현재 후진 상태인지 */
	bool bReversing = false;
};
