// FBattleDamageCalculator — 战斗伤害/治疗计算（静态工具类）
//
// P0 公式定义（与 Battle_System_P0_Plan.md 对齐）：
//   普攻伤害 = max(1, ATK - DEF * 0.5)
//   技能伤害 = max(1, SkillPower + Spirit * 0.8)        // 忽略防御
//   治疗量   = max(0, SkillHealAmount + Spirit * 0.5)
//   防御减伤 = 上回合选了防御的目标，受到伤害 *0.5
//
// 属性约定（FName 常量见 .cpp）：
//   HP / MaxHP / MP / MaxMP / ATK / DEF / SPD / LUK / Spirit
//
// 接入点：UBattleFlow_PlayerAction / UBattleFlow_EnemyAction 的 Attacking SubState

#pragma once

#include "CoreMinimal.h"

class UAttributeComponent;

namespace BattleAttr
{
	// 战斗系统使用的标准属性名（在 .cpp 中静态初始化，避免重复构造 FName）
	AIGAMESKILLRUNTIME_API extern const FName HP;
	AIGAMESKILLRUNTIME_API extern const FName MaxHP;
	AIGAMESKILLRUNTIME_API extern const FName MP;
	AIGAMESKILLRUNTIME_API extern const FName MaxMP;
	AIGAMESKILLRUNTIME_API extern const FName ATK;
	AIGAMESKILLRUNTIME_API extern const FName DEF;
	AIGAMESKILLRUNTIME_API extern const FName SPD;
	AIGAMESKILLRUNTIME_API extern const FName LUK;
	AIGAMESKILLRUNTIME_API extern const FName Spirit;
}

/**
 * 伤害/治疗计算器（纯静态，不持有状态）。
 * 公式实现严格遵循 P0 计划。后续如需扩展暴击/属性相克，
 * 在此类内追加方法或结构体返回值，外部接入点不变。
 */
class AIGAMESKILLRUNTIME_API FBattleDamageCalculator
{
public:
	/** 普攻伤害：max(1, ATK - DEF*0.5) */
	static int32 CalcMeleeDamage(const UAttributeComponent* AttackerAttr, const UAttributeComponent* DefenderAttr);

	/** 技能伤害：max(1, SkillPower + Spirit*0.8)（不计防御） */
	static int32 CalcSkillDamage(int32 SkillPower, const UAttributeComponent* CasterAttr);

	/** 治疗量：max(0, SkillHealAmount + Spirit*0.5) */
	static int32 CalcHealing(int32 SkillHealAmount, const UAttributeComponent* CasterAttr);

	/** 防御中减伤：bDefending=true 时伤害 ×0.5 */
	static int32 ApplyDefendingMod(int32 IncomingDamage, bool bDefenderIsDefending);

	/** 实际扣 HP（不会扣到负数）。返回真实造成的伤害（受当前血量限制）。 */
	static int32 ApplyDamage(UAttributeComponent* DefenderAttr, int32 Damage);

	/** 实际加 HP（不会超过 MaxHP）。返回真实回血量。 */
	static int32 ApplyHealing(UAttributeComponent* TargetAttr, int32 HealAmount);

	/** 工具：判断目标是否已死亡（HP<=0）。 */
	static bool IsDead(const UAttributeComponent* TargetAttr);
};
