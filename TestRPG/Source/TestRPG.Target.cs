using UnrealBuildTool;

// AGMaker generated minimal target. Keep Latest for installed-engine compatibility.
public class TestRPGTarget : TargetRules
{
    public TestRPGTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
    }
}
