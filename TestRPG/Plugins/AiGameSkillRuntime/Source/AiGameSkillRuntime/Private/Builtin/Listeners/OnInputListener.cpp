// OnInput Listener 实现
// 不直接绑定 UE Input 系统，而是依赖外部把按键标记写到 USkillInstance 的输入 Flag。
// UCustomInputSubsystem 在派发 Action 时，会调用 SkillComponent::SetActiveSkillInputFlag。
// 这里只做：在订阅时记录 button 名，等待外部触发时由 SkillInstance 主动调用 OnTrigger。

#include "OnInputListener.h"
#include "SkillInstance.h"
#include "AiGameSkillRuntime.h"

void FOnInputListener::Subscribe(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params, FOnListenerTriggered OnTrigger)
{
	CachedTrigger = MoveTemp(OnTrigger);

	if (!Instance || !Params.IsValid()) return;

	Params->TryGetStringField(TEXT("button"), BoundButtonName);
	if (BoundButtonName.IsEmpty())
	{
		UE_LOG(LogSkillRuntime, Verbose, TEXT("OnInput: no button specified"));
		return;
	}

	UE_LOG(LogSkillRuntime, Verbose, TEXT("OnInput: subscribed for button %s"), *BoundButtonName);

	// TODO（Phase 2）: 与 UCustomInputSubsystem 直接对接，
	// 由 Subsystem 在按键按下时主动调用 CachedTrigger.ExecuteIfBound()。
	// 当前 Phase: 通过 USkillInstance::CheckInputFlag + Condition 间接判定。
}

void FOnInputListener::Unsubscribe()
{
	CachedTrigger.Unbind();
	BoundButtonName.Reset();
}
