// FEffectComponent 实现

#include "BattleEffect.h"
#include "Dom/JsonObject.h"

namespace CombatTag
{
	const FName Stunned(TEXT("Stunned"));
	const FName Silenced(TEXT("Silenced"));
}

bool FEffectComponent::FromJsonProps(const TSharedPtr<FJsonObject>& Props, FEffectComponent& Out)
{
	if (!Props.IsValid()) return false;

	FString TypeStr;
	if (!Props->TryGetStringField(TEXT("type"), TypeStr)) return false;

	Out = FEffectComponent();

	auto GetNum = [&Props](const TCHAR* Field, double Default) -> double
	{
		double V = Default;
		Props->TryGetNumberField(Field, V);
		return V;
	};

	if (TypeStr == TEXT("damage"))
	{
		Out.Type = EEffectType::Damage;
		Out.Power = static_cast<int32>(GetNum(TEXT("power"), 0));
	}
	else if (TypeStr == TEXT("heal"))
	{
		Out.Type = EEffectType::Heal;
		Out.Amount = static_cast<int32>(GetNum(TEXT("amount"), 0));
	}
	else if (TypeStr == TEXT("buff"))
	{
		Out.Type = EEffectType::Buff;
		FString AttrStr;
		Props->TryGetStringField(TEXT("attr"), AttrStr);
		Out.Attr = AttrStr.IsEmpty() ? NAME_None : FName(*AttrStr);
		FString ModeStr;
		Props->TryGetStringField(TEXT("mode"), ModeStr);
		Out.bPercent = (ModeStr == TEXT("percent"));
		Out.Value = static_cast<float>(GetNum(TEXT("value"), 0));
		Out.Duration = static_cast<int32>(GetNum(TEXT("duration"), 1));
		Out.MaxStacks = FMath::Max(1, static_cast<int32>(GetNum(TEXT("maxStacks"), 1)));
	}
	else if (TypeStr == TEXT("control"))
	{
		Out.Type = EEffectType::Control;
		FString CtrlStr;
		Props->TryGetStringField(TEXT("control"), CtrlStr);
		Out.Control = (CtrlStr == TEXT("silence")) ? CombatTag::Silenced : CombatTag::Stunned;
		Out.Duration = static_cast<int32>(GetNum(TEXT("duration"), 1));
		Out.MaxStacks = FMath::Max(1, static_cast<int32>(GetNum(TEXT("maxStacks"), 1)));
	}
	else if (TypeStr == TEXT("dot"))
	{
		Out.Type = EEffectType::Dot;
		Out.DamagePerTurn = static_cast<int32>(GetNum(TEXT("damagePerTurn"), 0));
		Out.Duration = static_cast<int32>(GetNum(TEXT("duration"), 1));
		Out.MaxStacks = FMath::Max(1, static_cast<int32>(GetNum(TEXT("maxStacks"), 1)));
	}
	else
	{
		return false;
	}
	return true;
}
