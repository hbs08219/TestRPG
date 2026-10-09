// ISkillTrackHandler — 时间轴轨道事件的处理接口
// Range 事件走 OnEnter/OnExit/OnTick；Point 事件走 OnEnter（fire-and-forget）

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class USkillInstance;

class ISkillTrackHandler
{
public:
	virtual ~ISkillTrackHandler() = default;

	/** 轨道类型标识（如 "HitBox", "VFX", "SFX"） */
	virtual FName GetTrackType() const = 0;

	/** Range 事件：进入激活区域 / Point 事件：触发一次 */
	virtual void OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) = 0;

	/** Range 事件：离开激活区域 */
	virtual void OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) = 0;

	/** Range 事件：每帧 tick（可选，默认空实现） */
	virtual void OnTick(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) {}

	/** 技能结束 / TrackHandler 被销毁前调用，用于清理残留资源（VFX/SFX 组件等） */
	virtual void Cleanup() {}
};
