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
		// Sound: OGG files from Data/Audio decoded with the engine's Vorbis decoder (WRSound); its quality-info struct lives in TargetPlatform.
		PrivateDependencyModuleNames.AddRange(new string[] { "VorbisAudioDecoder", "AudioExtensions", "AudioMixer" });  // AudioExtensions: the procedural wave subclass (UWRPcmSound); AudioMixer: WRSoundRecord
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PrivateDependencyModuleNames.Add("AnimationCore");  // two-bone IK: the survivor's feet planted on slopes (WRAnimInstance)
		PrivateDependencyModuleNames.Add("MoviePlayer");  // the loading screen during map loads (WRUIFlow)
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });  // effects: WRFx draws its sprites with its own scene proxy

		// Editor-only map-building helpers (UWREditorTerrainLibrary).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
