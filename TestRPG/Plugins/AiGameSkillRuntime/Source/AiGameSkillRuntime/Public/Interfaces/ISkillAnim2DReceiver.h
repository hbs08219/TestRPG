// ISkillAnim2DReceiver — HD-2D 角色动画接收接口
//
// 由 AiGame 的 UAppearanceComponent 实现。技能运行时(AiGameSkillRuntime)通过此接口,
// 在不依赖游戏模块的前提下让 2D 精灵播放技能 clip(state × 当前朝向)。
// 避免 AiGameSkillRuntime → AiGame 的循环依赖(AiGame 已依赖 AiGameSkillRuntime)。

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISkillAnim2DReceiver.generated.h"

UINTERFACE(MinimalAPI)
class USkillAnim2DReceiver : public UInterface
{
	GENERATED_BODY()
};

class ISkillAnim2DReceiver
{
	GENERATED_BODY()

public:
	/** 是否处于 HD-2D 渲染模式(true 时技能走 2D clip,而非骨骼 montage) */
	virtual bool SkillAnim_Is2D() const = 0;

	/** 按状态名播放 2D clip;朝向由实现方维护的"当前朝向"决定 */
	virtual void SkillAnim_PlayState(const FString& State) = 0;

	/** 当前正在播的 clip 一个周期时长(秒);无/非2D 返回 0。供施法演出按 clip 真实长度对齐一次播放。 */
	virtual float SkillAnim_GetCurrentClipDurationSeconds() const = 0;
};
