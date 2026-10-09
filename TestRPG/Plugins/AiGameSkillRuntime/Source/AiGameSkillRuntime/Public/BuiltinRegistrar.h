// 内置 Handler 注册 — 在模块 Startup 时调用

#pragma once

class USkillRuntimeSubsystem;

namespace SkillRuntime::Builtin
{
	/** 注册所有内置 Handler */
	void RegisterAll(USkillRuntimeSubsystem& Subsys);
}
