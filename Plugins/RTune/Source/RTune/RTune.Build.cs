//Copyright 2024 P.Kallisto SKG

using System.IO;
using UnrealBuildTool;

public class RTune : ModuleRules
{
	public RTune(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));
		//PublicIncludePaths.Add("R:/UE VehiclePhysics/RTuneVehiclePhysics/Plugins/RTune/Source/ThirdParty/AsyncTickPhysics/Source/AsyncTickPhysics/Public");

        //PrivateIncludePaths.Add("R:/UE VehiclePhysics/RTuneVehiclePhysics/Plugins/RTune/Source/ThirdParty/AsyncTickPhysics/Source/AsyncTickPhysics/Private");


        PublicIncludePaths.AddRange(
			new string[] {

				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {

				// ... add other private include paths required here ...

				//As
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);

		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"AsyncTickPhysics",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
