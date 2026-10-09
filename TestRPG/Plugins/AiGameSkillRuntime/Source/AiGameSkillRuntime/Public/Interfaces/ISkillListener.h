// ISkillListener — 监听节点接口
// 订阅游戏事件（OnInput / OnHit / OnEvent），触发时回调

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Delegates/Delegate.h"

class USkillInstance;

/** 监听触发的回调 */
DECLARE_DELEGATE(FOnListenerTriggered);

class ISkillListener
{
public:
	virtual ~ISkillListener() = default;

	/** 监听类型标识（如 "OnInput", "OnHit", "OnSkillEnd"） */
	virtual FName GetListenerType() const = 0;

	/** 订阅事件。当事件触发时调用 OnTrigger */
	virtual void Subscribe(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params, FOnListenerTriggered OnTrigger) = 0;

	/** 取消订阅（技能结束 / 节点离开时调用） */
	virtual void Unsubscribe() = 0;
};
