// UBattleFlow 实现 — 战斗三层架构 Layer 2
//
// 战斗节奏(移动/攻击/返回/停留/施法蓄力)由 ProjectSettings.battle 配置,
// 经 ABattleManager → UBattleStateMachine::SetTiming 注入,Flow 用 OwnerSM->GetTiming() 读。

#include "BattleFlow.h"
#include "BattleStateMachine.h"
#include "BattleAction.h"
#include "BattleDamageCalculator.h"
#include "BattleEnemyAI.h"
#include "Components/AttributeComponent.h"
#include "Components/CombatStatusComponent.h"
#include "SkillEffectResolver.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"

namespace
{
	FString CharName(ACharacter* C) { return C ? C->GetName() : FString(TEXT("<null>")); }
}

// ============================================
// UBattleFlow 基类
// ============================================

void UBattleFlow::Initialize(UBattleStateMachine* InOwnerSM)
{
	OwnerSM = InOwnerSM;
	bFinished = false;
}

void UBattleFlow::Enter()           { MarkFinished(); }
void UBattleFlow::Tick(float)       {}
void UBattleFlow::Exit()            {}

void UBattleFlow::MarkFinished()
{
	if (bFinished) return;
	bFinished = true;
	OnFinished.Broadcast();
}

// ----- Action 队列运行器（Layer 3 驱动）-----

void UBattleFlow::StartActions(const TArray<UBattleAction*>& Actions)
{
	ActionQueue = Actions;
	ActionCursor = 0;
	if (ActionQueue.IsValidIndex(ActionCursor) && ActionQueue[ActionCursor])
	{
		ActionQueue[ActionCursor]->Enter();
	}
}

bool UBattleFlow::TickActions(float DeltaSeconds)
{
	if (!ActionQueue.IsValidIndex(ActionCursor))
	{
		return true; // 队列已空 / 已跑完
	}

	UBattleAction* Cur = ActionQueue[ActionCursor];
	if (!Cur)
	{
		ActionCursor++;
		return !ActionQueue.IsValidIndex(ActionCursor);
	}

	Cur->Tick(DeltaSeconds);
	if (Cur->IsActionFinished())
	{
		ActionCursor++;
		if (ActionQueue.IsValidIndex(ActionCursor) && ActionQueue[ActionCursor])
		{
			ActionQueue[ActionCursor]->Enter();
			return false;
		}
		return true; // 全部完成
	}
	return false;
}

bool UBattleFlow::HasActions() const
{
	return ActionQueue.IsValidIndex(ActionCursor);
}

// ============================================
// UBattleFlow_Idle — 战斗开局（P0：立即跳 PlayerAction）
// ============================================

void UBattleFlow_Idle::Enter()
{
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:Idle] enter"));
	MarkFinished();
}

// ============================================
// UBattleFlow_PlayerAction
// ============================================

void UBattleFlow_PlayerAction::Enter()
{
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] enter"));
	NextState = TEXT("EnemyAction");
	bUsingActionQueue = false;

	if (!OwnerSM)
	{
		MarkFinished();
		return;
	}

	// 回合开始结算(能力系统时效):DoT 扣血 → buff/控制时效递减 → 眩晕跳过。
	if (ACharacter* P = OwnerSM->GetPlayerChar())
	{
		if (UCombatStatusComponent* CS = FSkillEffectResolver::GetOrCreateStatus(P))
		{
			const FCombatTurnTickResult R = CS->TickTurnStart();
			if (R.PoisonDamage > 0)
			{
				UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] 玩家中毒 -%dHP"), R.PoisonDamage);
				OwnerSM->OnBattleHit.Broadcast(P, P, R.PoisonDamage, R.bDiedFromPoison);
			}
			if (R.bDiedFromPoison)
			{
				MarkFinished();  // 毒死:交由 SM 结算败北
				return;
			}
			if (R.bStunned)
			{
				UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] 玩家被眩晕,跳过回合"));
				MarkFinished();
				return;
			}
		}
	}

	// P0 回合制:等待玩家经 HUD 提交指令(ABattleManager 转发 OnCommandSelected → SubmitPlayerCommand)。
	// Tick 轮询 HasPendingPlayerCommand,收到后调 ResolveCommand 解算本回合。
	// 若玩家在敌方回合提前点过(已有 pending),Tick 下一帧即解算,不额外等待。
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] waiting for player command (HUD)..."));
}

void UBattleFlow_PlayerAction::ResolveCommand()
{
	if (!OwnerSM)
	{
		MarkFinished();
		return;
	}

	// 取出玩家指令（无指令时默认普攻）
	Command = OwnerSM->ConsumePendingPlayerCommand();

	// 解析目标
	ACharacter* Target = nullptr;
	if (Command.TargetIndex >= 0)
	{
		Target = OwnerSM->GetEnemyAt(Command.TargetIndex);
	}
	if (!Target)
	{
		Target = OwnerSM->GetCurrentEnemy();
	}
	TargetEnemy = Target;

	switch (Command.Command)
	{
	case EBattleCommand::Defend:
	{
		// 即时指令：标记防御后立即结束本回合
		UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] DEFEND - reduces incoming damage by 50%%"));
		OwnerSM->SetPlayerDefending(true);
		MarkFinished();
		return;
	}
	case EBattleCommand::Flee:
	{
		// 即时指令:逃跑 100% 成功(P0 调试设定)。
		UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] FLEE - success (100%%)"));
		OwnerSM->MarkFled();
		NextState = TEXT("Finished");
		MarkFinished();
		return;
	}
	case EBattleCommand::Item:
	{
		// 即时指令：物品立即生效（P0：默认治疗药 = +50 HP；正式 Schema 见 Phase 4）
		UAttributeComponent* SelfAttr = OwnerSM->GetPlayerAttr();
		const int32 HealAmount = (Command.SkillHealAmount > 0) ? Command.SkillHealAmount : 50;
		const int32 Done = FBattleDamageCalculator::ApplyHealing(SelfAttr, HealAmount);
		UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] ITEM '%s' - heal +%d"),
			*Command.ItemId, Done);
		MarkFinished();
		return;
	}
	case EBattleCommand::Skill:
	{
		// 技能执行器(阶段3):读逻辑页时间线的判定/效果轨,按目标集施加效果组件。
		// 取代旧的「单一 power/heal 写死结算」。技能在 Flow 层即时解算(无 Move/Hit/Return 队列)。
		UAttributeComponent* SelfAttr = OwnerSM->GetPlayerAttr();

		FSkillEffectPlan Plan;
		if (!FSkillEffectResolver::LoadPlan(Command.SkillPath, Plan) || !Plan.IsValid())
		{
			UE_LOG(LogSkillRuntime, Warning,
				TEXT("[BattleFlow:PlayerAction] SKILL '%s' 无有效效果计划,跳过"), *Command.SkillPath);
			MarkFinished();
			return;
		}

		// 门控(沉默/MP/requireTags/blockTags):与 HUD 灰显共用 resolver::CanCast(单一判定源)
		UCombatStatusComponent* CasterCS = FSkillEffectResolver::GetOrCreateStatus(OwnerSM->GetPlayerChar());
		const float CurMP = SelfAttr ? SelfAttr->GetFinalValue(BattleAttr::MP) : 0.f;
		FString Reason;
		if (!FSkillEffectResolver::CanCast(Plan, CasterCS, CurMP, Reason))
		{
			UE_LOG(LogSkillRuntime, Warning,
				TEXT("[BattleFlow:PlayerAction] SKILL '%s' 不可放(%s),跳过"), *Plan.Name, *Reason);
			MarkFinished();
			return;
		}
		// 扣 MP
		if (Plan.MpCost > 0 && SelfAttr)
		{
			SelfAttr->SetBaseValue(BattleAttr::MP, CurMP - Plan.MpCost);
		}

		// 施法上下文:玩家方=友军,敌人列表=敌方;single 锚点用上面按 TargetIndex 解析的 Target
		FSkillTargetContext Ctx;
		Ctx.Caster         = OwnerSM->GetPlayerChar();
		Ctx.SelectedTarget = Target;
		Ctx.Enemies        = OwnerSM->GetAliveEnemies();
		Ctx.Allies         = { OwnerSM->GetPlayerChar() };

		// 走施法 Action(动画 + 节奏 + 在蓄力帧调 resolver 结算),不再瞬间结算
		UBattleAction_CastSkill* CastAct = NewObject<UBattleAction_CastSkill>(this);
		CastAct->InitializeAction(OwnerSM);
		CastAct->Setup(OwnerSM->GetPlayerChar(), Plan, Ctx, Plan.CastClip,
			/*Windup=*/OwnerSM->GetTiming().SkillWindup, /*Total=*/0.9f);
		StartActions(TArray<UBattleAction*>{ CastAct });
		bUsingActionQueue = true;

		UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] SKILL '%s' 施法开始"), *Plan.Name);
		return;
	}
	case EBattleCommand::Attack:
	default:
	{
		if (!Target)
		{
			UE_LOG(LogSkillRuntime, Warning,
				TEXT("[BattleFlow:PlayerAction] no valid target, skip"));
			MarkFinished();
			return;
		}
		// MP 校验（技能时）
		if (Command.Command == EBattleCommand::Skill && Command.MpCost > 0)
		{
			UAttributeComponent* SelfAttr = OwnerSM->GetPlayerAttr();
			const float CurMP = SelfAttr ? SelfAttr->GetFinalValue(BattleAttr::MP) : 0.f;
			if (CurMP < Command.MpCost)
			{
				UE_LOG(LogSkillRuntime, Warning,
					TEXT("[BattleFlow:PlayerAction] not enough MP (%.0f<%d), fallback to attack"),
					CurMP, Command.MpCost);
				Command.Command = EBattleCommand::Attack;
			}
			else
			{
				SelfAttr->SetBaseValue(BattleAttr::MP, CurMP - Command.MpCost);
			}
		}
		UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:PlayerAction] %s → %s"),
			Command.Command == EBattleCommand::Skill ? TEXT("SKILL") : TEXT("ATTACK"),
			*CharName(Target));

		// 构建 Action 队列：移动 → 命中结算 → 返回（敌人 P0 不防御，故 bDefenderDefending=false）
		ACharacter* Player = OwnerSM->GetPlayerChar();

		UBattleAction_MoveToTarget* Move = NewObject<UBattleAction_MoveToTarget>(this);
		Move->InitializeAction(OwnerSM);
		Move->Setup(Player, Target, OwnerSM->GetTiming().MoveDuration);

		UBattleAction_ExecuteHit* Hit = NewObject<UBattleAction_ExecuteHit>(this);
		Hit->InitializeAction(OwnerSM);
		Hit->Setup(Player, Target, Command, /*bDefenderDefending=*/false, OwnerSM->GetTiming().AttackDuration);

		UBattleAction_ReturnHome* Ret = NewObject<UBattleAction_ReturnHome>(this);
		Ret->InitializeAction(OwnerSM);
		Ret->Setup(Player, OwnerSM->GetIdleLocation(), OwnerSM->GetTiming().ReturnDuration);

		StartActions(TArray<UBattleAction*>{ Move, Hit, Ret });
		bUsingActionQueue = true;
		return;
	}
	}
}

void UBattleFlow_PlayerAction::Tick(float DeltaSeconds)
{
	if (bFinished) return;

	// 等待玩家指令阶段:轮询 HUD 提交。即时指令(防御/物品/逃跑)在 ResolveCommand 内已 MarkFinished,不会走到这。
	if (!bUsingActionQueue)
	{
		if (OwnerSM && OwnerSM->HasPendingPlayerCommand())
		{
			ResolveCommand();
		}
		return;
	}

	if (TickActions(DeltaSeconds))
	{
		// Action 队列跑完。CheckBattleEnd 若返回 true 会切到 Victory/Defeat（旧 Flow 委托已解绑，
		// 后续 MarkFinished 为安全空转）；否则交回 SM 推进 NextState。
		if (NextState.Equals(TEXT("Finished")) || (OwnerSM && OwnerSM->CheckBattleEnd()))
		{
			MarkFinished();
			return;
		}
		MarkFinished();
	}
}

// ============================================
// UBattleFlow_EnemyAction
// ============================================

void UBattleFlow_EnemyAction::Enter()
{
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:EnemyAction] enter"));
	CurrentEnemyIdx = -1;

	if (!OwnerSM)
	{
		MarkFinished();
		return;
	}
	StepNextEnemy();
}

void UBattleFlow_EnemyAction::StepNextEnemy()
{
	if (!OwnerSM)
	{
		MarkFinished();
		return;
	}

	// 取下一个存活敌人（按 Enemies 数组顺序）
	const TArray<ACharacter*> Alive = OwnerSM->GetAliveEnemies();
	CurrentEnemyIdx++;
	if (!Alive.IsValidIndex(CurrentEnemyIdx))
	{
		// 所有敌人都行动完了：清防御标记（防御只持续一回合）后结束本 Flow
		OwnerSM->SetPlayerDefending(false);
		MarkFinished();
		return;
	}

	CurrentEnemy = Alive[CurrentEnemyIdx];
	ACharacter* Enemy = CurrentEnemy.Get();
	ACharacter* Player = OwnerSM->GetPlayerChar();

	// 死亡 / 无目标 → 跳过该敌人
	if (!Enemy || !Player)
	{
		StepNextEnemy();
		return;
	}

	// 敌人回合开始结算:DoT 扣血 → 时效递减 → 眩晕则跳过其行动。
	if (UCombatStatusComponent* CS = FSkillEffectResolver::GetOrCreateStatus(Enemy))
	{
		const FCombatTurnTickResult R = CS->TickTurnStart();
		if (R.PoisonDamage > 0)
		{
			UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:EnemyAction] %s 中毒 -%dHP"), *CharName(Enemy), R.PoisonDamage);
			OwnerSM->OnBattleHit.Broadcast(Enemy, Enemy, R.PoisonDamage, R.bDiedFromPoison);
		}
		if (R.bDiedFromPoison) { StepNextEnemy(); return; }  // 毒死,换下一个
		if (R.bStunned)
		{
			UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:EnemyAction] %s 被眩晕,跳过"), *CharName(Enemy));
			StepNextEnemy();
			return;
		}
	}

	CurrentCommand = FBattleEnemyAI::DecideAction(OwnerSM, Enemy);

	// 技能命令(带 path)→ 走能力系统 CastSkill(敌人 POV:施法者=敌、敌方=玩家);加载失败回退普攻
	if (CurrentCommand.Command == EBattleCommand::Skill && !CurrentCommand.SkillPath.IsEmpty())
	{
		FSkillEffectPlan Plan;
		if (FSkillEffectResolver::LoadPlan(CurrentCommand.SkillPath, Plan) && Plan.IsValid())
		{
			FSkillTargetContext Ctx;
			Ctx.Caster         = Enemy;
			Ctx.SelectedTarget = Player;
			Ctx.Enemies        = { Player };               // 敌人视角:玩家方=敌方
			Ctx.Allies         = OwnerSM->GetAliveEnemies(); // 友方=其他存活敌人(+自己)

			UBattleAction_CastSkill* CastAct = NewObject<UBattleAction_CastSkill>(this);
			CastAct->InitializeAction(OwnerSM);
			CastAct->Setup(Enemy, Plan, Ctx, Plan.CastClip, /*Windup=*/OwnerSM->GetTiming().SkillWindup, /*Total=*/0.9f);
			StartActions(TArray<UBattleAction*>{ CastAct });
			UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:EnemyAction] %s 施法 '%s'"), *CharName(Enemy), *Plan.Name);
			return;
		}
		// 计划无效 → 退回普攻
		CurrentCommand.Command = EBattleCommand::Attack;
	}

	// 普攻:构建 Action 队列：移动 → 命中结算（受玩家防御影响）→ 返回
	UBattleAction_MoveToTarget* Move = NewObject<UBattleAction_MoveToTarget>(this);
	Move->InitializeAction(OwnerSM);
	Move->Setup(Enemy, Player, OwnerSM->GetTiming().MoveDuration);

	UBattleAction_ExecuteHit* Hit = NewObject<UBattleAction_ExecuteHit>(this);
	Hit->InitializeAction(OwnerSM);
	Hit->Setup(Enemy, Player, CurrentCommand, OwnerSM->IsPlayerDefending(), OwnerSM->GetTiming().AttackDuration);

	UBattleAction_ReturnHome* Ret = NewObject<UBattleAction_ReturnHome>(this);
	Ret->InitializeAction(OwnerSM);
	Ret->Setup(Enemy, Enemy->GetActorLocation(), OwnerSM->GetTiming().ReturnDuration);

	StartActions(TArray<UBattleAction*>{ Move, Hit, Ret });
}

void UBattleFlow_EnemyAction::Tick(float DeltaSeconds)
{
	if (bFinished) return;

	if (TickActions(DeltaSeconds))
	{
		// 当前敌人行动完毕：玩家若已倒下则提前结束，否则换下一个敌人
		if (OwnerSM && OwnerSM->CheckBattleEnd())
		{
			MarkFinished();
			return;
		}
		StepNextEnemy();
	}
}

// ============================================
// UBattleFlow_Victory
// ============================================

void UBattleFlow_Victory::Enter()
{
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:Victory] player wins!"));
	Timer = 0.f;
	// TODO Phase 3：触发 HUD 胜利动画/UI
}

void UBattleFlow_Victory::Tick(float DeltaSeconds)
{
	if (bFinished) return;
	Timer += DeltaSeconds;
	if (Timer >= OwnerSM->GetTiming().VictoryHold)
	{
		MarkFinished();
	}
}

// ============================================
// UBattleFlow_Defeat
// ============================================

void UBattleFlow_Defeat::Enter()
{
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:Defeat] player defeated"));
	Timer = 0.f;
	// TODO Phase 3：触发 HUD 失败动画/重试按钮
}

void UBattleFlow_Defeat::Tick(float DeltaSeconds)
{
	if (bFinished) return;
	Timer += DeltaSeconds;
	if (Timer >= OwnerSM->GetTiming().DefeatHold)
	{
		MarkFinished();
	}
}

// ============================================
// UBattleFlow_Reward
// ============================================

void UBattleFlow_Reward::Enter()
{
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:Reward] distributing rewards"));
	Timer = 0.f;
	// P0 占位：固定经验+金钱（Phase 4 接入 EnemyDataSubsystem 后从敌人配置读）
	// 仅日志，实际累加待 Phase 4 的 PlayerProgressSubsystem
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleFlow:Reward] +10 EXP, +5 Gold (placeholder)"));
}

void UBattleFlow_Reward::Tick(float DeltaSeconds)
{
	if (bFinished) return;
	Timer += DeltaSeconds;
	if (Timer >= OwnerSM->GetTiming().RewardHold)
	{
		MarkFinished();
	}
}
