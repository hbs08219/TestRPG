// USkillFlow — 三层架构：Flow 层（Layer 2）
// 每个节点对应一个 Flow，封装该节点的执行逻辑
// StateMachine 驱动 Flow.Tick()，Flow 完成后触发 Transition

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkillDataTypes.h"

// 前向声明，避免与 SkillStateMachine.h 循环依赖
class USkillStateMachine;

#include "SkillFlow.generated.h"

class USkillStateMachine;
class USkillInstance;

// ============================================
// 委托
// ============================================
DECLARE_MULTICAST_DELEGATE(FOnFlowFinished);

// ============================================
// USkillFlow — 基类
// ============================================
UCLASS(Abstract, Blueprintable, BlueprintType)
class AIGAMESKILLRUNTIME_API USkillFlow : public UObject
{
	GENERATED_BODY()

public:
	/** 初始化：绑定 StateMachine 和当前节点数据 */
	void Initialize(USkillStateMachine* InOwnerSM, const FSkillDataNode& InNodeData);

	/** 进入 Flow（StateMachine 激活此 Flow 时调用）*/
	virtual void Enter();

	/** Tick：由 StateMachine 每帧驱动 */
	virtual void Tick(float DeltaSeconds);

	/** 退出 Flow */
	virtual void Exit();

	/** Flow 是否已完成？ */
	bool IsFinished() const { return bFinished; }

	/** 获取当前节点 ID */
	const FString& GetNodeId() const { return NodeId; }

	/** 完成委托（StateMachine 监听此委托以触发 Transition）*/
	FOnFlowFinished OnFinished;

protected:
	/** 标记 Flow 完成 → 触发 OnFinished 委托 */
	void MarkFinished();

	/** 快捷访问 */
	USkillStateMachine* GetSM() const { return OwnerSM; }
	USkillInstance* GetInstance() const;

	/** 节点数据 */
	FSkillDataNode NodeData;
	FString NodeId;

private:
	UPROPERTY() USkillStateMachine* OwnerSM = nullptr;
	bool bFinished = false;
};

// ============================================
// 流程节点 Flow（Entry / Exit / Sequence / Conditional / Random / Loop）
// ============================================

UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Entry : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;  // 立即走 "out" 分支
};

UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Exit : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;  // 立即标记 Skill 结束
};

UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Sequence : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
};

UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Conditional : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;  // 评估条件，立即走满足条件的分支
};

UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Random : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;  // 按权重随机选分支，立即 Transition
};

UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Loop : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
};

// ============================================
// 行为节点 Flow（Action 节点）
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Action : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Exit() override;

private:
	/** 初始化 Timeline 参数 */
	void InitializeTimeline(const TSharedPtr<FJsonObject>& ActionData);

	/** 执行非 Timeline 立即行动 */
	void ExecuteImmediateAction(const TSharedPtr<FJsonObject>& ActionData);

	/** 处理 Timeline 推进 */
	void AdvanceTimeline(float DeltaSeconds);

	/** 处理 Timeline 事件 */
	void ProcessTimelineEvents();

	/** 时间轴参数（从节点 data.tracks 解析）*/
	int32 TotalFrames = 30;
	float FrameRate = 30.0f;
	float PlayRate = 1.0f;
	float AccumulatedTime = 0.0f;
	int32 CurrentFrame = 0;

	/** 当前活跃 Range 事件集合 */
	TSet<FString> ActiveRangeEvents;

	/** TrackHandler 缓存 */
	TMap<FName, TSharedPtr<class ISkillTrackHandler>> TrackHandlerCache;

	/** 非 Timeline 行为是否已完成 */
	bool bActionDone = false;
};

// ============================================
// 监听节点 Flow（Listener 节点）
// ============================================
UCLASS()
class AIGAMESKILLRUNTIME_API USkillFlow_Listener : public USkillFlow
{
	GENERATED_BODY()
public:
	virtual void Enter() override;
	virtual void Exit() override;

private:
	TArray<TSharedPtr<class ISkillListener>> ActiveListeners;
};
