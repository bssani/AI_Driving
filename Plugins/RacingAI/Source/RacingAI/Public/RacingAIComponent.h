#pragma once

#include "CoreMinimal.h"
#include "RaceParticipantComponent.h"
#include "RaceDirectorSubsystem.h"
#include "RacingAITypes.h"
#include "UObject/ScriptInterface.h"
#include "RacingAIComponent.generated.h"

class ARacingSpline;
class IRacingVehicleInput;
class URacingAIProfile;

/**
 * AI 드라이버의 두뇌입니다. 상대 차량 폰에 붙입니다.
 *
 * 스스로 틱하지 않습니다. Director가 프레임마다 몇 대씩만 골라 Advance를 호출하므로
 * 대수가 늘어도 프레임 비용이 균일합니다.
 *
 * 조향은 스플라인 pure pursuit으로, 속도는 전방 곡률에서 역산한 목표 속도로 만듭니다.
 * 차량에 값을 적용하는 일은 IRacingVehicleInput 구현체가 맡으므로 이 클래스는
 * 어떤 차량 물리와도 무관합니다.
 */
UCLASS(ClassGroup = "Racing AI", meta = (BlueprintSpawnableComponent, DisplayName = "Racing AI Driver"))
class RACINGAI_API URacingAIComponent : public URaceParticipantComponent
{
	GENERATED_BODY()

public:
	URacingAIComponent();

	/** 난이도 프로필입니다. 비우면 기본값으로 동작합니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	TObjectPtr<URacingAIProfile> Profile;

	/** 배정된 기본 차선 오프셋 (cm). 그리드에서 좌우로 벌려 세울 때 씁니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Racing AI")
	float BaseLaneOffset = 0.f;

	//--------------------------------------------------------------------------
	// 상태 (읽기 전용)
	//--------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|State")
	ERacingAIState State = ERacingAIState::Waiting;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|State")
	float TargetSpeed = 0.f;

	/**
	 * 앞차를 고려하지 않았을 때의 목표 속도입니다 (cm/s).
	 *
	 * 추월 판단의 기준으로 씁니다. "앞차가 나보다 느린가"를 현재 속도로 재면,
	 * 앞차 때문에 멈춘 순간 자기 속도도 0이 되어 판정이 뒤집히고 추월 의사가
	 * 껐다 켜졌다 합니다. 방해받지 않았을 때의 속도로 재야 안정적입니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|State")
	float FreeTargetSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|State")
	float CurrentSteering = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|State")
	float CurrentThrottle = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|State")
	float CurrentBrake = 0.f;

	/** 현재 사용 중인 차선 오프셋 (cm). 목표를 향해 서서히 이동합니다 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|State")
	float CurrentLaneOffset = 0.f;

	//--------------------------------------------------------------------------
	// Director가 매 프레임 써 넣는 지시입니다
	//--------------------------------------------------------------------------

	/** 러버밴딩 계수. 1이면 보정 없음 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Director")
	float SpeedScale = 1.f;

	/** 배정된 목표 차선 오프셋 (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Director")
	float TargetLaneOffset = 0.f;

	/** 앞선 차량 정보 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Director")
	FRacerAhead RacerAhead;

	/**
	 * Director가 추월을 지시했는지 여부입니다.
	 *
	 * 켜져 있으면 속도 제어가 최소 전진 속도를 유지해, 멈춘 차 뒤에서도
	 * 옆으로 빠져나갈 여지를 만듭니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Racing AI|Director")
	bool bIntendToPass = false;

	/**
	 * 직전 주행 갱신 이후 흐른 시간입니다. Director가 관리합니다.
	 *
	 * 분산 실행 때문에 프레임 델타와 다르며, 레이트 리밋과 타이머는 반드시 이 값을
	 * 써야 합니다. 프레임 수로 시간을 세면 프레임레이트에 따라 동작이 달라집니다.
	 */
	float TimeSinceAdvance = 0.f;

	//--------------------------------------------------------------------------
	// Director가 호출합니다
	//--------------------------------------------------------------------------

	/** 주행을 한 스텝 진행합니다. DeltaTime은 이 AI의 직전 갱신 이후 경과 시간입니다 */
	void Advance(const ARacingSpline& Track, float DeltaTime);

	/**
	 * 스폰 직후 설정을 주입합니다.
	 *
	 * 런타임에 컴포넌트를 추가하면 BeginPlay가 곧바로 돌아 버리므로, 프로필과 차선을
	 * 나중에 넣어도 반영되도록 별도 진입점을 둡니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing AI")
	void Configure(URacingAIProfile* InProfile, float InLaneOffset, const FText& InDisplayName);

	/** 그리드 대기 상태로 전환합니다 */
	void EnterWaiting();

	/** 주행을 시작합니다 */
	void BeginRacing();

	/** 유효한 프로필을 반환합니다. Profile이 비어 있으면 기본 인스턴스를 만듭니다 */
	const URacingAIProfile& GetEffectiveProfile() const;

protected:
	virtual void BeginPlay() override;

private:
	/** 소유 액터와 그 컴포넌트에서 IRacingVehicleInput 구현체를 찾습니다 */
	void ResolveVehicleInput();

	void ApplyInputs(float Steering, float Throttle, float Brake);

	/** 조향을 계산합니다. pure pursuit 곡률 방식 */
	float ComputeSteering(const ARacingSpline& Track, float DeltaTime);

	/** 목표 속도를 계산하고 스로틀/브레이크로 바꿉니다 */
	void ComputeSpeedControl(const ARacingSpline& Track, float DeltaTime, float& OutThrottle, float& OutBrake);

	/** 복구 타이머를 갱신하고 필요하면 상태를 전환합니다. true면 이번 프레임 주행을 건너뜁니다 */
	bool UpdateRecovery(const ARacingSpline& Track, float DeltaTime);

	/** 스플라인 위로 재배치합니다 */
	void RespawnOnTrack(const ARacingSpline& Track);

	/** 차량에 입력을 적용하는 구현체입니다 */
	UPROPERTY(Transient)
	TScriptInterface<IRacingVehicleInput> VehicleInput;

	/** Profile이 비었을 때 사용하는 기본 프로필입니다 */
	UPROPERTY(Transient)
	mutable TObjectPtr<URacingAIProfile> FallbackProfile;

	float StuckTimer = 0.f;
	float ReverseTimer = 0.f;
	float FlippedTimer = 0.f;

	/** 후진 중 조향 방향. 매번 바꿔가며 빠져나오도록 합니다 */
	float ReverseSteerSign = 1.f;
};
