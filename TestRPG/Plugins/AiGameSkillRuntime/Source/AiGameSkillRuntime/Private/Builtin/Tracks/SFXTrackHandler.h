// SFX Track Handler — 在 Runtime 中播放/停止音效

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ISkillTrackHandler.h"

class FSFXTrackHandler : public ISkillTrackHandler
{
public:
	virtual FName GetTrackType() const override { return FName(TEXT("sfx")); }
	virtual void OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void OnTick(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void Cleanup() override;

private:
	/** 当前活跃的 Audio 组件（事件 id → 弱引用） */
	TMap<FString, TWeakObjectPtr<class UAudioComponent>> ActiveSFX;
};
