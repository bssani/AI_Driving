using UnrealBuildTool;

/**
 * 에디터 전용 운영 도구 모듈입니다.
 *
 * Race Setup 패널(AI 구성 프리셋)이 여기 있습니다. 패키징한 게임에는 들어가지 않습니다.
 * 플레이어 차량에 의존하지 않으므로 어느 프로젝트로 옮겨도 그대로 동작합니다.
 */
public class RacingAIEditor : ModuleRules
{
	public RacingAIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"RacingAI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"ToolMenus",
			"Settings"
		});
	}
}
