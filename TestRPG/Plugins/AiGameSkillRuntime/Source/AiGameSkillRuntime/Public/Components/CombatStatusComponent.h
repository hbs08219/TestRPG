// UCombatStatusComponent — 战斗临时状态组件(回合制能力系统)
//
// 持有战斗期的「非属性」临时状态,与同 Actor 上的 UAttributeComponent 配合:
//   - CombatTags:带回合时效的字符串标签(控制:Stunned/Silenced;门控用)。
//   - DoT 列表:中毒等周期伤害(每回合开始扣固定 HP,可叠层)。
//   - buff/debuff 的数值修正下放给 UAttributeComponent 的 timed modifier(不在此重复存)。
//
// 入口:
//   ApplyEffect(效果组件, 施法者属性, 来源) —— 分发 damage/heal/buff/control/dot。
//   TickTurnStart() —— 角色回合开始时调用:先 DoT 扣血,再递减所有时效(tag/dot/attr modifier)。
//   IsStunned()/IsSilenced() —— 行动门控查询(见 Docs/Battle_Ability_System_Design.md §5.1)。
//
// 设计文档:Docs/Battle_Ability_System_Design.md §4/§5/§5.1/§6。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleEffect.h"
#include "CombatStatusComponent.generated.h"

class UAttributeComponent;

/** 一层 DoT(按来源聚合,叠层) */
USTRUCT()
struct FActiveDot
{
	GENERATED_BODY()

	UPROPERTY() FName Source = NAME_None;
	UPROPERTY() int32 DamagePerTurn = 0;
	UPROPERTY() int32 RemainingTurns = 0;
	UPROPERTY() int32 Stacks = 1;
};

/** 回合开始结算的返回(供 HUD/日志/AI 用) */
USTRUCT(BlueprintType)
struct FCombatTurnTickResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 PoisonDamage = 0;     // 本回合 DoT 总扣血
	UPROPERTY(BlueprintReadOnly) bool bDiedFromPoison = false;
	UPROPERTY(BlueprintReadOnly) bool bStunned = false;      // 递减后仍处于 Stunned(本回合应跳过行动)
	UPROPERTY(BlueprintReadOnly) bool bSilenced = false;     // 递减后仍处于 Silenced(技能不可选)
};

UCLASS(ClassGroup=(Battle), meta=(BlueprintSpawnableComponent))
class AIGAMESKILLRUNTIME_API UCombatStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatStatusComponent();

	virtual void BeginPlay() override;

	/** 显式注入属性组件(测试/手动装配用);不调用则 BeginPlay 自动从 Owner 查找。 */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetAttributeComponent(UAttributeComponent* InAttr) { AttrComp = InAttr; }

	/** 施加一个效果组件到自身(damage/heal 即时;buff/control/dot 进时效状态)。
	 *  @param Effect     效果组件
	 *  @param CasterAttr 施法者属性(用于 Spirit 加成计算;可空)
	 *  @param Source     来源标识(技能 id 等,用于 buff/dot 去重叠层) */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void ApplyEffect(const FEffectComponent& Effect, UAttributeComponent* CasterAttr, FName Source);

	/** 角色回合开始:① DoT 扣血 ② tag/dot/attr-modifier 时效递减、归 0 移除。返回结算信息。 */
	UFUNCTION(BlueprintCallable, Category="Combat")
	FCombatTurnTickResult TickTurnStart();

	// ---- Tag 门控 ----
	UFUNCTION(BlueprintCallable, Category="Combat") void AddTag(FName Tag, int32 Turns);
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Combat") bool HasTag(FName Tag) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Combat") bool IsStunned() const { return HasTag(CombatTag::Stunned); }
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Combat") bool IsSilenced() const { return HasTag(CombatTag::Silenced); }
	/** 能否行动(未被眩晕) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Combat") bool CanAct() const { return !IsStunned(); }
	/** 能否放技能(未被沉默) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Combat") bool CanCastSkill() const { return !IsSilenced(); }

	/** 清空所有临时状态(战斗结束/复位) */
	UFUNCTION(BlueprintCallable, Category="Combat") void ClearAll();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Combat|Debug") int32 GetDotStacks() const;

	/** 汇总当前生效状态为紧凑可读串(供 HUD 显示),如「速↓2 毒3 晕1」。无状态返回空串。
	 *  含:属性 timed modifier(buff/debuff)+ 控制 tag(晕/默)+ DoT(毒)+ 各自剩余回合。 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Combat")
	FString GetStatusSummary() const;

private:
	UAttributeComponent* ResolveAttr();

	UPROPERTY() UAttributeComponent* AttrComp = nullptr;

	/** 控制 tag → 剩余回合数(>0) */
	TMap<FName, int32> TagTurns;

	UPROPERTY()
	TArray<FActiveDot> Dots;
};
