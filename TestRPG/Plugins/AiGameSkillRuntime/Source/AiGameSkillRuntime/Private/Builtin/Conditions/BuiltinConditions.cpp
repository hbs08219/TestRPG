// 内置条件评估函数实现

#include "BuiltinConditions.h"
#include "SkillRuntimeSubsystem.h"
#include "AiGameSkillRuntime.h"
#include "SkillInstance.h"

void SkillRuntime::BuiltinConditions::RegisterAll(USkillRuntimeSubsystem& Subsys)
{
	// Always — 永远为真
	Subsys.RegisterCondition(FName(TEXT("always")), [](const FSkillConditionContext& Ctx) -> bool
	{
		return true;
	});

	// VarEquals — 变量等于某值
	Subsys.RegisterCondition(FName(TEXT("varEquals")), [](const FSkillConditionContext& Ctx) -> bool
	{
		FSkillVarValue VarVal = Ctx.ResolveParam(TEXT("varName"));
		FSkillVarValue CompVal = Ctx.ResolveParam(TEXT("value"));
		return VarVal.ToString() == CompVal.ToString();
	});

	// VarGreater — 变量大于某值
	Subsys.RegisterCondition(FName(TEXT("varGreater")), [](const FSkillConditionContext& Ctx) -> bool
	{
		FSkillVarValue VarVal = Ctx.ResolveParam(TEXT("varName"));
		FSkillVarValue CompVal = Ctx.ResolveParam(TEXT("value"));
		if (VarVal.Type == FSkillVarValue::Number && CompVal.Type == FSkillVarValue::Number)
		{
			return VarVal.NumberVal > CompVal.NumberVal;
		}
		return false;
	});

	// comparison — 通用比较（编辑器内置 conditionType）
	Subsys.RegisterCondition(FName(TEXT("comparison")), [](const FSkillConditionContext& Ctx) -> bool
	{
		if (!Ctx.Params.IsValid()) return false;

		FString Operator = TEXT(">=");
		Ctx.Params->TryGetStringField(TEXT("operator"), Operator);

		FSkillVarValue Value = Ctx.ResolveParam(TEXT("value"));
		// comparison 暂时只比较字面量
		double NumVal = 0.0;
		if (Value.Type == FSkillVarValue::Number) NumVal = Value.NumberVal;

		if (Operator == TEXT(">=")) return NumVal >= 0;
		if (Operator == TEXT("<=")) return NumVal <= 0;
		if (Operator == TEXT(">"))  return NumVal > 0;
		if (Operator == TEXT("<"))  return NumVal < 0;
		if (Operator == TEXT("==")) return NumVal == 0;
		if (Operator == TEXT("!=")) return NumVal != 0;
		return false;
	});

	// resource — 资源条件（SP/HP 等）
	Subsys.RegisterCondition(FName(TEXT("resource")), [](const FSkillConditionContext& Ctx) -> bool
	{
		if (!Ctx.Params.IsValid()) return false;

		FString ResourceName;
		Ctx.Params->TryGetStringField(TEXT("resource"), ResourceName);

		FSkillVarValue Value = Ctx.ResolveParam(TEXT("value"));
		double Threshold = (Value.Type == FSkillVarValue::Number) ? Value.NumberVal : 0;

		FString Operator = TEXT(">=");
		Ctx.Params->TryGetStringField(TEXT("operator"), Operator);

		// 尝试从变量读资源值
		FSkillVarValue ResVal = Ctx.Instance ? Ctx.Instance->GetVariable(ResourceName) : FSkillVarValue();
		double Current = (ResVal.Type == FSkillVarValue::Number) ? ResVal.NumberVal : 0;

		if (Operator == TEXT(">=")) return Current >= Threshold;
		if (Operator == TEXT("<=")) return Current <= Threshold;
		if (Operator == TEXT(">"))  return Current > Threshold;
		if (Operator == TEXT("<"))  return Current < Threshold;
		if (Operator == TEXT("==")) return Current == Threshold;
		if (Operator == TEXT("!=")) return Current != Threshold;
		return false;
	});

	// input — 输入条件
	// Transition 上的 input 条件语义：如果 button 为空，视为无约束（return true）
	// 如果 button 非空，检查角色当前是否按住了该键
	Subsys.RegisterCondition(FName(TEXT("input")), [](const FSkillConditionContext& Ctx) -> bool
	{
		if (!Ctx.Params.IsValid()) return true;

		FString ButtonName;
		Ctx.Params->TryGetStringField(TEXT("button"), ButtonName);

		// button 为空 → 无约束，通过
		if (ButtonName.IsEmpty()) return true;

		// button 非空 → Phase 1: 检查 SkillInstance 的输入标记
		// （后续 M5 接入 OnInputListener 后，可改为实时查询按键状态）
		if (Ctx.Instance)
		{
			return Ctx.Instance->HasInputFlag(ButtonName);
		}
		return false;
	});

	UE_LOG(LogSkillRuntime, Log, TEXT("Registered built-in conditions"));
}
