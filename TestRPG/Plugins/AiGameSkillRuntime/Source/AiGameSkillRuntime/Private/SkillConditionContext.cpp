// FSkillConditionContext 实现

#include "SkillConditionContext.h"
#include "SkillInstance.h"

FSkillVarValue FSkillConditionContext::ResolveValue(const TSharedPtr<FJsonValue>& JsonValue) const
{
	if (!Instance || !JsonValue.IsValid()) return FSkillVarValue();
	return Instance->ResolveValue(JsonValue);
}

FSkillVarValue FSkillConditionContext::ResolveParam(const FString& Key) const
{
	if (!Instance || !Params.IsValid()) return FSkillVarValue();
	return Instance->ResolveField(Params, Key);
}
