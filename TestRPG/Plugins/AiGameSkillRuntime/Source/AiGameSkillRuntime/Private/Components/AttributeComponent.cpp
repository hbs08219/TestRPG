// UAttributeComponent 实现

#include "Components/AttributeComponent.h"
#include "AiGameSkillRuntime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// =============================================================================
// JSON 加载
// =============================================================================

bool UAttributeComponent::LoadFromFile(const FString& RelPath)
{
	const FString FullPath = FPaths::ProjectContentDir() / TEXT("Data/ToolGen/Attributes") / RelPath;
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("AttributeComponent: failed to load %s"), *FullPath);
		return false;
	}
	return LoadFromJsonString(JsonString);
}

bool UAttributeComponent::LoadFromJsonString(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("AttributeComponent: invalid JSON"));
		return false;
	}

	// 兼容 v1（"attributes"）和 v2（"builtinAttributes" + "customAttributes"）
	auto ReadAttrGroup = [&](const TCHAR* FieldName)
	{
		const TSharedPtr<FJsonObject>* GroupObj = nullptr;
		if (!Root->TryGetObjectField(FieldName, GroupObj)) return;

		for (const auto& KV : (*GroupObj)->Values)
		{
			const FName AttrName(*KV.Key);
			const TSharedPtr<FJsonObject> Obj = KV.Value->AsObject();
			if (!Obj.IsValid()) continue;

			double BaseValue = 0.0;
			if (Obj->TryGetNumberField(TEXT("baseValue"), BaseValue))
			{
				SetBaseValue(AttrName, static_cast<float>(BaseValue));
			}
		}
	};

	// v2 优先：分别读 builtin 和 custom
	ReadAttrGroup(TEXT("builtinAttributes"));
	ReadAttrGroup(TEXT("customAttributes"));

	// v1 兼容：如果上面都没读到东西，回退读 "attributes"
	if (BaseValues.Num() == 0)
	{
		ReadAttrGroup(TEXT("attributes"));
	}

	if (BaseValues.Num() == 0)
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("AttributeComponent: no attributes found (tried builtinAttributes/customAttributes/attributes)"));
		return false;
	}

	// 自动补上限属性：模板未显式写 Max* 时，令 MaxHP=HP / MaxMP=MP / MaxSP=SP。
	// 各属性模板目前只写"当前值"(HP/MP/SP)，但 HUD 显示与战斗回血上限钳制(BattleDamageCalculator)都读 Max*。
	auto EnsureMax = [this](const FName Cur, const FName Max)
	{
		const float* CurVal = BaseValues.Find(Cur);
		if (CurVal && !BaseValues.Contains(Max))
		{
			SetBaseValue(Max, *CurVal);
		}
	};
	EnsureMax(TEXT("HP"), TEXT("MaxHP"));
	EnsureMax(TEXT("MP"), TEXT("MaxMP"));
	EnsureMax(TEXT("SP"), TEXT("MaxSP"));

	// 成长曲线(可选):"growthCurves": { "HP": [ {"level":N,"value":V}, ... ], ... }
	// 仅缓存,不立即生效;由 ApplyLevel(等级) 按曲线插值套用。
	GrowthCurves.Reset();
	const TSharedPtr<FJsonObject>* CurvesObj = nullptr;
	if (Root->TryGetObjectField(TEXT("growthCurves"), CurvesObj))
	{
		for (const auto& KV : (*CurvesObj)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (!KV.Value->TryGetArray(Arr) || !Arr) continue;

			TArray<FGrowthKeyframe> Keys;
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) continue;
				double L = 1.0, Val = 0.0;
				O->TryGetNumberField(TEXT("level"), L);
				O->TryGetNumberField(TEXT("value"), Val);
				FGrowthKeyframe K;
				K.Level = FMath::Max(1, static_cast<int32>(L));
				K.Value = static_cast<float>(Val);
				Keys.Add(K);
			}
			Keys.Sort([](const FGrowthKeyframe& A, const FGrowthKeyframe& B) { return A.Level < B.Level; });
			if (Keys.Num() > 0)
			{
				GrowthCurves.Add(FName(*KV.Key), MoveTemp(Keys));
			}
		}
	}

	UE_LOG(LogSkillRuntime, Log, TEXT("AttributeComponent: loaded %d attributes"), BaseValues.Num());
	return true;
}

// =============================================================================
// Base 值
// =============================================================================

void UAttributeComponent::SetBaseValue(FName Attr, float Value)
{
	BaseValues.Add(Attr, Value);
	RecomputeAndBroadcast(Attr);
}

float UAttributeComponent::GetBaseValue(FName Attr) const
{
	const float* P = BaseValues.Find(Attr);
	return P ? *P : 0.f;
}

bool UAttributeComponent::HasAttribute(FName Attr) const
{
	return BaseValues.Contains(Attr);
}

void UAttributeComponent::ApplyLevel(int32 Level)
{
	Level = FMath::Max(1, Level);

	const FName NHP(TEXT("HP")), NMP(TEXT("MP")), NSP(TEXT("SP"));

	for (const auto& KV : GrowthCurves)
	{
		const TArray<FGrowthKeyframe>& Keys = KV.Value;
		if (Keys.Num() == 0) continue;

		// 按等级线性插值(夹在首尾关键帧之间)
		float Value;
		if (Level <= Keys[0].Level)
		{
			Value = Keys[0].Value;
		}
		else if (Level >= Keys.Last().Level)
		{
			Value = Keys.Last().Value;
		}
		else
		{
			Value = Keys.Last().Value;
			for (int32 i = 1; i < Keys.Num(); ++i)
			{
				if (Level <= Keys[i].Level)
				{
					const FGrowthKeyframe& A = Keys[i - 1];
					const FGrowthKeyframe& B = Keys[i];
					const float T = (B.Level == A.Level) ? 0.f
						: static_cast<float>(Level - A.Level) / static_cast<float>(B.Level - A.Level);
					Value = FMath::Lerp(A.Value, B.Value, T);
					break;
				}
			}
		}

		SetBaseValue(KV.Key, Value);  // 广播 OnAttributeChanged

		// HP/MP/SP 的曲线代表该等级上限:同步 Max* 并令当前值=满
		if      (KV.Key == NHP) SetBaseValue(TEXT("MaxHP"), Value);
		else if (KV.Key == NMP) SetBaseValue(TEXT("MaxMP"), Value);
		else if (KV.Key == NSP) SetBaseValue(TEXT("MaxSP"), Value);
	}

	UE_LOG(LogSkillRuntime, Log, TEXT("AttributeComponent: ApplyLevel(%d) → %d 条成长曲线"), Level, GrowthCurves.Num());
}

// =============================================================================
// Modifier 管理
// =============================================================================

void UAttributeComponent::AddModifier(FName Source, FName Attr, float Value, EAttributeOp Op)
{
	if (Source.IsNone() || Attr.IsNone()) return;

	// 同 Source+Attr+Op 覆盖（避免每帧重复加 Add 之类的 bug）
	for (FAttributeModifier& M : Modifiers)
	{
		if (M.Source == Source && M.AttributeName == Attr && M.Op == Op)
		{
			M.Value = Value;
			RecomputeAndBroadcast(Attr);
			return;
		}
	}

	FAttributeModifier NewMod;
	NewMod.Source        = Source;
	NewMod.AttributeName = Attr;
	NewMod.Value         = Value;
	NewMod.Op            = Op;
	Modifiers.Add(NewMod);

	RecomputeAndBroadcast(Attr);
}

void UAttributeComponent::AddTimedModifier(FName Source, FName Attr, float Value, EAttributeOp Op, int32 Turns, int32 MaxStacks)
{
	if (Source.IsNone() || Attr.IsNone()) return;
	Turns = FMath::Max(1, Turns);
	MaxStacks = FMath::Max(1, MaxStacks);

	// 同 Source+Attr+Op → 叠层 + 刷新数值与时效
	for (FAttributeModifier& M : Modifiers)
	{
		if (M.Source == Source && M.AttributeName == Attr && M.Op == Op)
		{
			M.Stacks = FMath::Min(MaxStacks, FMath::Max(1, M.Stacks) + 1);
			M.Value = Value;
			M.RemainingTurns = Turns;
			RecomputeAndBroadcast(Attr);
			return;
		}
	}

	FAttributeModifier NewMod;
	NewMod.Source         = Source;
	NewMod.AttributeName  = Attr;
	NewMod.Value          = Value;
	NewMod.Op             = Op;
	NewMod.RemainingTurns = Turns;
	NewMod.Stacks         = 1;
	Modifiers.Add(NewMod);

	RecomputeAndBroadcast(Attr);
}

void UAttributeComponent::TickModifierTurns()
{
	TSet<FName> AffectedAttrs;
	for (int32 i = Modifiers.Num() - 1; i >= 0; --i)
	{
		FAttributeModifier& M = Modifiers[i];
		if (M.RemainingTurns < 0) continue;  // 永久,不递减
		--M.RemainingTurns;
		if (M.RemainingTurns <= 0)
		{
			AffectedAttrs.Add(M.AttributeName);
			Modifiers.RemoveAtSwap(i);
		}
	}
	for (const FName& Attr : AffectedAttrs)
	{
		RecomputeAndBroadcast(Attr);
	}
}

TArray<FAttributeModifier> UAttributeComponent::GetTimedModifiers() const
{
	TArray<FAttributeModifier> Out;
	for (const FAttributeModifier& M : Modifiers)
	{
		if (M.RemainingTurns > 0) Out.Add(M);
	}
	return Out;
}

void UAttributeComponent::RemoveModifiersBySource(FName Source)
{
	if (Source.IsNone()) return;

	TSet<FName> AffectedAttrs;
	for (int32 i = Modifiers.Num() - 1; i >= 0; --i)
	{
		if (Modifiers[i].Source == Source)
		{
			AffectedAttrs.Add(Modifiers[i].AttributeName);
			Modifiers.RemoveAtSwap(i);
		}
	}
	for (const FName& Attr : AffectedAttrs)
	{
		RecomputeAndBroadcast(Attr);
	}
}

void UAttributeComponent::RemoveModifiersBySourceAndAttr(FName Source, FName Attr)
{
	if (Source.IsNone() || Attr.IsNone()) return;

	bool bAny = false;
	for (int32 i = Modifiers.Num() - 1; i >= 0; --i)
	{
		if (Modifiers[i].Source == Source && Modifiers[i].AttributeName == Attr)
		{
			Modifiers.RemoveAtSwap(i);
			bAny = true;
		}
	}
	if (bAny) RecomputeAndBroadcast(Attr);
}

void UAttributeComponent::ClearAllModifiers()
{
	if (Modifiers.Num() == 0) return;

	TSet<FName> AffectedAttrs;
	for (const FAttributeModifier& M : Modifiers)
	{
		AffectedAttrs.Add(M.AttributeName);
	}
	Modifiers.Empty();
	for (const FName& Attr : AffectedAttrs)
	{
		RecomputeAndBroadcast(Attr);
	}
}

// =============================================================================
// 最终值计算
// =============================================================================

float UAttributeComponent::ComputeFinalValue(FName Attr) const
{
	const float Base = GetBaseValue(Attr);

	// Override 优先：取最后一条 Override（多个 Override 取最新）
	bool bHasOverride = false;
	float OverrideValue = 0.f;

	float SumAdd = 0.f;
	float SumMulDelta = 0.f;

	for (const FAttributeModifier& M : Modifiers)
	{
		if (M.AttributeName != Attr) continue;
		const float Eff = M.Value * static_cast<float>(FMath::Max(1, M.Stacks));  // 按层计入
		switch (M.Op)
		{
		case EAttributeOp::Add:
			SumAdd += Eff;
			break;
		case EAttributeOp::Multiply:
			SumMulDelta += Eff;
			break;
		case EAttributeOp::Override:
			bHasOverride = true;
			OverrideValue = M.Value;
			break;
		}
	}

	if (bHasOverride) return OverrideValue;
	return (Base + SumAdd) * (1.f + SumMulDelta);
}

float UAttributeComponent::GetFinalValue(FName Attr) const
{
	return ComputeFinalValue(Attr);
}

TMap<FName, float> UAttributeComponent::GetAllFinalValues() const
{
	TMap<FName, float> Out;
	Out.Reserve(BaseValues.Num());
	for (const auto& KV : BaseValues)
	{
		Out.Add(KV.Key, ComputeFinalValue(KV.Key));
	}
	return Out;
}

// =============================================================================
// 内部：广播变更
// =============================================================================

void UAttributeComponent::RecomputeAndBroadcast(FName Attr)
{
	const float NewValue = ComputeFinalValue(Attr);
	const float OldValue = CachedFinalValues.FindRef(Attr);

	if (!FMath::IsNearlyEqual(OldValue, NewValue))
	{
		CachedFinalValues.Add(Attr, NewValue);
		OnAttributeChanged.Broadcast(Attr, OldValue, NewValue);
	}
}

// =============================================================================
// 调试
// =============================================================================

void UAttributeComponent::DumpToLog() const
{
	UE_LOG(LogSkillRuntime, Log, TEXT("=== AttributeComponent Dump (%d attrs, %d mods) ==="),
		BaseValues.Num(), Modifiers.Num());

	for (const auto& KV : BaseValues)
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("  %s: base=%.2f, final=%.2f"),
			*KV.Key.ToString(), KV.Value, ComputeFinalValue(KV.Key));
	}
	for (const FAttributeModifier& M : Modifiers)
	{
		const TCHAR* OpName =
			M.Op == EAttributeOp::Add      ? TEXT("Add")      :
			M.Op == EAttributeOp::Multiply ? TEXT("Multiply") :
			                                 TEXT("Override");
		UE_LOG(LogSkillRuntime, Log, TEXT("  Modifier: src=%s attr=%s op=%s value=%.2f"),
			*M.Source.ToString(), *M.AttributeName.ToString(), OpName, M.Value);
	}
}
