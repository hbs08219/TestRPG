// HitBox Track Handler — DrawDebugSphere + SphereOverlapActors 打印日志

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ISkillTrackHandler.h"

class FHitBoxTrackHandler : public ISkillTrackHandler
{
public:
	virtual FName GetTrackType() const override { return FName(TEXT("HitBox")); }
	virtual void OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;
	virtual void OnTick(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame) override;

private:
	void DrawDebug(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params, int32 Frame, bool bActive);
	void DoOverlapTest(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params);
};
