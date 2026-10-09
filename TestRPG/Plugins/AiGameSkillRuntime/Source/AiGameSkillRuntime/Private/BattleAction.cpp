// UBattleAction 实现 — 战斗三层架构 Layer 3

#include "BattleAction.h"
#include "BattleStateMachine.h"
#include "BattleDamageCalculator.h"
#include "SkillEffectResolver.h"
#include "Components/AttributeComponent.h"
#include "Interfaces/ISkillAnim2DReceiver.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"

namespace
{
	UAttributeComponent* BA_GetAttr(ACharacter* Char)
	{
		if (!Char) return nullptr;
		return Cast<UAttributeComponent>(
			Char->FindComponentByClass(UAttributeComponent::StaticClass()));
	}

	FString BA_CharName(ACharacter* C) { return C ? C->GetName() : FString(TEXT("<null>")); }

	/** 取角色上实现 ISkillAnim2DReceiver 的组件(UAppearanceComponent);非 2D / 无接收者 → nullptr。 */
	ISkillAnim2DReceiver* BA_GetAnimReceiver(ACharacter* C)
	{
		if (!C) return nullptr;
		for (UActorComponent* Comp : C->GetComponents())
		{
			if (Comp && Comp->GetClass()->ImplementsInterface(USkillAnim2DReceiver::StaticClass()))
			{
				if (ISkillAnim2DReceiver* R = Cast<ISkillAnim2DReceiver>(Comp))
				{
					if (R->SkillAnim_Is2D()) return R;
				}
			}
		}
		return nullptr;
	}
}

// ============================================
// UBattleAction 基类
// ============================================

void UBattleAction::InitializeAction(UBattleStateMachine* InSM)
{
	SM = InSM;
	bActionFinished = false;
}

// ============================================
// UBattleAction_MoveToTarget
// ============================================

void UBattleAction_MoveToTarget::Setup(ACharacter* InMover, ACharacter* InTarget, float InDuration)
{
	Mover = InMover;
	Target = InTarget;
	Duration = InDuration;
}

void UBattleAction_MoveToTarget::Enter()
{
	Elapsed = 0.f;
	// Phase 2：记录起点，Tick 内向 Target 前方 lerp
}

void UBattleAction_MoveToTarget::Tick(float DeltaSeconds)
{
	if (bActionFinished) return;
	Elapsed += DeltaSeconds;
	if (Elapsed >= Duration)
	{
		FinishAction();
	}
}

// ============================================
// UBattleAction_ExecuteHit
// ============================================

void UBattleAction_ExecuteHit::Setup(ACharacter* InAttacker, ACharacter* InDefender,
	const FBattleCommandPayload& InCommand, bool bInDefenderDefending, float InWindupDuration)
{
	Attacker = InAttacker;
	Defender = InDefender;
	Command = InCommand;
	bDefenderDefending = bInDefenderDefending;
	WindupDuration = InWindupDuration;
}

void UBattleAction_ExecuteHit::Enter()
{
	Elapsed = 0.f;
	bResolved = false;
	// Phase 3：在此触发攻击动画 / SkillComponent.TryActivateSkill()
}

void UBattleAction_ExecuteHit::Tick(float DeltaSeconds)
{
	if (bActionFinished) return;
	Elapsed += DeltaSeconds;
	if (!bResolved && Elapsed >= WindupDuration)
	{
		ResolveDamage();
		bResolved = true;
		FinishAction();
	}
}

void UBattleAction_ExecuteHit::ResolveDamage()
{
	ACharacter* A = Attacker.Get();
	ACharacter* D = Defender.Get();
	if (!A || !D) return;

	UAttributeComponent* AttackerAttr = BA_GetAttr(A);
	UAttributeComponent* DefenderAttr = BA_GetAttr(D);
	if (!AttackerAttr || !DefenderAttr) return;

	int32 Damage = 0;
	if (Command.Command == EBattleCommand::Skill)
	{
		Damage = FBattleDamageCalculator::CalcSkillDamage(Command.SkillPower, AttackerAttr);
	}
	else
	{
		Damage = FBattleDamageCalculator::CalcMeleeDamage(AttackerAttr, DefenderAttr);
	}

	// 受击方防御中减伤
	if (bDefenderDefending)
	{
		Damage = FBattleDamageCalculator::ApplyDefendingMod(Damage, true);
	}

	const int32 Actual = FBattleDamageCalculator::ApplyDamage(DefenderAttr, Damage);
	UE_LOG(LogSkillRuntime, Log,
		TEXT("[Battle] %s %s causes %d damage to %s%s (HP=%.0f)"),
		*BA_CharName(A),
		Command.Command == EBattleCommand::Skill ? TEXT("skill") : TEXT("attack"),
		Actual, *BA_CharName(D),
		bDefenderDefending ? TEXT(" [DEFENDED]") : TEXT(""),
		DefenderAttr->GetFinalValue(BattleAttr::HP));

	// 广播命中事件(AiGame 层订阅 → 屏幕消息)
	if (SM)
	{
		SM->OnBattleHit.Broadcast(A, D, Actual, FBattleDamageCalculator::IsDead(DefenderAttr));
	}
}

// ============================================
// UBattleAction_CastSkill
// ============================================

void UBattleAction_CastSkill::Setup(ACharacter* InCaster, const FSkillEffectPlan& InPlan,
	const FSkillTargetContext& InCtx, const FString& InCastClip, float InWindupDuration, float InTotalDuration)
{
	Caster = InCaster;
	Plan = InPlan;
	Ctx = InCtx;
	CastClip = InCastClip.IsEmpty() ? TEXT("attack") : InCastClip;
	WindupDuration = InWindupDuration;
	TotalDuration = FMath::Max(InTotalDuration, InWindupDuration + 0.1f);
}

void UBattleAction_CastSkill::Enter()
{
	Elapsed = 0.f;
	bResolved = false;
	// 施法动画(2D clip;非 2D 角色为 no-op),并按 clip 真实时长对齐演出(避免循环 clip 在固定窗口里多播)
	if (ISkillAnim2DReceiver* R = BA_GetAnimReceiver(Caster.Get()))
	{
		R->SkillAnim_PlayState(CastClip);
		const float Dur = R->SkillAnim_GetCurrentClipDurationSeconds();
		if (Dur > 0.05f)
		{
			TotalDuration  = FMath::Max(Dur, 0.2f);                       // 播放一个完整周期
			WindupDuration = FMath::Clamp(Dur * 0.5f, 0.1f, TotalDuration - 0.02f);  // 周期中点结算
		}
	}
	UE_LOG(LogSkillRuntime, Log, TEXT("[CastSkill] %s 施法 '%s' clip=%s total=%.2f windup=%.2f"),
		*BA_CharName(Caster.Get()), *Plan.Name, *CastClip, TotalDuration, WindupDuration);
}

void UBattleAction_CastSkill::Tick(float DeltaSeconds)
{
	if (bActionFinished) return;
	Elapsed += DeltaSeconds;

	// 蓄力到点:结算效果(判定→目标集→施加效果组件)
	if (!bResolved && Elapsed >= WindupDuration)
	{
		FSkillEffectResolver::ResolveAndApply(Plan, Ctx, SM);
		bResolved = true;
	}

	// 演出结束:切回 idle,完成
	if (Elapsed >= TotalDuration)
	{
		if (ISkillAnim2DReceiver* R = BA_GetAnimReceiver(Caster.Get()))
		{
			R->SkillAnim_PlayState(TEXT("idle"));
		}
		FinishAction();
	}
}

// ============================================
// UBattleAction_ReturnHome
// ============================================

void UBattleAction_ReturnHome::Setup(ACharacter* InMover, FVector InHomeLocation, float InDuration)
{
	Mover = InMover;
	HomeLocation = InHomeLocation;
	Duration = InDuration;
}

void UBattleAction_ReturnHome::Enter()
{
	Elapsed = 0.f;
	// Phase 2：记录当前位置，Tick 内 lerp 回 HomeLocation
}

void UBattleAction_ReturnHome::Tick(float DeltaSeconds)
{
	if (bActionFinished) return;
	Elapsed += DeltaSeconds;
	if (Elapsed >= Duration)
	{
		FinishAction();
	}
}
