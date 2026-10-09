// SetVariable Action — 修改 Flow 变量
// 支持 =, +=, -=, *=, toggle 运算符

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ISkillAction.h"

class FSetVariableAction : public ISkillAction
{
public:
	virtual void Execute(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params) override;
	virtual FName GetActionType() const override { return FName(TEXT("SetVariable")); }
};
