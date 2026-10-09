// FSkillEffectResolver — 技能执行器(回合制能力系统 阶段3)
//
// 取代写死的「ExecuteHit 单体 CalcSkillDamage」:
//   读技能 JSON 的逻辑页时间线(TimelineAction 节点的 hittest/effect 轨道),
//   按 scope×faction 解析命中目标集,对每个目标施加 FEffectComponent(走 Phase2 的
//   UCombatStatusComponent::ApplyEffect)。判定与效果靠 hitId/targetRef 关联。
//
// 数据契约:Docs/Battle_Ability_System_Design.md §3/§4/§7。
// 仅做逻辑结算(turn-based);动画/位移/特效的逐帧演出留作后续 polish。

#pragma once

#include "CoreMinimal.h"
#include "BattleEffect.h"

class ACharacter;
class UAttributeComponent;
class UCombatStatusComponent;
class UBattleStateMachine;

/** 一条判定(hittest 轨道事件) */
struct FResolvedHitTest
{
	FString HitId;
	FString Scope;     // single / all / row
	FString Faction;   // enemy / ally / self
};

/** 一条效果(effect 轨道事件)= targetRef + 一个效果组件 */
struct FResolvedEffectEvent
{
	FString TargetRef;
	FEffectComponent Effect;
};

/** 解析后的技能效果计划(从 JSON 提取一次,可复用) */
struct FSkillEffectPlan
{
	FString Name;
	FString TargetType;             // battle.targetType(供 HUD/门控)
	int32   MpCost = 0;
	bool    bHasBattle = false;
	TArray<FString> RequireTags;    // 需全部具备才可释放
	TArray<FString> BlockTags;      // 具备任一则禁用
	FString CastClip;               // 施法动画 clip(逻辑页 sprite2d 轨第一条 clip2d;缺省走 "attack")
	TArray<FResolvedHitTest> HitTests;
	TArray<FResolvedEffectEvent> Effects;

	/** 有可施加的效果才算有效(否则技能无逻辑产出) */
	bool IsValid() const { return Effects.Num() > 0; }
};

/** 施法上下文:谁放、选了谁、敌我两侧(相对施法者) */
struct FSkillTargetContext
{
	ACharacter* Caster = nullptr;
	ACharacter* SelectedTarget = nullptr;   // HUD 预选目标(single 锚点;可空)
	TArray<ACharacter*> Enemies;            // 敌方(相对施法者)
	TArray<ACharacter*> Allies;             // 友方(含施法者)
};

class AIGAMESKILLRUNTIME_API FSkillEffectResolver
{
public:
	/** 从 Content/Data/ToolGen/Skill/<path>.json 解析效果计划。失败/无效返回 false。 */
	static bool LoadPlan(const FString& SkillPath, FSkillEffectPlan& Out);

	/** 按计划解析目标集并施加所有效果。SM 非空时对伤害广播 OnBattleHit(驱动屏幕提示/死亡判定)。 */
	static void ResolveAndApply(const FSkillEffectPlan& Plan, const FSkillTargetContext& Ctx, UBattleStateMachine* SM);

	/** 取(无则在该 Actor 上动态创建并注册)CombatStatus 组件。回合 tick / 施加效果共用。 */
	static UCombatStatusComponent* GetOrCreateStatus(ACharacter* Char);

	/** 门控判定:沉默禁全部 → MP 足 → requireTags 全具备 → blockTags 全不具备。
	 *  CasterStatus 可空(空则视为无 tag/未沉默)。不可放时 OutReason 给中文原因。 */
	static bool CanCast(const FSkillEffectPlan& Plan, const UCombatStatusComponent* CasterStatus, float CurMP, FString& OutReason);

private:
	/** 按一条判定解析命中目标集(已过滤存活) */
	static TArray<ACharacter*> ResolveTargets(const FResolvedHitTest& Hit, const FSkillTargetContext& Ctx);
};
