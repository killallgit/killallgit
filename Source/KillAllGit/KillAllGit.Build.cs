// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KillAllGit : ModuleRules
{
	public KillAllGit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"Json",
			"JsonUtilities",
			"HTTP"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"KillAllGit",
			"KillAllGit/GitHub",
			"KillAllGit/Tests",
			"KillAllGit/Variant_Platforming",
			"KillAllGit/Variant_Platforming/Animation",
			"KillAllGit/Variant_Combat",
			"KillAllGit/Variant_Combat/AI",
			"KillAllGit/Variant_Combat/Animation",
			"KillAllGit/Variant_Combat/Gameplay",
			"KillAllGit/Variant_Combat/Interfaces",
			"KillAllGit/Variant_Combat/UI",
			"KillAllGit/Variant_SideScrolling",
			"KillAllGit/Variant_SideScrolling/AI",
			"KillAllGit/Variant_SideScrolling/Gameplay",
			"KillAllGit/Variant_SideScrolling/Interfaces",
			"KillAllGit/Variant_SideScrolling/UI",
			"KillAllGit/SaveSystem",
			"KillAllGit/UI",
			"KillAllGit/Tests/SaveSystem",
			"KillAllGit/Tests/GitHub"
		});
	}
}
