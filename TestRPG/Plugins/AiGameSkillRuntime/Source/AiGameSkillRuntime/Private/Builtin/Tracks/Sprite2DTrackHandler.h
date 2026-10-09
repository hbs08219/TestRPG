// Sprite2D Track Handler — HD-2D 序列帧轨道
//
// 技能时间轴上的 sprite2d 轨道事件,进入时让角色 2D 精灵播放指定 clip 状态(clip2d)。
// 通过 ISkillAnim2DReceiver 接口跨模块调用 UAppearanceComponent(避免 SkillRuntime→AiGame 依赖)。

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ISkillTrackHandler.h"

class FSprite2DTrackHandler : public ISkillTrackHandler
{
public:
	virtual FName GetTrackType() const override { return FName(TEXT("sprite2d")); }
	virtual void OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
};
