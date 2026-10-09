// VFX Track Handler — 在 Runtime 中 spawn/destroy Niagara 特效

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ISkillTrackHandler.h"

class FVFXTrackHandler : public ISkillTrackHandler
{
public:
	virtual FName GetTrackType() const override { return FName(TEXT("vfx")); }
	virtual void OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void OnTick(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void Cleanup() override;

private:
	/** 当前活跃的 Niagara 组件（事件 id → 弱引用） */
	TMap<FString, TWeakObjectPtr<class UNiagaraComponent>> ActiveVFX;

	/** 生成 VFX */
	void SpawnVFX(USkillInstance* Instance, const FString& EventId, const FString& ObjectPath, const FString& SocketName);

	/** 销毁 VFX */
	void DestroyVFX(const FString& EventId);
};
