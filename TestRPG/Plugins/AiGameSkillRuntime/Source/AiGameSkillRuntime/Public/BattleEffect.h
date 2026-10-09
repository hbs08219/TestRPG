// FEffectComponent — 效果组件(回合制能力系统)
//
// TS 侧 EffectComponent 判别联合的 C++ 镜像(扁平化:type 枚举 + 各类型字段)。
// 来源:技能逻辑页「效果轨道」事件的 properties(见 Docs/Battle_Ability_System_Design.md §4)。
//   damage  : Power
//   heal    : Amount
//   buff    : Attr + bPercent(flat/percent) + Value + Duration + MaxStacks   (减速=Attr:SPD,Value负)
//   control : Control(stun→Stunned / silence→Silenced) + Duration
//   dot     : DamagePerTurn + Duration + MaxStacks   (poison)
//
// 应用由 UCombatStatusComponent::ApplyEffect 分发。解析由 FromJsonProps(供阶段3技能执行器用)。

#pragma once

#include "CoreMinimal.h"
#include "BattleEffect.generated.h"

class FJsonObject;

UENUM(BlueprintType)
enum class EEffectType : uint8
{
	Damage   UMETA(DisplayName="伤害"),
	Heal     UMETA(DisplayName="治疗"),
	Buff     UMETA(DisplayName="Buff/Debuff"),
	Control  UMETA(DisplayName="控制"),
	Dot      UMETA(DisplayName="持续伤害"),
};

/** 控制 tag 名(施加到 CombatStatus 的 CombatTags) */
namespace CombatTag
{
	AIGAMESKILLRUNTIME_API extern const FName Stunned;   // 眩晕:不能行动
	AIGAMESKILLRUNTIME_API extern const FName Silenced;  // 沉默:不能放技能
}

/** 单个效果组件(一个效果轨道事件=一个 FEffectComponent) */
USTRUCT(BlueprintType)
struct AIGAMESKILLRUNTIME_API FEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) EEffectType Type = EEffectType::Damage;

	// —— damage ——
	UPROPERTY(BlueprintReadWrite) int32 Power = 0;
	// —— heal ——
	UPROPERTY(BlueprintReadWrite) int32 Amount = 0;
	// —— buff ——
	UPROPERTY(BlueprintReadWrite) FName Attr = NAME_None;       // ATK/DEF/MATK/MDEF/SPD/LUK
	UPROPERTY(BlueprintReadWrite) bool bPercent = false;        // false=flat(加减固定值) true=percent(百分比)
	UPROPERTY(BlueprintReadWrite) float Value = 0.f;            // 正=增益 负=减益
	// —— control ——
	UPROPERTY(BlueprintReadWrite) FName Control = NAME_None;    // CombatTag::Stunned / Silenced
	// —— dot ——
	UPROPERTY(BlueprintReadWrite) int32 DamagePerTurn = 0;
	// —— 通用(buff/control/dot)——
	UPROPERTY(BlueprintReadWrite) int32 Duration = 0;           // 持续回合数
	UPROPERTY(BlueprintReadWrite) int32 MaxStacks = 1;          // 叠加上限(≥1)

	/** 从效果轨道事件的 properties JSON 解析一个效果组件(阶段3技能执行器用)。
	 *  无法识别 type 时返回 false。 */
	static bool FromJsonProps(const TSharedPtr<FJsonObject>& Props, FEffectComponent& Out);
};
