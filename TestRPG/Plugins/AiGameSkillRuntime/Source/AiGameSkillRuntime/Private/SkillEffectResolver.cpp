// FSkillEffectResolver 实现

#include "SkillEffectResolver.h"
#include "Components/AttributeComponent.h"
#include "Components/CombatStatusComponent.h"
#include "BattleDamageCalculator.h"
#include "BattleStateMachine.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	UAttributeComponent* SR_GetAttr(ACharacter* C)
	{
		return C ? C->FindComponentByClass<UAttributeComponent>() : nullptr;
	}

	bool SR_IsAlive(ACharacter* C)
	{
		UAttributeComponent* A = SR_GetAttr(C);
		return A && !FBattleDamageCalculator::IsDead(A);
	}

	/** 从一个 TimelineAction 节点的 data 收集 hittest / effect 轨道事件到 Plan */
	void CollectTracksFromNode(const TSharedPtr<FJsonObject>& Data, FSkillEffectPlan& Plan)
	{
		const TArray<TSharedPtr<FJsonValue>>* Tracks = nullptr;
		if (!Data->TryGetArrayField(TEXT("tracks"), Tracks) || !Tracks) return;

		for (const TSharedPtr<FJsonValue>& TV : *Tracks)
		{
			const TSharedPtr<FJsonObject> Track = TV->AsObject();
			if (!Track.IsValid()) continue;

			FString TypeId;
			Track->TryGetStringField(TEXT("typeId"), TypeId);

			const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
			if (!Track->TryGetArrayField(TEXT("events"), Events) || !Events) continue;

			for (const TSharedPtr<FJsonValue>& EV : *Events)
			{
				const TSharedPtr<FJsonObject> Ev = EV->AsObject();
				if (!Ev.IsValid()) continue;
				const TSharedPtr<FJsonObject>* Props = nullptr;
				if (!Ev->TryGetObjectField(TEXT("properties"), Props) || !Props) continue;

				if (TypeId == TEXT("sprite2d"))
				{
					// 施法动画:取第一条 sprite2d 事件的 clip2d
					if (Plan.CastClip.IsEmpty())
					{
						(*Props)->TryGetStringField(TEXT("clip2d"), Plan.CastClip);
					}
				}
				else if (TypeId == TEXT("hittest"))
				{
					FResolvedHitTest H;
					(*Props)->TryGetStringField(TEXT("hitId"), H.HitId);
					(*Props)->TryGetStringField(TEXT("scope"), H.Scope);
					(*Props)->TryGetStringField(TEXT("faction"), H.Faction);
					if (H.HitId.IsEmpty()) H.HitId = TEXT("H1");
					if (H.Scope.IsEmpty()) H.Scope = TEXT("single");
					if (H.Faction.IsEmpty()) H.Faction = TEXT("enemy");
					Plan.HitTests.Add(MoveTemp(H));
				}
				else if (TypeId == TEXT("effect"))
				{
					FResolvedEffectEvent E;
					(*Props)->TryGetStringField(TEXT("targetRef"), E.TargetRef);
					if (E.TargetRef.IsEmpty()) E.TargetRef = TEXT("H1");
					if (FEffectComponent::FromJsonProps(*Props, E.Effect))
					{
						Plan.Effects.Add(MoveTemp(E));
					}
				}
			}
		}
	}

	/** 在一组 nodes 里找所有 TimelineAction 节点并收集轨道。返回是否找到至少一个。 */
	bool CollectFromNodes(const TArray<TSharedPtr<FJsonValue>>* Nodes, FSkillEffectPlan& Plan)
	{
		if (!Nodes) return false;
		bool bFound = false;
		for (const TSharedPtr<FJsonValue>& NV : *Nodes)
		{
			const TSharedPtr<FJsonObject> Node = NV->AsObject();
			if (!Node.IsValid()) continue;
			const TSharedPtr<FJsonObject>* Data = nullptr;
			if (!Node->TryGetObjectField(TEXT("data"), Data) || !Data) continue;

			FString NodeKind, ActionType;
			(*Data)->TryGetStringField(TEXT("nodeKind"), NodeKind);
			(*Data)->TryGetStringField(TEXT("actionType"), ActionType);
			if (NodeKind == TEXT("action") && ActionType == TEXT("TimelineAction"))
			{
				CollectTracksFromNode(*Data, Plan);
				bFound = true;
			}
		}
		return bFound;
	}
}

bool FSkillEffectResolver::LoadPlan(const FString& SkillPath, FSkillEffectPlan& Out)
{
	Out = FSkillEffectPlan();

	const FString FullPath = FPaths::ProjectContentDir() / TEXT("Data/ToolGen/Skill") / (SkillPath + TEXT(".json"));
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *FullPath))
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("[SkillResolver] 技能文件未找到: %s"), *FullPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("[SkillResolver] 技能 JSON 解析失败: %s"), *SkillPath);
		return false;
	}

	Root->TryGetStringField(TEXT("name"), Out.Name);
	if (Out.Name.IsEmpty()) Out.Name = SkillPath;

	// battle 配置(数据页)
	const TSharedPtr<FJsonObject>* Battle = nullptr;
	if (Root->TryGetObjectField(TEXT("battle"), Battle) && Battle)
	{
		Out.bHasBattle = true;
		(*Battle)->TryGetStringField(TEXT("targetType"), Out.TargetType);
		double Mp = 0.0;
		if ((*Battle)->TryGetNumberField(TEXT("mpCost"), Mp)) Out.MpCost = static_cast<int32>(Mp);

		auto ReadTags = [&](const TCHAR* Field, TArray<FString>& OutArr)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if ((*Battle)->TryGetArrayField(Field, Arr) && Arr)
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					FString S;
					if (V->TryGetString(S) && !S.IsEmpty()) OutArr.Add(S);
				}
			}
		};
		ReadTags(TEXT("requireTags"), Out.RequireTags);
		ReadTags(TEXT("blockTags"), Out.BlockTags);
	}

	// 逻辑页时间线:优先读顶层 graph.nodes;没有 TimelineAction 再回退 stateMachine.states[*].flow.nodes。
	// (二者通常是同一份;只取一处避免效果被重复施加。)
	bool bFound = false;
	const TSharedPtr<FJsonObject>* Graph = nullptr;
	if (Root->TryGetObjectField(TEXT("graph"), Graph) && Graph)
	{
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		if ((*Graph)->TryGetArrayField(TEXT("nodes"), Nodes))
		{
			bFound = CollectFromNodes(Nodes, Out);
		}
	}
	if (!bFound)
	{
		const TSharedPtr<FJsonObject>* SM = nullptr;
		if (Root->TryGetObjectField(TEXT("stateMachine"), SM) && SM)
		{
			const TArray<TSharedPtr<FJsonValue>>* States = nullptr;
			if ((*SM)->TryGetArrayField(TEXT("states"), States) && States)
			{
				for (const TSharedPtr<FJsonValue>& SV : *States)
				{
					const TSharedPtr<FJsonObject> St = SV->AsObject();
					if (!St.IsValid()) continue;
					const TSharedPtr<FJsonObject>* Flow = nullptr;
					if (!St->TryGetObjectField(TEXT("flow"), Flow) || !Flow) continue;
					const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
					if ((*Flow)->TryGetArrayField(TEXT("nodes"), Nodes))
					{
						if (CollectFromNodes(Nodes, Out)) { bFound = true; break; }  // 只取第一处
					}
				}
			}
		}
	}

	UE_LOG(LogSkillRuntime, Log, TEXT("[SkillResolver] 计划 '%s': %d 判定 / %d 效果 (mp=%d target=%s)"),
		*Out.Name, Out.HitTests.Num(), Out.Effects.Num(), Out.MpCost, *Out.TargetType);
	return true;
}

bool FSkillEffectResolver::CanCast(const FSkillEffectPlan& Plan, const UCombatStatusComponent* CasterStatus, float CurMP, FString& OutReason)
{
	if (CasterStatus && CasterStatus->IsSilenced())   // 沉默禁全部
	{
		OutReason = TEXT("沉默");
		return false;
	}
	if (Plan.MpCost > CurMP)
	{
		OutReason = TEXT("MP不足");
		return false;
	}
	for (const FString& Rt : Plan.RequireTags)         // 须全具备
	{
		if (!CasterStatus || !CasterStatus->HasTag(FName(*Rt)))
		{
			OutReason = TEXT("条件不足");
			return false;
		}
	}
	for (const FString& Bt : Plan.BlockTags)           // 须全不具备
	{
		if (CasterStatus && CasterStatus->HasTag(FName(*Bt)))
		{
			OutReason = TEXT("被限制");
			return false;
		}
	}
	OutReason.Reset();
	return true;
}

UCombatStatusComponent* FSkillEffectResolver::GetOrCreateStatus(ACharacter* Char)
{
	if (!Char) return nullptr;
	if (UCombatStatusComponent* Existing = Char->FindComponentByClass<UCombatStatusComponent>())
	{
		return Existing;
	}
	UCombatStatusComponent* CS = NewObject<UCombatStatusComponent>(Char);
	if (!CS) return nullptr;
	CS->RegisterComponent();
	CS->SetAttributeComponent(SR_GetAttr(Char));
	return CS;
}

TArray<ACharacter*> FSkillEffectResolver::ResolveTargets(const FResolvedHitTest& Hit, const FSkillTargetContext& Ctx)
{
	// 阵营集合
	TArray<ACharacter*> Faction;
	if (Hit.Faction == TEXT("self"))
	{
		if (Ctx.Caster) Faction.Add(Ctx.Caster);
	}
	else if (Hit.Faction == TEXT("ally"))
	{
		Faction = Ctx.Allies;
	}
	else // enemy(默认)
	{
		Faction = Ctx.Enemies;
	}

	// 过滤存活
	Faction.RemoveAll([](ACharacter* C) { return !SR_IsAlive(C); });

	TArray<ACharacter*> Out;
	if (Hit.Scope == TEXT("all") || Hit.Scope == TEXT("row"))  // row 暂同 all(无站位分排)
	{
		Out = Faction;
	}
	else // single
	{
		if (Ctx.SelectedTarget && SR_IsAlive(Ctx.SelectedTarget) && Faction.Contains(Ctx.SelectedTarget))
		{
			Out.Add(Ctx.SelectedTarget);
		}
		else if (Faction.Num() > 0)
		{
			Out.Add(Faction[0]);
		}
	}
	return Out;
}

void FSkillEffectResolver::ResolveAndApply(const FSkillEffectPlan& Plan, const FSkillTargetContext& Ctx, UBattleStateMachine* SM)
{
	UAttributeComponent* CasterAttr = SR_GetAttr(Ctx.Caster);
	const FName Source(*Plan.Name);

	for (const FResolvedEffectEvent& E : Plan.Effects)
	{
		// 找此效果引用的判定
		const FResolvedHitTest* Hit = Plan.HitTests.FindByPredicate(
			[&E](const FResolvedHitTest& H) { return H.HitId == E.TargetRef; });
		if (!Hit)
		{
			UE_LOG(LogSkillRuntime, Warning, TEXT("[SkillResolver] 效果 targetRef='%s' 无对应判定,跳过"), *E.TargetRef);
			continue;
		}

		const TArray<ACharacter*> Targets = ResolveTargets(*Hit, Ctx);
		for (ACharacter* T : Targets)
		{
			UCombatStatusComponent* CS = GetOrCreateStatus(T);
			if (!CS) continue;

			if (E.Effect.Type == EEffectType::Damage)
			{
				// 前后 HP 差 → 真实伤害,驱动 OnBattleHit(屏幕提示/死亡判定)
				UAttributeComponent* TAttr = SR_GetAttr(T);
				const float Before = TAttr ? TAttr->GetFinalValue(BattleAttr::HP) : 0.f;
				CS->ApplyEffect(E.Effect, CasterAttr, Source);
				const float After = TAttr ? TAttr->GetFinalValue(BattleAttr::HP) : 0.f;
				const int32 Dealt = FMath::Max(0, FMath::RoundToInt(Before - After));
				if (SM)
				{
					SM->OnBattleHit.Broadcast(Ctx.Caster, T, Dealt,
						TAttr ? FBattleDamageCalculator::IsDead(TAttr) : false);
				}
			}
			else
			{
				CS->ApplyEffect(E.Effect, CasterAttr, Source);
			}
		}
	}
}
