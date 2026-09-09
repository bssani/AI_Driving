// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AI_Driving : ModuleRules
{
	public AI_Driving(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"Niagara",
			"UMG",
			"Slate",
			"SlateCore",
			"HeadMountedDisplay"
		});

		PublicIncludePaths.AddRange(new string[] {
			"AI_Driving",
			"AI_Driving/SportsCar",
			"AI_Driving/OffroadCar",
			"AI_Driving/Variant_Offroad",
			"AI_Driving/Variant_TimeTrial",
			"AI_Driving/Variant_TimeTrial/UI",
			"AI_Driving/VR"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// The hand tracking probe reports both routes into the runtime side by side. The Meta
		// route is optional so that deleting the Meta XR plugin doesn't break the build.
		bool bHasMetaXR = Target.ProjectFile != null
			&& Directory.Exists(Path.Combine(Target.ProjectFile.Directory.FullName, "Plugins", "MetaXR"));

		if (bHasMetaXR)
		{
			PrivateDependencyModuleNames.Add("OculusXRInput");
		}

		PublicDefinitions.Add("WITH_METAXR_HANDS=" + (bHasMetaXR ? "1" : "0"));

		// The sound plugin lives in its own repository and is linked in as a junction, so a fresh
		// clone of this project won't have it until that is set up. Keep it optional rather than
		// failing the build: the cost of it missing is silence, not a broken project.
		bool bHasVehicleSound = Target.ProjectFile != null
			&& Directory.Exists(Path.Combine(Target.ProjectFile.Directory.FullName, "Plugins", "VehicleSoundSystem"));

		if (bHasVehicleSound)
		{
			PrivateDependencyModuleNames.Add("VehicleSoundSystem");
		}

		PublicDefinitions.Add("WITH_VEHICLE_SOUND=" + (bHasVehicleSound ? "1" : "0"));

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
