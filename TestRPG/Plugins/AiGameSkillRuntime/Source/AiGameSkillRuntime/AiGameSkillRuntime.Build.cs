// AiGameSkillRuntime — 技能运行时模块
// 纯 Runtime，打包后可用。数据驱动 + 注册式扩展。

using UnrealBuildTool;

public class AiGameSkillRuntime : ModuleRules
{
	public AiGameSkillRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Json",             // TSharedPtr<FJsonObject> 加载技能数据
			"JsonUtilities",    // FJsonObjectConverter（备用）
			"AnimGraphRuntime", // UAnimSingleNodeInstance / UAnimMontage
			"Niagara",          // VFX 预览
		});

		// 历史 Build.cs 曾误挂 Slate/SlateCore 私有依赖，但源码并未引用任何 Slate API，
		// 此处已移除，避免无谓的模块耦合与编译面。
		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
