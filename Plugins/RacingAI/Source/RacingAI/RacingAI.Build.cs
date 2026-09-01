using UnrealBuildTool;

/**
 * 레이싱 AI 코어 모듈입니다.
 *
 * 의도적으로 차량 물리 플러그인에 의존하지 않습니다. AI는 조향/스로틀/브레이크
 * 세 개의 float만 만들고, 그것을 적용하는 일은 IRacingVehicleInput 구현체가 맡습니다.
 * 덕분에 Chaos든 커스텀 폰이든 이 모듈을 그대로 재사용할 수 있습니다.
 */
public class RacingAI : ModuleRules
{
	public RacingAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"DeveloperSettings"
		});
	}
}
