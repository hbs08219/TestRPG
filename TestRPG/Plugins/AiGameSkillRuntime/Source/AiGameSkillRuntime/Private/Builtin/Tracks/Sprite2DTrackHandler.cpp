// FSprite2DTrackHandler 实现

#include "Sprite2DTrackHandler.h"
#include "SkillInstance.h"
#include "AiGameSkillRuntime.h"
#include "Interfaces/ISkillAnim2DReceiver.h"
#include "GameFramework/Character.h"

static ISkillAnim2DReceiver* FindReceiver(ACharacter* Char)
{
	if (!Char) return nullptr;
	TInlineComponentArray<UActorComponent*> Comps(Char);
	for (UActorComponent* C : Comps)
	{
		if (ISkillAnim2DReceiver* R = Cast<ISkillAnim2DReceiver>(C))
		{
			return R;
		}
	}
	return nullptr;
}

void FSprite2DTrackHandler::OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	ACharacter* Char = Instance ? Instance->GetOwnerCharacter() : nullptr;
	if (!Char || !EventParams.IsValid()) return;

	FString Clip;
	EventParams->TryGetStringField(TEXT("clip2d"), Clip);
	if (Clip.IsEmpty()) return;

	ISkillAnim2DReceiver* R = FindReceiver(Char);
	if (R && R->SkillAnim_Is2D())
	{
		R->SkillAnim_PlayState(Clip);
		UE_LOG(LogSkillRuntime, Log, TEXT("Sprite2DTrack: play clip2d=%s @frame %d"), *Clip, Frame);
	}
}

void FSprite2DTrackHandler::OnExit(USkillInstance* /*Instance*/, const TSharedPtr<FJsonObject>& /*EventParams*/, int32 /*Frame*/)
{
	// 事件结束暂不强制回 idle:由战斗/定位状态机后续状态切换驱动
}
