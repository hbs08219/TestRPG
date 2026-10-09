// UBattleAction — 战斗三层架构：Action 层（Layer 3）
//
// 设计文档 §13：StateMachine → Flow → Action 三层。
// Action 是"具体的表现与逻辑"执行单元，由 Flow 按队列驱动。
//
// 与技能系统 Action 的差异：技能 Action 是无状态、单帧 Execute(JSON)（走 Timeline）；
// 战斗 Action 有持续时间（移动/攻击/返回各占若干秒），需要 Tick 推进，故单独定义本基类。
//
// P0：Move/Return 用计时占位（不真位移）；Phase 2 在各 Action 的 Tick 内接入真实位移/动画。

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Math/Vector.h"
#include "BattleCommand.h"
#include "SkillEffectResolver.h"   // FSkillEffectPlan / FSkillTargetContext
#include "BattleAction.generated.h"

class UBattleStateMachine;
class ACharacter;

// ============================================
// UBattleAction — 基类
// ============================================
UCLASS(Abstract)
class AIGAMESKILLRUNTIME_API UBattleAction : public UObject
{
	GENERATED_BODY()

public:
	/** 绑定战斗状态机上下文 */
	void InitializeAction(UBattleStateMachine* InSM);

	/** 进入该 Action（Flow 激活队列中此 Action 时调用一次） */
	virtual void Enter() {}

	/** 每帧推进；完成时调用 FinishAction() */
	virtual void Tick(float DeltaSeconds) {}

	/** 是否已完成 */
	bool IsActionFinished() const { return bActionFinished; }

	virtual FString GetActionName() const { return TEXT("Action"); }

protected:
	void FinishAction() { bActionFinished = true; }

	UPROPERTY()
	UBattleStateMachine* SM = nullptr;

	bool bActionFinished = false;
};

// ============================================
// UBattleAction_MoveToTarget — 移动到目标面前
// P0：计时占位；Phase 2 在 Tick 内 lerp Mover 到 Target 前方
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleAction_MoveToTarget : public UBattleAction
{
	GENERATED_BODY()
public:
	void Setup(ACharacter* InMover, ACharacter* InTarget, float InDuration);
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetActionName() const override { return TEXT("MoveToTarget"); }

private:
	TWeakObjectPtr<ACharacter> Mover;
	TWeakObjectPtr<ACharacter> Target;
	float Duration = 0.6f;
	float Elapsed = 0.f;
};

// ============================================
// UBattleAction_ExecuteHit — 攻击命中结算
// 蓄力 WindupDuration 后结算伤害（普攻/技能），玩家与敌人共用此 Action
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleAction_ExecuteHit : public UBattleAction
{
	GENERATED_BODY()
public:
	/** @param bInDefenderDefending 受击方本回合是否处于防御（减伤 ×0.5） */
	void Setup(ACharacter* InAttacker, ACharacter* InDefender,
		const FBattleCommandPayload& InCommand, bool bInDefenderDefending, float InWindupDuration);
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetActionName() const override { return TEXT("ExecuteHit"); }

private:
	void ResolveDamage();

	TWeakObjectPtr<ACharacter> Attacker;
	TWeakObjectPtr<ACharacter> Defender;
	FBattleCommandPayload Command;
	bool bDefenderDefending = false;
	float WindupDuration = 0.4f;
	float Elapsed = 0.f;
	bool bResolved = false;
};

// ============================================
// UBattleAction_CastSkill — 技能施法(动画 + 节奏 + 效果结算)
// 施法者播 CastClip 动画;蓄力 WindupDuration 帧调 FSkillEffectResolver 结算效果;
// TotalDuration 结束后切回 idle 动画。取代「技能瞬间结算」,给放技能加上演出。
// 注:回合制不位移,施法在原地;动画走 ISkillAnim2DReceiver 接口(2D 角色)。
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleAction_CastSkill : public UBattleAction
{
	GENERATED_BODY()
public:
	void Setup(ACharacter* InCaster, const FSkillEffectPlan& InPlan, const FSkillTargetContext& InCtx,
		const FString& InCastClip, float InWindupDuration, float InTotalDuration);
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetActionName() const override { return TEXT("CastSkill"); }

private:
	TWeakObjectPtr<ACharacter> Caster;
	FSkillEffectPlan Plan;
	FSkillTargetContext Ctx;
	FString CastClip = TEXT("attack");
	float WindupDuration = 0.4f;
	float TotalDuration = 0.9f;
	float Elapsed = 0.f;
	bool bResolved = false;
};
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleAction_ReturnHome : public UBattleAction
{
	GENERATED_BODY()
public:
	void Setup(ACharacter* InMover, FVector InHomeLocation, float InDuration);
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetActionName() const override { return TEXT("ReturnHome"); }

private:
	TWeakObjectPtr<ACharacter> Mover;
	FVector HomeLocation = FVector::ZeroVector;
	float Duration = 0.6f;
	float Elapsed = 0.f;
};
