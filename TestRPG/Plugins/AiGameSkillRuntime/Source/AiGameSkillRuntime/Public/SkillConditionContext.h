// FSkillConditionContext — 条件评估上下文
// 传给注册的条件评估函数，提供变量读取等辅助

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "SkillDataTypes.h"

class USkillInstance;

struct FSkillConditionContext
{
	USkillInstance* Instance = nullptr;
	TSharedPtr<FJsonObject> Params;     // 条件的 properties JSON

	/** 解析值：如果是 { "__varRef": "X" } 则读变量，否则返回字面量 */
	FSkillVarValue ResolveValue(const TSharedPtr<FJsonValue>& JsonValue) const;

	/** 便捷：从 Params 中取字段并解析（字面量 or VarRef） */
	FSkillVarValue ResolveParam(const FString& Key) const;
};
