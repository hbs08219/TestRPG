// PlayAnim Action 实现
// 从 TimelineAction 的 tracks 中查找 animation 轨道并播放 AnimSequence
// 如果外部已通过 DoActivateSkill 预融合（bAnimationPreBlended=true），
// 则只设置播放速率，不切换 AnimationMode

#include "PlayAnimAction.h"
#include "SkillInstance.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"

void FPlayAnimAction::Execute(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params)
{
	ACharacter* Char = Instance ? Instance->GetOwnerCharacter() : nullptr;
	if (!Char)
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("PlayAnim: no owner character"));
		return;
	}

	// HD-2D 角色的 2D 动画改由 sprite2d 轨道事件驱动(FSprite2DTrackHandler),此处不再处理。
	// 2D 角色骨骼 mesh 已隐藏,以下骨骼 montage 路径对其为无害 no-op。

	USkeletalMeshComponent* MeshComp = Char->GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("PlayAnim: no mesh component"));
		return;
	}

	const float AnimPlayRate = Instance ? Instance->GetPlayRate() : 1.0f;

	// ── 外部已预融合（DoActivateSkill 做完了 BlendTo），只设置播放速率即可 ──
	if (Instance && Instance->bAnimationPreBlended)
	{
		MeshComp->SetPlayRate(AnimPlayRate);
		UE_LOG(LogSkillRuntime, Log, TEXT("PlayAnim: animation pre-blended, rate=%.2f"), AnimPlayRate);
		return;
	}

	// ── 传统路径：自己查找动画并切换到 SingleNode ──
	FString AnimPath;
	const TSharedPtr<FJsonObject>* ResourceObj;
	if (Params->TryGetObjectField(TEXT("resource"), ResourceObj))
	{
		(*ResourceObj)->TryGetStringField(TEXT("anim"), AnimPath);
	}

	if (AnimPath.IsEmpty())
	{
		const TArray<TSharedPtr<FJsonValue>>* Tracks;
		if (Params->TryGetArrayField(TEXT("tracks"), Tracks))
		{
			for (const auto& TrackVal : *Tracks)
			{
				const TSharedPtr<FJsonObject> Track = TrackVal->AsObject();
				FString TrackType;
				Track->TryGetStringField(TEXT("typeId"), TrackType);
				if (TrackType.IsEmpty()) Track->TryGetStringField(TEXT("trackType"), TrackType);
				if (TrackType != TEXT("animation")) continue;

				const TArray<TSharedPtr<FJsonValue>>* Events;
				if (Track->TryGetArrayField(TEXT("events"), Events))
				{
					for (const auto& EventVal : *Events)
					{
						const TSharedPtr<FJsonObject> Event = EventVal->AsObject();
						const TSharedPtr<FJsonObject>* Props;
						if (Event->TryGetObjectField(TEXT("properties"), Props))
						{
							const TSharedPtr<FJsonObject>* ClipObj;
							if ((*Props)->TryGetObjectField(TEXT("clip"), ClipObj))
							{
								(*ClipObj)->TryGetStringField(TEXT("objectPath"), AnimPath);
								if (!AnimPath.IsEmpty()) break;
							}
						}
					}
				}
				if (!AnimPath.IsEmpty()) break;
			}
		}
	}

	if (AnimPath.IsEmpty())
	{
		UE_LOG(LogSkillRuntime, Verbose, TEXT("PlayAnim: no anim specified, skipping"));
		return;
	}

	FString AssetPath = AnimPath;
	if (!AssetPath.Contains(TEXT(".")))
	{
		int32 LastSlash;
		if (AssetPath.FindLastChar('/', LastSlash))
		{
			AssetPath += TEXT(".") + AssetPath.RightChop(LastSlash + 1);
		}
	}

	UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AssetPath);
	if (!AnimSeq)
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("PlayAnim: failed to load anim: %s"), *AssetPath);
		return;
	}

	MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MeshComp->SetAnimation(AnimSeq);
	MeshComp->SetPosition(0.0f);
	MeshComp->SetPlayRate(AnimPlayRate);
	MeshComp->Play(false);

	UE_LOG(LogSkillRuntime, Log, TEXT("PlayAnim: playing %s at rate %.2f"), *AssetPath, AnimPlayRate);
}
