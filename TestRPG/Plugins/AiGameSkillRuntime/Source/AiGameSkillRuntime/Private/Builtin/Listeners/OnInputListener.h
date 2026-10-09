// OnInput Listener — 监听按键事件
// 实际监听通过 USkillInstance::SetInputFlag 由 UCustomInputSubsystem 推送

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ISkillListener.h"

class FOnInputListener : public ISkillListener
{
public:
	virtual FName GetListenerType() const override { return FName(TEXT("OnInput")); }
	virtual void Subscribe(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params, FOnListenerTriggered OnTrigger) override;
	virtual void Unsubscribe() override;

private:
	FOnListenerTriggered CachedTrigger;
	FString BoundButtonName;
};
