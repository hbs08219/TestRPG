// UAttributeComponent — 属性系统组件
// 弱 Schema：所有属性以 TMap<FName, float> 存储，加属性不需要改 C++。
// Modifier 系统：支持 Add / Multiply / Override 三种叠加方式，按 Source 标识增删。
//
// 计算公式（Phase 1）：
//     Final = (Base + ∑Add) * (1 + ∑MultiplyDelta) * Override?
//   - Add：直接加到 Base
//   - Multiply：累计百分比（例：0.2 表示 +20%）
//   - Override：直接覆盖最终值（最高优先级，存在则跳过 Add/Multiply）
//
// JSON 加载：通过 LoadFromJson(JsonString) 或 LoadFromFile(RelPath) 注入基础值。
// 委托：OnAttributeChanged 在最终值变化时广播（Old → New）。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

// =============================================================================
// 枚举与结构
// =============================================================================

UENUM(BlueprintType)
enum class EAttributeOp : uint8
{
	Add        UMETA(DisplayName="Add"),         // Base + Value
	Multiply   UMETA(DisplayName="Multiply"),    // *(1 + Value)
	Override   UMETA(DisplayName="Override"),    // 直接覆盖
};

/** 单条 Modifier 记录 */
USTRUCT(BlueprintType)
struct FAttributeModifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName Source = NAME_None;       // 装备 GUID / Buff GUID / Skill GUID

	UPROPERTY(BlueprintReadOnly)
	FName AttributeName = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	float Value = 0.f;

	UPROPERTY(BlueprintReadOnly)
	EAttributeOp Op = EAttributeOp::Add;

	/** 剩余回合数:-1=永久(装备等),>0=带时效(战斗 buff),回合开始 -- 归 0 移除。 */
	UPROPERTY(BlueprintReadOnly)
	int32 RemainingTurns = -1;

	/** 叠加层数(≥1):有效贡献 = Value × Stacks。 */
	UPROPERTY(BlueprintReadOnly)
	int32 Stacks = 1;
};

// =============================================================================
// 委托
// =============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttributeChanged,
	FName, AttributeName, float, OldValue, float, NewValue);

// =============================================================================
// Component
// =============================================================================

UCLASS(ClassGroup=(Attribute), meta=(BlueprintSpawnableComponent))
class AIGAMESKILLRUNTIME_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeComponent();

	// =========================================================================
	// 配置加载
	// =========================================================================

	/** 从 JSON 文件加载（相对 Content/Data/ToolGen/Attributes/） */
	UFUNCTION(BlueprintCallable, Category="Attribute")
	bool LoadFromFile(const FString& RelPath);

	/** 从 JSON 字符串加载 */
	bool LoadFromJsonString(const FString& JsonString);

	/** 按等级应用成长曲线(JSON "growthCurves"):有曲线的属性按 level 线性插值重设基础值。
	 *  HP/MP/SP 的曲线视为该等级上限 → 同步设置 Max* 且当前值回满。无曲线的属性不变。 */
	UFUNCTION(BlueprintCallable, Category="Attribute")
	void ApplyLevel(int32 Level);

	// =========================================================================
	// 基础值访问
	// =========================================================================

	/** 注册一个属性（不存在则添加；存在则覆盖基础值） */
	UFUNCTION(BlueprintCallable, Category="Attribute")
	void SetBaseValue(FName Attr, float Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Attribute")
	float GetBaseValue(FName Attr) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Attribute")
	bool HasAttribute(FName Attr) const;

	// =========================================================================
	// Modifier 管理
	// =========================================================================

	/** 注册一条 Modifier。同 Source+Attr+Op 会覆盖（避免重复叠加）。永久(RemainingTurns=-1)。 */
	UFUNCTION(BlueprintCallable, Category="Attribute|Modifier")
	void AddModifier(FName Source, FName Attr, float Value, EAttributeOp Op);

	/** 注册一条带回合时效的 Modifier（战斗 buff/debuff）。
	 *  同 Source+Attr+Op 已存在 → 叠层(Stacks++ 至 MaxStacks)、刷新 Value 与 RemainingTurns；
	 *  否则新建 Stacks=1。Turns<=0 视为 1。有效贡献 = Value×Stacks。 */
	UFUNCTION(BlueprintCallable, Category="Attribute|Modifier")
	void AddTimedModifier(FName Source, FName Attr, float Value, EAttributeOp Op, int32 Turns, int32 MaxStacks = 1);

	/** 回合开始调用：所有带时效(RemainingTurns>0)的 Modifier 计数 -1，归 0 的移除并刷新终值。
	 *  永久 Modifier(RemainingTurns=-1)不受影响。 */
	UFUNCTION(BlueprintCallable, Category="Attribute|Modifier")
	void TickModifierTurns();

	/** 返回所有带时效(RemainingTurns>0)的 Modifier(供 HUD 显示 buff/debuff 用)。 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Attribute|Modifier")
	TArray<FAttributeModifier> GetTimedModifiers() const;

	/** 移除某 Source 下的所有 Modifier（典型用例：装备卸下、Buff 结束） */
	UFUNCTION(BlueprintCallable, Category="Attribute|Modifier")
	void RemoveModifiersBySource(FName Source);

	/** 移除某 Source + Attr 的所有 Modifier */
	UFUNCTION(BlueprintCallable, Category="Attribute|Modifier")
	void RemoveModifiersBySourceAndAttr(FName Source, FName Attr);

	/** 清掉所有 Modifier（不动 Base） */
	UFUNCTION(BlueprintCallable, Category="Attribute|Modifier")
	void ClearAllModifiers();

	// =========================================================================
	// 最终值
	// =========================================================================

	/** 实时计算并返回最终值 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Attribute")
	float GetFinalValue(FName Attr) const;

	/** 一次性获取所有属性的最终值（用于 GM 工具） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Attribute")
	TMap<FName, float> GetAllFinalValues() const;

	// =========================================================================
	// 委托
	// =========================================================================

	UPROPERTY(BlueprintAssignable, Category="Attribute")
	FOnAttributeChanged OnAttributeChanged;

	// =========================================================================
	// 调试
	// =========================================================================

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Attribute|Debug")
	int32 GetModifierCount() const { return Modifiers.Num(); }

	UFUNCTION(BlueprintCallable, Category="Attribute|Debug")
	void DumpToLog() const;

private:
	/** 基础属性 Map（来自 JSON / SetBaseValue） */
	UPROPERTY()
	TMap<FName, float> BaseValues;

	/** 所有 Modifier */
	UPROPERTY()
	TArray<FAttributeModifier> Modifiers;

	/** 上一次广播过的最终值，用于检测变化（避免重复 broadcast） */
	mutable TMap<FName, float> CachedFinalValues;

	/** 成长曲线:属性名 → 按 level 升序的关键帧(来自 JSON "growthCurves",可选)。
	 *  非 UPROPERTY 纯运行时数据,仅 ApplyLevel 使用。 */
	struct FGrowthKeyframe { int32 Level = 1; float Value = 0.f; };
	TMap<FName, TArray<FGrowthKeyframe>> GrowthCurves;

	/** 重新计算并广播 dirty 的属性 */
	void RecomputeAndBroadcast(FName Attr);

	/** 内部：计算最终值（不缓存，纯函数） */
	float ComputeFinalValue(FName Attr) const;
};
