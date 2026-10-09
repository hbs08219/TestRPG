// UCombatStatusComponent 实现

#include "Components/CombatStatusComponent.h"
#include "Components/AttributeComponent.h"
#include "BattleDamageCalculator.h"
#include "AiGameSkillRuntime.h"

UCombatStatusComponent::UCombatStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveAttr();
}

UAttributeComponent* UCombatStatusComponent::ResolveAttr()
{
	if (AttrComp) return AttrComp;
	if (const AActor* Owner = GetOwner())
	{
		AttrComp = Owner->FindComponentByClass<UAttributeComponent>();
	}
	return AttrComp;
}

void UCombatStatusComponent::ApplyEffect(const FEffectComponent& Effect, UAttributeComponent* CasterAttr, FName Source)
{
	UAttributeComponent* Attr = ResolveAttr();
	if (!Attr) return;

	switch (Effect.Type)
	{
	case EEffectType::Damage:
	{
		const int32 Dmg = FBattleDamageCalculator::CalcSkillDamage(Effect.Power, CasterAttr);
		FBattleDamageCalculator::ApplyDamage(Attr, Dmg);
		break;
	}
	case EEffectType::Heal:
	{
		const int32 Heal = FBattleDamageCalculator::CalcHealing(Effect.Amount, CasterAttr);
		FBattleDamageCalculator::ApplyHealing(Attr, Heal);
		break;
	}
	case EEffectType::Buff:
	{
		if (Effect.Attr.IsNone()) break;
		// flat → Add(直接加);percent → Multiply(存分数,0.3=+30%)
		const EAttributeOp Op = Effect.bPercent ? EAttributeOp::Multiply : EAttributeOp::Add;
		const float Val = Effect.bPercent ? (Effect.Value / 100.f) : Effect.Value;
		Attr->AddTimedModifier(Source, Effect.Attr, Val, Op, Effect.Duration, Effect.MaxStacks);
		break;
	}
	case EEffectType::Control:
	{
		if (!Effect.Control.IsNone())
		{
			AddTag(Effect.Control, Effect.Duration);
		}
		break;
	}
	case EEffectType::Dot:
	{
		const int32 MaxStacks = FMath::Max(1, Effect.MaxStacks);
		const int32 Turns = FMath::Max(1, Effect.Duration);
		// 同来源叠层 + 刷新时效
		for (FActiveDot& D : Dots)
		{
			if (D.Source == Source)
			{
				D.Stacks = FMath::Min(MaxStacks, FMath::Max(1, D.Stacks) + 1);
				D.DamagePerTurn = Effect.DamagePerTurn;
				D.RemainingTurns = Turns;
				return;
			}
		}
		FActiveDot NewDot;
		NewDot.Source = Source;
		NewDot.DamagePerTurn = Effect.DamagePerTurn;
		NewDot.RemainingTurns = Turns;
		NewDot.Stacks = 1;
		Dots.Add(NewDot);
		break;
	}
	}
}

FCombatTurnTickResult UCombatStatusComponent::TickTurnStart()
{
	FCombatTurnTickResult Result;
	UAttributeComponent* Attr = ResolveAttr();

	// ① DoT 扣血(先于行动;按层计)
	if (Attr)
	{
		int32 Total = 0;
		for (const FActiveDot& D : Dots)
		{
			Total += D.DamagePerTurn * FMath::Max(1, D.Stacks);
		}
		if (Total > 0)
		{
			Result.PoisonDamage = FBattleDamageCalculator::ApplyDamage(Attr, Total);
			Result.bDiedFromPoison = FBattleDamageCalculator::IsDead(Attr);
		}
	}

	// ② 时效递减:DoT
	for (int32 i = Dots.Num() - 1; i >= 0; --i)
	{
		if (--Dots[i].RemainingTurns <= 0)
		{
			Dots.RemoveAtSwap(i);
		}
	}

	// ② 时效递减:控制 tag
	for (auto It = TagTurns.CreateIterator(); It; ++It)
	{
		if (--It.Value() <= 0)
		{
			It.RemoveCurrent();
		}
	}

	// ② 时效递减:属性 timed modifier
	if (Attr)
	{
		Attr->TickModifierTurns();
	}

	// ③ 递减后仍存在的控制 → gating
	Result.bStunned = IsStunned();
	Result.bSilenced = IsSilenced();
	return Result;
}

void UCombatStatusComponent::AddTag(FName Tag, int32 Turns)
{
	if (Tag.IsNone()) return;
	Turns = FMath::Max(1, Turns);
	int32& Cur = TagTurns.FindOrAdd(Tag);
	Cur = FMath::Max(Cur, Turns);  // 同 tag 取较长时效(刷新)
}

bool UCombatStatusComponent::HasTag(FName Tag) const
{
	const int32* P = TagTurns.Find(Tag);
	return P && *P > 0;
}

void UCombatStatusComponent::ClearAll()
{
	TagTurns.Empty();
	Dots.Empty();
}

int32 UCombatStatusComponent::GetDotStacks() const
{
	int32 Total = 0;
	for (const FActiveDot& D : Dots) Total += FMath::Max(1, D.Stacks);
	return Total;
}

FString UCombatStatusComponent::GetStatusSummary() const
{
	TArray<FString> Parts;

	// 属性 buff/debuff(timed modifier)
	if (AttrComp)
	{
		auto Short = [](FName Attr) -> FString
		{
			const FString S = Attr.ToString();
			if (S == TEXT("ATK"))  return TEXT("攻");
			if (S == TEXT("DEF"))  return TEXT("防");
			if (S == TEXT("MATK")) return TEXT("法攻");
			if (S == TEXT("MDEF")) return TEXT("法防");
			if (S == TEXT("SPD"))  return TEXT("速");
			if (S == TEXT("LUK"))  return TEXT("运");
			return S;
		};
		for (const FAttributeModifier& M : AttrComp->GetTimedModifiers())
		{
			const TCHAR* Arrow = (M.Value < 0.f) ? TEXT("↓") : TEXT("↑");
			Parts.Add(FString::Printf(TEXT("%s%s%d"), *Short(M.AttributeName), Arrow, M.RemainingTurns));
		}
	}

	// DoT(毒)
	for (const FActiveDot& D : Dots)
	{
		if (D.Stacks > 1)
			Parts.Add(FString::Printf(TEXT("毒x%d %d"), D.Stacks, D.RemainingTurns));
		else
			Parts.Add(FString::Printf(TEXT("毒%d"), D.RemainingTurns));
	}

	// 控制 tag(晕/默)
	for (const auto& KV : TagTurns)
	{
		if (KV.Value <= 0) continue;
		if (KV.Key == CombatTag::Stunned)       Parts.Add(FString::Printf(TEXT("晕%d"), KV.Value));
		else if (KV.Key == CombatTag::Silenced) Parts.Add(FString::Printf(TEXT("默%d"), KV.Value));
		else                                    Parts.Add(FString::Printf(TEXT("%s%d"), *KV.Key.ToString(), KV.Value));
	}

	return FString::Join(Parts, TEXT(" "));
}
