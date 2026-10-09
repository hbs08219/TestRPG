// USkillInstance — 技能实例（改造后）
// 组合 USkillStateMachine（Layer 1），自身只负责数据容器 + 委托调用

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkillDataTypes.h"
#include "SkillStateMachine.h"
#include "Interfaces/ISkillListener.h"
#include "SkillInstance.generated.h"

class ACharacter;
class USkillRuntimeSubsystem;

UENUM()
enum class ESkillState : uint8
{
	Idle,
	Running,
	Finished,
};

UCLASS()
class AIGAMESKILLRUNTIME_API USkillInstance : public UObject
{
	GENERATED_BODY()

public:
	/** 初始化技能实例 */
	void Initialize(TSharedPtr<FSkillData> InData, ACharacter* InOwner);

	/** 每帧驱动 */
	void Tick(float DeltaSeconds);

	/** 手动终止 */
	void Terminate();

	// ============================================
	// 状态查询（委托给 StateMachine）
	// ============================================

	bool IsFinished() const;
	bool IsRunning() const;
	ESkillState GetState() const;
	ACharacter* GetOwnerCharacter() const { return Owner.Get(); }

	/** 获取当前帧（供 Action / TrackHandler 读取） */
	int32 GetCurrentFrame() const { return CurrentFrame; }

	/** 获取播放速率（供 PlayAnimAction 读取） */
	float GetPlayRate() const { return PlayRate; }

	// ============================================
	// 变量容器
	// ============================================

	FSkillVarValue GetVariable(const FString& Name) const;
	void SetVariable(const FString& Name, const FSkillVarValue& Value);
	const TMap<FString, FSkillVarValue>& GetAllVariables() const { return Variables; }

	/** 解析值：如果是 { "__varRef": "X" } → 读变量，否则字面量 */
	FSkillVarValue ResolveValue(const TSharedPtr<FJsonValue>& JsonValue) const;

	/** 从 JSON 对象的指定字段解析值（支持 VarRef） */
	FSkillVarValue ResolveField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName) const;

	// ============================================
	// 输入标记（供 input 条件和 OnInput Listener 使用）
	// ============================================

	/** 设置输入标记（由 SkillComponent 在按键回调中调用） */
	void SetInputFlag(const FString& ButtonName, bool bPressed);

	/** 查询输入标记 */
	bool HasInputFlag(const FString& ButtonName) const;

	/** 清除所有输入标记（每帧末尾调用） */
	void ClearInputFlags();

	// ============================================
	// 数据访问（供 StateMachine / Flow / Action 使用）
	// ============================================

	/** 获取技能数据 */
	const TSharedPtr<FSkillData>& GetSkillData() const { return Data; }

	/** 获取 StateMachine */
	USkillStateMachine* GetStateMachine() const { return StateMachine; }

	/** 进入技能的过渡融合时间（从 Entry 节点的 blendInTime 读取） */
	float BlendInTime = 0.0f;

	/** 外部已预先设置好动画（如 DoActivateSkill 做了预融合），PlayAnimAction 跳过动画设置 */
	bool bAnimationPreBlended = false;

	// ============================================
	// 时间轴状态（供 Flow 层读写）
	// ============================================

	/** 当前时间轴帧（0 开始） */
	int32 CurrentFrame = 0;

	/** 播放速率（从动画轨道的 playRate 读取，默认 1.0） */
	float PlayRate = 1.0f;

	/** 当前行为节点的总帧数 */
	int32 TotalFrames = 0;

	/** 帧率（从动画或默认 30） */
	float FrameRate = 30.0f;

	/** 累积时间（用于帧推进） */
	float AccumulatedTime = 0.0f;

private:
	/** 技能数据 */
	TSharedPtr<FSkillData> Data;

	/** 拥有者角色 */
	TWeakObjectPtr<ACharacter> Owner;

	/** 状态机（Layer 1） */
	UPROPERTY()
	USkillStateMachine* StateMachine = nullptr;

	/** 变量容器 */
	TMap<FString, FSkillVarValue> Variables;

	/** 输入标记：ButtonName → 是否按下 */
	TMap<FString, bool> InputFlags;
};
