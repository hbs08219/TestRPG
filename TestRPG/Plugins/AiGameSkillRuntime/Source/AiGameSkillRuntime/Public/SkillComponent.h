// USkillComponent — 挂在角色上的技能组件
// 技能互斥：旧技能未结束时新触发被拒绝（不是打断）
// 按键 → 技能映射改由 UCustomInputSubsystem 提供（JSON 配置）

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillComponent.generated.h"

class USkillInstance;

DECLARE_MULTICAST_DELEGATE(FOnSkillFinished);

UCLASS(ClassGroup=(Skill), meta=(BlueprintSpawnableComponent))
class AIGAMESKILLRUNTIME_API USkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================
	// 技能激活
	// ============================================

	/** 尝试激活技能。旧技能未结束 → 返回 false */
	UFUNCTION(BlueprintCallable, Category="Skill")
	bool TryActivateSkill(const FString& SkillPath);

	/** 设置当前活跃技能的输入标记（供 input 条件 / OnInput Listener 读取） */
	UFUNCTION(BlueprintCallable, Category="Skill")
	void SetActiveSkillInputFlag(const FString& ButtonName, bool bPressed);

	/** 当前是否正忙（技能运行中） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skill")
	bool IsBusy() const;

	/** 获取当前技能实例 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Skill")
	USkillInstance* GetActiveInstance() const { return ActiveInstance; }

	/** 技能完成回调（在动画模式恢复后、实例清除前广播） */
	FOnSkillFinished OnSkillFinished;

private:
	/** 当前活跃的技能实例 */
	UPROPERTY()
	USkillInstance* ActiveInstance = nullptr;
};
