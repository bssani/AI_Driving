using UnrealBuildTool;

/**
 * Chaos Wheeled Vehicle 어댑터 모듈입니다.
 *
 * 코어(RacingAI)를 Chaos에 묶지 않기 위해 어댑터만 여기에 격리했습니다.
 * 다른 차량 물리로 갈아탈 때 이 모듈만 교체하면 됩니다.
 */
public class RacingAIChaos : ModuleRules
{
	public RacingAIChaos(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RacingAI",
			"ChaosVehicles"
		});
	}
}
