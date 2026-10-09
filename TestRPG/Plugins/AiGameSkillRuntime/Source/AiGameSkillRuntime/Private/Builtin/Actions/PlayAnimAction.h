// PlayAnim Action — 播放 AnimSequence（Montage）
// 从行为节点的 data JSON 中读 resource.anim 字段

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ISkillAction.h"

class FPlayAnimAction : public ISkillAction
{
public:
	virtual void Execute(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params) override;
	virtual FName GetActionType() const override { return FName(TEXT("PlayAnim")); }
};
