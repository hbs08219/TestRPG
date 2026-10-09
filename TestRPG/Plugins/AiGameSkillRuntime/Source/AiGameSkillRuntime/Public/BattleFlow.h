// UBattleFlow — 战斗三层架构：Flow 层（Layer 2）
// 每个战斗阶段对应一个 Flow，封装该阶段的执行逻辑

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleCommand.h"

// 前向声明，避免与 BattleStateMachine.h 循环依赖
class UBattleStateMachine;
class UBattleAction;
class ACharacter;

#include "BattleFlow.generated.h"

// ============================================
// 委托
// ============================================
DECLARE_MULTICAST_DELEGATE(FOnBattleFlowFinished);

// ============================================
// UBattleFlow — 基类
// ============================================
UCLASS(Abstract, Blueprintable, BlueprintType)
class AIGAMESKILLRUNTIME_API UBattleFlow : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UBattleStateMachine* InOwnerSM);
	virtual void Enter();
	virtual void Tick(float DeltaSeconds);
	virtual void Exit();

	bool IsFinished() const { return bFinished; }

	virtual FString GetFlowName() const { return TEXT("Unknown"); }
	virtual FString GetNextStateName() const { return TEXT(""); }

	FOnBattleFlowFinished OnFinished;

protected:
	void MarkFinished();
	UBattleStateMachine* GetOwnerSM() const { return OwnerSM; }

	// ----- Action 队列运行器（Layer 3 驱动）-----
	/** 设置并启动一串 Action（重置游标并 Enter 第一个） */
	void StartActions(const TArray<UBattleAction*>& Actions);
	/** 推进当前 Action；当前完成则切下一个；全部完成返回 true */
	bool TickActions(float DeltaSeconds);
	/** 当前是否还有未跑完的 Action 队列 */
	bool HasActions() const;

	UPROPERTY()
	UBattleStateMachine* OwnerSM = nullptr;

	UPROPERTY()
	TArray<UBattleAction*> ActionQueue;

	int32 ActionCursor = -1;

	bool bFinished = false;
};

// ============================================
// UBattleFlow_Idle — 战斗开局
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleFlow_Idle : public UBattleFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual FString GetFlowName() const override { return TEXT("Idle"); }
	virtual FString GetNextStateName() const override { return TEXT("PlayerAction"); }
};

// ============================================
// UBattleFlow_PlayerAction — 玩家行动
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleFlow_PlayerAction : public UBattleFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetFlowName() const override { return TEXT("PlayerAction"); }
	virtual FString GetNextStateName() const override { return NextState; }

private:
	/** 收到玩家指令后解算本回合(消费 pending;即时指令直接结算,攻击/技能建 Action 队列) */
	void ResolveCommand();

	FBattleCommandPayload Command;
	TWeakObjectPtr<ACharacter> TargetEnemy;

	/** 攻击/技能走 Action 队列；防御/物品/逃跑为即时指令（Enter 内直接结算并结束） */
	bool bUsingActionQueue = false;

	/** 下一状态：默认 EnemyAction，逃跑成功 → "Finished" */
	FString NextState = TEXT("EnemyAction");
};

// ============================================
// UBattleFlow_EnemyAction — 敌人行动
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleFlow_EnemyAction : public UBattleFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetFlowName() const override { return TEXT("EnemyAction"); }
	virtual FString GetNextStateName() const override { return TEXT("PlayerAction"); }

private:
	/** 为下一个存活敌人构建 [Move,Hit,Return] 队列；无更多敌人则结束本 Flow */
	void StepNextEnemy();

	int32 CurrentEnemyIdx = -1;
	TWeakObjectPtr<ACharacter> CurrentEnemy;
	FBattleCommandPayload CurrentCommand;
};

// ============================================
// 战斗结束 Flow
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API UBattleFlow_Victory : public UBattleFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetFlowName() const override { return TEXT("Victory"); }
	virtual FString GetNextStateName() const override { return TEXT("Reward"); }
private:
	float Timer = 0.f;
};

UCLASS()
class AIGAMESKILLRUNTIME_API UBattleFlow_Defeat : public UBattleFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetFlowName() const override { return TEXT("Defeat"); }
	virtual FString GetNextStateName() const override { return TEXT("Finished"); }
private:
	float Timer = 0.f;
};

UCLASS()
class AIGAMESKILLRUNTIME_API UBattleFlow_Reward : public UBattleFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString GetFlowName() const override { return TEXT("Reward"); }
	virtual FString GetNextStateName() const override { return TEXT("Finished"); }
private:
	float Timer = 0.f;
};
