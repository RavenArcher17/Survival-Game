using UnrealBuildTool;

public class WildreachTarget : TargetRules
{
	public WildreachTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Wildreach");
	}
}
