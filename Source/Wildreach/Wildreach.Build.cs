using UnrealBuildTool;

public class Wildreach : ModuleRules
{
	public Wildreach(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets files in subfolders (Tests/) include the module's headers directly.
		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"Json",
			"NetCore",
			"Landscape"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "ProceduralMeshComponent", "HTTP", "ImageWrapper" });  // lava / ice surfaces; the main menu's update check
		PrivateDependencyModuleNames.Add("MoviePlayer");  // the loading screen during map loads (WRUIFlow)

		// Editor-only map-building helpers (UWREditorTerrainLibrary).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
