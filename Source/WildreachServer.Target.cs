using UnrealBuildTool;

// The packaged dedicated server (cooks/build-wild-100players.md 4): needs Unreal built from source (Tools\build_engine.ps1);
// the launcher engine can't build Server targets. Until then a dedicated server runs as the editor binary with -server.
public class WildreachServerTarget : TargetRules
{
	public WildreachServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Wildreach");
	}
}
