// FBattleCommand — 战斗指令载荷
//
// P0 战斗指令：Attack / Skill / Item / Defend / Flee
// PlayerAction Flow 通过 UBattleStateMachine::SubmitPlayerCommand() 接收 UI 选项；
// EnemyAction Flow 通过 FBattleEnemyAI::DecideAction() 直接产出。

#pragma once

#include "CoreMinimal.h"
#include "BattleCommand.generated.h"

UENUM(BlueprintType)
enum class EBattleCommand : uint8
{
	Attack    UMETA(DisplayName="普攻"),
	Skill     UMETA(DisplayName="技能"),
	Item      UMETA(DisplayName="物品"),
	Defend    UMETA(DisplayName="防御"),
	Flee      UMETA(DisplayName="逃跑"),
};

/** 战斗指令载荷：UI/AI 提交给战斗状态机的统一结构 */
USTRUCT(BlueprintType)
struct AIGAMESKILLRUNTIME_API FBattleCommandPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	EBattleCommand Command = EBattleCommand::Attack;

	/** 技能 path（仅 Command==Skill 时有效），相对 Content/Data/ToolGen/Skill */
	UPROPERTY(BlueprintReadWrite)
	FString SkillPath;

	/** 物品 ID（仅 Command==Item 时有效） */
	UPROPERTY(BlueprintReadWrite)
	FString ItemId;

	/** 目标敌人索引（在 BattleStateMachine 的 Enemies 数组中的 idx）；负数 = 自动选第一个存活 */
	UPROPERTY(BlueprintReadWrite)
	int32 TargetIndex = -1;

	/** 技能威力（P0：先随 payload 传，未来从 SkillData.battleParams 读取） */
	UPROPERTY(BlueprintReadWrite)
	int32 SkillPower = 0;

	/** 治疗量（仅 Skill 类型且为治疗时） */
	UPROPERTY(BlueprintReadWrite)
	int32 SkillHealAmount = 0;

	/** MP 消耗 */
	UPROPERTY(BlueprintReadWrite)
	int32 MpCost = 0;
};
