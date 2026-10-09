using UnrealBuildTool;

// AGMaker generated minimal target. Keep Latest for installed-engine compatibility.
public class TestRPGEditorTarget : TargetRules
{
    public TestRPGEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
    }
}
