// ISkillAction — 行为节点 / 单帧行为 的执行接口
// 注册到 USkillRuntimeSubsystem，由 ActionDispatcher 按 ActionType 分发

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class USkillInstance;

class ISkillAction
{
public:
	virtual ~ISkillAction() = default;

	/** 执行行为。Params 是节点 data 的完整 JSON（包含 actionType 等所有字段） */
	virtual void Execute(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params) = 0;

	/** 行为类型标识（用于注册/查询，如 "PlayAnim", "SetVariable"） */
	virtual FName GetActionType() const = 0;
};
