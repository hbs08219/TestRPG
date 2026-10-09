// FBattleEnemyAI 实现
//
// P0 策略（对齐设计文档§6）：随机选一个存活玩家 + 随机普攻/技能。
//   - 目标：P0 单人队伍，目标恒为玩家；多人时在此随机挑存活玩家。
//   - 行动：50% 普攻 / 50% 技能。
//     P0 敌人技能数据尚未接入（Phase 4 的 EnemyDataSubsystem 才从敌人 JSON
//     的 skills.battleParams 读威力/MP），故技能用占位威力且不耗 MP。
// 后续扩展点（不在 P0）：
//   - 检测玩家血量，残血时优先普攻 / 优先治疗
//   - boss 模式：固定连招序列

#include "BattleEnemyAI.h"
#include "BattleStateMachine.h"
#include "Interfaces/IBattleSkillSource.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"

FBattleCommandPayload FBattleEnemyAI::DecideAction(UBattleStateMachine* SM, ACharacter* Enemy)
{
	FBattleCommandPayload Cmd;
	Cmd.Command = EBattleCommand::Attack;
	Cmd.TargetIndex = -1; // 自动选玩家（P0 单人队伍，多人时此处随机选存活玩家）

	if (!SM || !Enemy)
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("[EnemyAI] DecideAction called with null SM/Enemy"));
		return Cmd;
	}

	// 敌人技能池(经接口取,SkillRuntime 不依赖 AiGame 的敌人类)
	TArray<FString> Skills;
	if (const IBattleSkillSource* Src = Cast<IBattleSkillSource>(Enemy))
	{
		Skills = Src->GetBattleSkillPaths();
	}

	// 有技能且掷中 → 放真技能(随机挑一个;走能力系统 resolver);否则普攻
	if (Skills.Num() > 0 && FMath::FRand() < 0.5f)
	{
		Cmd.Command = EBattleCommand::Skill;
		Cmd.SkillPath = Skills[FMath::RandRange(0, Skills.Num() - 1)];
		UE_LOG(LogSkillRuntime, Log, TEXT("[EnemyAI] %s decides: Skill '%s'"), *Enemy->GetName(), *Cmd.SkillPath);
	}
	else
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("[EnemyAI] %s decides: Attack"), *Enemy->GetName());
	}
	return Cmd;
}
