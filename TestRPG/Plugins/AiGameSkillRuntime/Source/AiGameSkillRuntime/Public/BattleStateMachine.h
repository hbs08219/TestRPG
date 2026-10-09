// UBattleStateMachine — 战斗状态机（Layer 1）
// 管理战斗各阶段的状态切换，Tick 驱动当前 Flow

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Math/Vector.h"
#include "BattleCommand.h"
#include "BattleStateMachine.generated.h"

class UBattleFlow;
class ACharacter;
class UAttributeComponent;

UENUM(BlueprintType)
enum class EBattleResult : uint8
{
	Ongoing  UMETA(DisplayName="进行中"),
	Victory  UMETA(DisplayName="胜利"),
	Defeat   UMETA(DisplayName="失败"),
	Fled     UMETA(DisplayName="逃跑"),
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBattleStateChanged, const FString& /*From*/, const FString& /*To*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleEnded, EBattleResult /*Result*/);
/** 战斗命中事件(供 HUD/屏幕消息显示):攻击者、目标、实际伤害、目标是否死亡 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnBattleHitNative, ACharacter* /*Attacker*/, ACharacter* /*Target*/, int32 /*Damage*/, bool /*bTargetDead*/);

/** 战斗节奏配置(秒);来自 ProjectSettings.battle,由 ABattleManager 注入。默认值=原写死值。 */
USTRUCT(BlueprintType)
struct AIGAMESKILLRUNTIME_API FBattleTiming
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle|Timing")
	float MoveDuration   = 0.6f;   // 移动到目标
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle|Timing")
	float AttackDuration = 0.4f;   // 普攻命中蓄力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle|Timing")
	float ReturnDuration = 0.6f;   // 返回站位
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle|Timing")
	float VictoryHold    = 1.5f;   // 胜利停留
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle|Timing")
	float DefeatHold     = 2.0f;   // 战败停留
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle|Timing")
	float RewardHold     = 2.0f;   // 奖励结算停留
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle|Timing")
	float SkillWindup    = 0.4f;   // 施法蓄力(效果结算点;总时长按动画 clip 自动算)
};

UCLASS()
class AIGAMESKILLRUNTIME_API UBattleStateMachine : public UObject
{
	GENERATED_BODY()

public:
	/** 初始化（传入玩家角色，以及敌人列表）*/
	void Initialize(ACharacter* InPlayerChar, const TArray<ACharacter*>& InEnemies);

	/** 战斗节奏配置(战斗开始前由 BattleManager 注入;Flow 经 GetTiming 读) */
	void SetTiming(const FBattleTiming& InTiming) { Timing = InTiming; }

	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	const FBattleTiming& GetTiming() const { return Timing; }

	/** 每帧驱动 */
	void Tick(float DeltaSeconds);

	/** 终止（战斗结束） */
	void Terminate();

	/** 状态查询 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	bool IsFinished() const { return bFinished; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	bool IsRunning() const { return !bFinished && CurrentFlow != nullptr; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	EBattleResult GetResult() const { return Result; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	FString GetCurrentStateName() const;

	/** 请求状态切换（由 Flow 调用） */
	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	void RequestTransition(const FString& NextStateName);

	/** ── TS 脚本驱动模式 ── */
	/** 启用后 StateMachine::Tick 跳过 C++ Flow,TS 经 RequestTransition/SubmitPlayerCommand/Action 工厂全权驱动。 */
	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	void SetUseScriptFlow(bool bUse) { bUseScriptFlow = bUse; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	bool IsUseScriptFlow() const { return bUseScriptFlow; }

	/** 获取玩家角色 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	ACharacter* GetPlayerChar() const { return PlayerChar.Get(); }

	/** 获取玩家 AttributeComponent */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	UAttributeComponent* GetPlayerAttr() const;

	/** 获取当前目标敌人（P0：第一个存活敌人）*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	ACharacter* GetCurrentEnemy() const;

	/** 按索引取敌人；越界/已死则返回 nullptr */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	ACharacter* GetEnemyAt(int32 Index) const;

	/** 获取敌人 AttributeComponent */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	UAttributeComponent* GetCurrentEnemyAttr() const;

	/** 获取所有存活敌人数组（弱指针解引用） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	TArray<ACharacter*> GetAliveEnemies() const;

	/** 获取玩家初始位置（用于回到原位）*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	FVector GetIdleLocation() const { return IdleLocation; }

	/** 检查战斗是否结束（玩家全灭 → Defeat，敌人全灭 → Victory）。
	 *  返回 true 表示已结束并切到了 Victory/Defeat。 */
	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	bool CheckBattleEnd();

	// ============================================
	// 玩家指令通道（UI/调试入口提交）
	// ============================================

	/** 由 UI/调试逻辑提交玩家本回合指令 */
	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	void SubmitPlayerCommand(const FBattleCommandPayload& Cmd);

	/** 是否已有待消费的玩家指令 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	bool HasPendingPlayerCommand() const { return bHasPendingCommand; }

	/** 取出并清空当前待消费的玩家指令；无指令则返回默认 Attack */
	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	FBattleCommandPayload ConsumePendingPlayerCommand();

	// ============================================
	// 防御状态（每回合开始清零）
	// ============================================

	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	void SetPlayerDefending(bool bDefending) { bPlayerDefending = bDefending; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Battle|StateMachine")
	bool IsPlayerDefending() const { return bPlayerDefending; }

	/** 标记本场战斗为"逃跑成功"：逃跑指令成功时由 Flow 调用，
	 *  使后续 Finished 切换时 OnBattleEnded 广播 EBattleResult::Fled。 */
	UFUNCTION(BlueprintCallable, Category="Battle|StateMachine")
	void MarkFled() { Result = EBattleResult::Fled; }

	// ============================================
	// 委托
	// ============================================

	FOnBattleStateChanged OnStateChanged;
	FOnBattleEnded OnBattleEnded;
	FOnBattleHitNative OnBattleHit;

protected:
	/** 创建指定类型的 Flow */
	UBattleFlow* CreateFlow(const FString& FlowTypeName);

	/** 当前 Flow 结束回调 */
	void OnFlowFinished();

	UPROPERTY() UBattleFlow* CurrentFlow = nullptr;
	TWeakObjectPtr<ACharacter> PlayerChar;
	TArray<TWeakObjectPtr<ACharacter>> Enemies;
	FVector IdleLocation = FVector::ZeroVector;
	bool bFinished = false;
	EBattleResult Result = EBattleResult::Ongoing;

	/** 待消费的玩家指令（PlayerAction Flow 进入时取出） */
	FBattleCommandPayload PendingCommand;
	bool bHasPendingCommand = false;

	/** 玩家本回合是否选择了"防御"（被攻击时减伤；EnemyAction 结束后清零） */
	bool bPlayerDefending = false;

	/** 战斗节奏(BattleManager 从 ProjectSettings 注入;默认=原写死值) */
	FBattleTiming Timing;

	/** 是否启用 TS 脚本驱动(跳过 C++ Flow::Tick) */
	bool bUseScriptFlow = false;

	/** Flow 完成委托句柄（用于解绑） */
	FDelegateHandle FlowFinishedHandle;
};
