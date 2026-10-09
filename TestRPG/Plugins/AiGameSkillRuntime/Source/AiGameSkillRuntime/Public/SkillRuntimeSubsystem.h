// USkillRuntimeSubsystem — 技能运行时注册中心
// GameInstanceSubsystem，管理四类注册表 + 技能数据缓存

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "SkillDataTypes.h"
#include "SkillConditionContext.h"
#include "Interfaces/ISkillAction.h"
#include "Interfaces/ISkillTrackHandler.h"
#include "Interfaces/ISkillListener.h"
#include "SkillRuntimeSubsystem.generated.h"

/** 条件评估函数类型 */
using FSkillConditionEvaluator = TFunction<bool(const FSkillConditionContext&)>;

UCLASS()
class AIGAMESKILLRUNTIME_API USkillRuntimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============================================
	// 注册接口
	// ============================================

	/** 注册行为 Handler 工厂 */
	void RegisterAction(FName Type, TFunction<TSharedPtr<ISkillAction>()> Factory);

	/** 注册条件评估函数 */
	void RegisterCondition(FName Type, FSkillConditionEvaluator Evaluator);

	/** 注册轨道 Handler 工厂 */
	void RegisterTrack(FName Type, TFunction<TSharedPtr<ISkillTrackHandler>()> Factory);

	/** 注册监听 Handler 工厂 */
	void RegisterListener(FName Type, TFunction<TSharedPtr<ISkillListener>()> Factory);

	// ============================================
	// 创建 / 查询
	// ============================================

	/** 创建指定类型的 Action 实例 */
	TSharedPtr<ISkillAction> CreateAction(FName Type) const;

	/** 评估指定类型的条件 */
	bool EvaluateCondition(FName Type, const FSkillConditionContext& Ctx) const;

	/** 创建指定类型的 TrackHandler 实例 */
	TSharedPtr<ISkillTrackHandler> CreateTrackHandler(FName Type) const;

	/** 创建指定类型的 Listener 实例 */
	TSharedPtr<ISkillListener> CreateListener(FName Type) const;

	// ============================================
	// 技能数据加载
	// ============================================

	/** 按相对路径加载技能 JSON（如 "Test/SK_xxx"） */
	TSharedPtr<FSkillData> LoadSkillData(const FString& SkillPath);

	/** 清除缓存 */
	void FlushCache();

private:
	/** 注册表 */
	TMap<FName, TFunction<TSharedPtr<ISkillAction>()>> ActionRegistry;
	TMap<FName, FSkillConditionEvaluator> ConditionRegistry;
	TMap<FName, TFunction<TSharedPtr<ISkillTrackHandler>()>> TrackRegistry;
	TMap<FName, TFunction<TSharedPtr<ISkillListener>()>> ListenerRegistry;

	/** 技能数据缓存（路径 → 解析结果） */
	TMap<FString, TSharedPtr<FSkillData>> SkillDataCache;

	/** 根目录 */
	static FString GetSkillRootDir();
};
