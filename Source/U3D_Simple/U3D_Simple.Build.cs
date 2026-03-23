// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class U3D_Simple : ModuleRules
{
	public U3D_Simple(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PhysicsCore",         // 物理材质支持
			"NavigationSystem",    // 导航系统支持
			"AIModule",            // AI模块支持
			"GameplayTags"         // Gameplay标签系统支持
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"RenderCore"   // 调试绘制支持
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
