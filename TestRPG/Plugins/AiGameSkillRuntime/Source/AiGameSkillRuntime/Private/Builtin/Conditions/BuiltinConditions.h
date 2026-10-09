// 内置条件评估函数 — Always / VarEquals / VarGreater / comparison / resource / input

#pragma once

#include "CoreMinimal.h"
#include "SkillConditionContext.h"

class USkillRuntimeSubsystem;

namespace SkillRuntime::BuiltinConditions
{
	void RegisterAll(USkillRuntimeSubsystem& Subsys);
}
