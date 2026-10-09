// FBattleDamageCalculator 实现

#include "BattleDamageCalculator.h"
#include "Components/AttributeComponent.h"
#include "AiGameSkillRuntime.h"

namespace BattleAttr
{
	const FName HP     = FName(TEXT("HP"));
	const FName MaxHP  = FName(TEXT("MaxHP"));
	const FName MP     = FName(TEXT("MP"));
	const FName MaxMP  = FName(TEXT("MaxMP"));
	const FName ATK    = FName(TEXT("ATK"));
	const FName DEF    = FName(TEXT("DEF"));
	const FName SPD    = FName(TEXT("SPD"));
	const FName LUK    = FName(TEXT("LUK"));
	const FName Spirit = FName(TEXT("Spirit"));
}

namespace
{
	// 安全读取属性。组件为空 / 属性不存在 → 返回默认值
	float SafeGet(const UAttributeComponent* Attr, FName Name, float Default = 0.f)
	{
		if (!Attr) return Default;
		// HasAttribute 内部即遍历 BaseValues，不会创建新属性
		if (!Attr->HasAttribute(Name)) return Default;
		return Attr->GetFinalValue(Name);
	}
}

int32 FBattleDamageCalculator::CalcMeleeDamage(
	const UAttributeComponent* AttackerAttr,
	const UAttributeComponent* DefenderAttr)
{
	const float ATK = SafeGet(AttackerAttr, BattleAttr::ATK, 0.f);
	const float DEF = SafeGet(DefenderAttr, BattleAttr::DEF, 0.f);

	// max(1, ATK - DEF*0.5) — 设计保证至少有 1 点伤害，避免对峙
	const float Raw = ATK - DEF * 0.5f;
	const int32 Final = FMath::Max(1, FMath::FloorToInt(Raw));
	return Final;
}

int32 FBattleDamageCalculator::CalcSkillDamage(
	int32 SkillPower,
	const UAttributeComponent* CasterAttr)
{
	const float Spirit = SafeGet(CasterAttr, BattleAttr::Spirit, 0.f);
	const float Raw = static_cast<float>(SkillPower) + Spirit * 0.8f;
	return FMath::Max(1, FMath::FloorToInt(Raw));
}

int32 FBattleDamageCalculator::CalcHealing(
	int32 SkillHealAmount,
	const UAttributeComponent* CasterAttr)
{
	const float Spirit = SafeGet(CasterAttr, BattleAttr::Spirit, 0.f);
	const float Raw = static_cast<float>(SkillHealAmount) + Spirit * 0.5f;
	return FMath::Max(0, FMath::FloorToInt(Raw));
}

int32 FBattleDamageCalculator::ApplyDefendingMod(int32 IncomingDamage, bool bDefenderIsDefending)
{
	if (!bDefenderIsDefending) return IncomingDamage;
	// 防御减伤 50%，但保留至少 1 点（与普攻下限一致）
	return FMath::Max(1, FMath::FloorToInt(IncomingDamage * 0.5f));
}

int32 FBattleDamageCalculator::ApplyDamage(UAttributeComponent* DefenderAttr, int32 Damage)
{
	if (!DefenderAttr || Damage <= 0) return 0;
	if (!DefenderAttr->HasAttribute(BattleAttr::HP)) return 0;

	const float Current = DefenderAttr->GetFinalValue(BattleAttr::HP);
	const int32 ActualDamage = FMath::Min(static_cast<int32>(Current), Damage);
	const float NewHP = FMath::Max(0.f, Current - static_cast<float>(Damage));

	DefenderAttr->SetBaseValue(BattleAttr::HP, NewHP);

	const AActor* DOwner = DefenderAttr->GetOwner();
	UE_LOG(LogSkillRuntime, Log,
		TEXT("[Damage] %s HP %.0f → %.0f (-%d)"),
		DOwner ? *DOwner->GetName() : TEXT("<no-owner>"), Current, NewHP, ActualDamage);

	return ActualDamage;
}

int32 FBattleDamageCalculator::ApplyHealing(UAttributeComponent* TargetAttr, int32 HealAmount)
{
	if (!TargetAttr || HealAmount <= 0) return 0;
	if (!TargetAttr->HasAttribute(BattleAttr::HP)) return 0;

	const float Current = TargetAttr->GetFinalValue(BattleAttr::HP);
	// 死人不能回血（设计：复活机制留给 P1）
	if (Current <= 0.f) return 0;

	const float MaxHP = TargetAttr->HasAttribute(BattleAttr::MaxHP)
		? TargetAttr->GetFinalValue(BattleAttr::MaxHP)
		: Current + HealAmount; // 没有 MaxHP 配置则不封顶
	const float NewHP = FMath::Min(MaxHP, Current + static_cast<float>(HealAmount));
	const int32 ActualHeal = FMath::FloorToInt(NewHP - Current);

	TargetAttr->SetBaseValue(BattleAttr::HP, NewHP);

	const AActor* TOwner = TargetAttr->GetOwner();
	UE_LOG(LogSkillRuntime, Log,
		TEXT("[Heal] %s HP %.0f → %.0f (+%d)"),
		TOwner ? *TOwner->GetName() : TEXT("<no-owner>"), Current, NewHP, ActualHeal);

	return ActualHeal;
}

bool FBattleDamageCalculator::IsDead(const UAttributeComponent* TargetAttr)
{
	if (!TargetAttr) return true;
	if (!TargetAttr->HasAttribute(BattleAttr::HP)) return false;
	return TargetAttr->GetFinalValue(BattleAttr::HP) <= 0.f;
}
