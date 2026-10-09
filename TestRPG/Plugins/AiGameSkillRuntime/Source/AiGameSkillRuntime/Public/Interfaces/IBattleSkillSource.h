// IBattleSkillSource — 战斗技能来源接口
//
// 由 AiGame 的战斗角色(ABattleEnemyCharacter 等)实现。战斗 runtime(AiGameSkillRuntime)
// 通过此接口拿到角色的可用技能 path 列表,在不依赖游戏模块的前提下让敌人 AI 选技能。
// 避免 AiGameSkillRuntime → AiGame 循环依赖(同 ISkillAnim2DReceiver 模式)。

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IBattleSkillSource.generated.h"

UINTERFACE(MinimalAPI)
class UBattleSkillSource : public UInterface
{
	GENERATED_BODY()
};

class IBattleSkillSource
{
	GENERATED_BODY()

public:
	/** 该角色在战斗中可用的技能 path 列表(相对 Content/Data/ToolGen/Skill) */
	virtual TArray<FString> GetBattleSkillPaths() const = 0;
};
