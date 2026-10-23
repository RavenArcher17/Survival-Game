using UnrealBuildTool;

public class WildreachEditorTarget : TargetRules
{
	public WildreachEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Wildreach");
	}
}
