// AiGameSkillRuntime 模块入口实现

#include "AiGameSkillRuntime.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SkillRuntimeSubsystem.h"
#include "BuiltinRegistrar.h"

DEFINE_LOG_CATEGORY(LogSkillRuntime);

class FAiGameSkillRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("AiGameSkillRuntime module started"));

		// 延迟注册：等 GameInstance 和 Subsystem 就绪后自动注册
		// USkillRuntimeSubsystem::Initialize 会调用 RegisterBuiltinHandlers
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("AiGameSkillRuntime module shut down"));
	}
};

IMPLEMENT_MODULE(FAiGameSkillRuntimeModule, AiGameSkillRuntime)
