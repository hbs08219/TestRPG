// FBattleEnemyAI — 最简敌人 AI（P0：随机选目标 + 随机普攻/技能）
//
// 设计：纯静态函数，不持有状态。后续可扩展为策略类（aggressive / defensive / boss-pattern）。

#pragma once

#include "CoreMinimal.h"
#include "BattleCommand.h"

class UBattleStateMachine;
class ACharacter;

class AIGAMESKILLRUNTIME_API FBattleEnemyAI
{
public:
	/** 决策：返回敌人本回合要执行的指令。
	 *  P0 策略（设计§6）：目标=玩家，50% 普攻 / 50% 技能（技能用占位威力，不耗 MP）。 */
	static FBattleCommandPayload DecideAction(UBattleStateMachine* SM, ACharacter* Enemy);
};
