// USkillStateMachine — 三层架构：状态机层（Layer 1）
// 负责管理 Flow 切换、Tick 驱动、Transition 逻辑
// USkillInstance 组合此类，而非继承

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkillDataTypes.h"
#include "SkillStateMachine.generated.h"

class USkillFlow;
class USkillInstance;
class ACharacter;

UCLASS()
class AIGAMESKILLRUNTIME_API USkillStateMachine : public UObject
{
	GENERATED_BODY()

public:
	/** 初始化 */
	void Initialize(USkillInstance* InInstance);

	/** 每帧驱动 */
	void Tick(float DeltaSeconds);

	/** 手动终止 */
	void Terminate();

	// ============================================
	// 状态查询
	// ============================================

	bool IsFinished() const { return bFinished; }
	bool IsRunning() const { return !bFinished && CurrentFlow != nullptr; }

	USkillInstance* GetInstance() const { return Instance.Get(); }

	// ============================================
	// Flow 切换
	// ============================================

	/** 进入指定节点（通过 NodeId 查找，创建对应 Flow） */
	void EnterNode(const FString& NodeId);

	/** 当前 Flow 结束后，按 Transition 规则切换到下一个节点 */
	void TryTransitions(const FString& NodeId, const FString& BranchId = TEXT("out"));

	// ============================================
	// 条件评估（供 Flow 调用）
	// ============================================

	bool EvaluateConditionGroup(const TSharedPtr<FJsonObject>& ConditionGroup) const;
	bool EvaluateSingleCondition(const TSharedPtr<FJsonObject>& Condition) const;

private:
	/** 创建对应 Node 的 Flow 对象 */
	USkillFlow* CreateFlowForNode(const FSkillDataNode& Node);

	/** 当前 Flow 结束回调 */
	UFUNCTION() void OnFlowFinished();

	/** 获取 World（通过 OwnerCharacter） */
	UWorld* GetWorld() const;

	UPROPERTY() USkillFlow* CurrentFlow = nullptr;
	TWeakObjectPtr<USkillInstance> Instance;
	bool bFinished = false;
};
