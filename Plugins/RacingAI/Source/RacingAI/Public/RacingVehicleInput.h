#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RacingVehicleInput.generated.h"

UINTERFACE(BlueprintType, meta = (DisplayName = "Racing Vehicle Input"))
class RACINGAI_API URacingVehicleInput : public UInterface
{
	GENERATED_BODY()
};

/**
 * AI가 만든 제어 입력을 차량에 적용하는 통로입니다.
 *
 * 이 플러그인이 차량 물리에 의존하지 않는 이유가 여기에 있습니다. AI는 조향/스로틀/브레이크
 * 세 값만 만들고, 그것을 어떤 무브먼트 컴포넌트에 어떻게 꽂을지는 구현체가 정합니다.
 *
 * 구현 방법은 세 가지입니다.
 * - Chaos 차량이면 UChaosVehicleInputAdapter 컴포넌트를 폰에 붙이기만 하면 됩니다.
 * - 커스텀 C++ 폰이면 이 인터페이스를 직접 상속해 _Implementation을 채웁니다.
 * - 블루프린트 폰이면 클래스 세팅에서 인터페이스를 추가하고 이벤트 세 개를 구현합니다.
 *
 * AI 컴포넌트는 소유 액터와 그 컴포넌트들 중에서 구현체를 자동으로 찾습니다.
 */
class RACINGAI_API IRacingVehicleInput
{
	GENERATED_BODY()

public:
	/** 조향을 적용합니다. -1(좌) ~ 1(우) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Racing AI|Vehicle Input")
	void ApplySteering(float Value);
	virtual void ApplySteering_Implementation(float Value) {}

	/** 스로틀을 적용합니다. 0 ~ 1 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Racing AI|Vehicle Input")
	void ApplyThrottle(float Value);
	virtual void ApplyThrottle_Implementation(float Value) {}

	/** 브레이크를 적용합니다. 0 ~ 1 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Racing AI|Vehicle Input")
	void ApplyBrake(float Value);
	virtual void ApplyBrake_Implementation(float Value) {}

	/**
	 * 후진 여부를 알립니다. 스턱 탈출에 사용합니다.
	 *
	 * 구현이 필요 없으면 비워 두어도 됩니다. 그 경우 AI는 후진 대신 제자리에서
	 * 조향만 바꾸며 탈출을 시도합니다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Racing AI|Vehicle Input")
	void SetReverseGear(bool bReverse);
	virtual void SetReverseGear_Implementation(bool bReverse) {}
};
