// SetVariable Action 实现

#include "SetVariableAction.h"
#include "SkillInstance.h"
#include "AiGameSkillRuntime.h"

void FSetVariableAction::Execute(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params)
{
	if (!Instance) return;

	// 读 targetVar
	const TSharedPtr<FJsonObject>* SingleActionParams;
	if (!Params->TryGetObjectField(TEXT("singleActionParams"), SingleActionParams)) return;

	FString TargetVar;
	if (!(*SingleActionParams)->TryGetStringField(TEXT("targetVar"), TargetVar)) return;
	if (TargetVar.IsEmpty()) return;

	// 读 operator
	FString Operator = TEXT("=");
	(*SingleActionParams)->TryGetStringField(TEXT("operator"), Operator);

	// 读 value（支持 VarRef）
	FSkillVarValue Value;
	TSharedPtr<FJsonValue> ValueJson = (*SingleActionParams)->TryGetField(TEXT("value"));
	if (ValueJson.IsValid())
	{
		Value = Instance->ResolveValue(ValueJson);
	}

	// 当前值
	FSkillVarValue Current = Instance->GetVariable(TargetVar);

	// 辅助 lambda：取 number 值
	auto GetNum = [](const FSkillVarValue& V) -> double
	{
		return (V.Type == FSkillVarValue::Number) ? V.NumberVal : 0.0;
	};

	// 应用运算
	if (Operator == TEXT("="))
	{
		Instance->SetVariable(TargetVar, Value);
	}
	else if (Operator == TEXT("+=") && Current.Type == FSkillVarValue::Number)
	{
		Instance->SetVariable(TargetVar, FSkillVarValue(Current.NumberVal + GetNum(Value)));
	}
	else if (Operator == TEXT("-=") && Current.Type == FSkillVarValue::Number)
	{
		Instance->SetVariable(TargetVar, FSkillVarValue(Current.NumberVal - GetNum(Value)));
	}
	else if (Operator == TEXT("*=") && Current.Type == FSkillVarValue::Number)
	{
		Instance->SetVariable(TargetVar, FSkillVarValue(Current.NumberVal * GetNum(Value)));
	}
	else if (Operator == TEXT("toggle") && Current.Type == FSkillVarValue::Bool)
	{
		Instance->SetVariable(TargetVar, FSkillVarValue(!Current.BoolVal));
	}
	else
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("SetVariable: unsupported operator '%s' for var '%s'"),
			*Operator, *TargetVar);
		return;
	}

	UE_LOG(LogSkillRuntime, Verbose, TEXT("SetVariable: %s %s → %s"),
		*TargetVar, *Operator, *Instance->GetVariable(TargetVar).ToString());
}
