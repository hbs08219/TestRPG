// USkillStateMachine 实现

#include "SkillStateMachine.h"
#include "AiGameSkillRuntime.h"
#include "SkillInstance.h"
#include "SkillFlow.h"
#include "SkillRuntimeSubsystem.h"
#include "Interfaces/ISkillAction.h"
#include "Interfaces/ISkillTrackHandler.h"
#include "Interfaces/ISkillListener.h"
#include "GameFramework/Character.h"

void USkillStateMachine::Initialize(USkillInstance* InInstance)
{
	Instance = InInstance;
	bFinished = false;

	UE_LOG(LogSkillRuntime, Log, TEXT("SkillStateMachine initialized"));
}

void USkillStateMachine::Tick(float DeltaSeconds)
{
	if (bFinished) return;

	if (CurrentFlow)
	{
		CurrentFlow->Tick(DeltaSeconds);

		// 延迟完成的 flow(如 TimelineAction:时间轴跑完才 MarkFinished)需在这里驱动转换。
		// 同步完成的 flow(Entry/Sequence/… 在 Enter() 里已自行 TryTransitions,CurrentFlow 已前移)
		// 不会在此命中:它们的 Enter 结束时 CurrentFlow 已是下一个未完成的 flow。
		if (!bFinished && CurrentFlow && CurrentFlow->IsFinished())
		{
			OnFlowFinished();
		}
	}
}

void USkillStateMachine::Terminate()
{
	if (!bFinished)
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("SkillStateMachine terminated"));
		if (CurrentFlow)
		{
			CurrentFlow->Exit();
			CurrentFlow = nullptr;
		}
		bFinished = true;
	}
}

void USkillStateMachine::EnterNode(const FString& NodeId)
{
	if (!Instance.IsValid()) return;

	const FSkillData* Data = Instance->GetSkillData().Get();
	if (!Data) return;

	const FSkillDataNode* Node = Data->FindNode(NodeId);
	if (!Node)
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("SkillStateMachine: Node not found: %s"), *NodeId);
		bFinished = true;
		return;
	}

	UE_LOG(LogSkillRuntime, Verbose, TEXT("SkillStateMachine: EnterNode %s (kind=%s, sub=%s)"),
		*NodeId, *Node->Kind.ToString(), *Node->SubType.ToString());

	// 退出旧 Flow
	if (CurrentFlow)
	{
		CurrentFlow->Exit();
		CurrentFlow = nullptr;
	}

	// 创建新 Flow
	CurrentFlow = CreateFlowForNode(*Node);
	if (!CurrentFlow)
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("SkillStateMachine: Failed to create flow for node: %s"), *NodeId);
		bFinished = true;
		return;
	}

	CurrentFlow->Initialize(this, *Node);
	CurrentFlow->Enter();
}

USkillFlow* USkillStateMachine::CreateFlowForNode(const FSkillDataNode& Node)
{
	if (!Instance.IsValid()) return nullptr;

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// 显式映射到具体 Flow 类。
	// 不用 FindObject<UClass>(nullptr, "SkillFlow_Xxx") 按名查:UE5 下该重载对"未被
	// 别处引用而尚未加载/注册可见"的类返回 null,会回退到抽象基类 USkillFlow,
	// NewObject(抽象类) 触发 ensure 并把对象 null 化 → 行为节点静默不执行。
	UClass* FlowClass = nullptr;
	if (Node.Kind == TEXT("action"))
	{
		// 所有 action 节点统一用 USkillFlow_Action,内部按 SubType(TimelineAction 等)再分流
		FlowClass = USkillFlow_Action::StaticClass();
	}
	else if (Node.Kind == TEXT("listener"))
	{
		FlowClass = USkillFlow_Listener::StaticClass();
	}
	else // flow 节点:按 SubType(flowType)分
	{
		const FString Sub = Node.SubType.ToString();
		if      (Sub == TEXT("Entry"))       FlowClass = USkillFlow_Entry::StaticClass();
		else if (Sub == TEXT("Exit"))        FlowClass = USkillFlow_Exit::StaticClass();
		else if (Sub == TEXT("Sequence"))    FlowClass = USkillFlow_Sequence::StaticClass();
		else if (Sub == TEXT("Conditional")) FlowClass = USkillFlow_Conditional::StaticClass();
		else if (Sub == TEXT("Random"))      FlowClass = USkillFlow_Random::StaticClass();
		else if (Sub == TEXT("Loop"))        FlowClass = USkillFlow_Loop::StaticClass();
	}

	if (!FlowClass || FlowClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogSkillRuntime, Warning,
			TEXT("CreateFlowForNode: 未知/不可实例化节点 kind=%s sub=%s,跳过"),
			*Node.Kind.ToString(), *Node.SubType.ToString());
		return nullptr;
	}

	return NewObject<USkillFlow>(World, FlowClass);
}

void USkillStateMachine::TryTransitions(const FString& NodeId, const FString& BranchId)
{
	if (!Instance.IsValid()) return;

	const FSkillData* Data = Instance->GetSkillData().Get();
	if (!Data) return;

	auto Outgoing = Data->FindOutgoing(NodeId, BranchId);

	// 按 priority 排序（降序）
	Outgoing.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
	{
		int32 PrioA = 0, PrioB = 0;
		A->TryGetNumberField(TEXT("priority"), PrioA);
		B->TryGetNumberField(TEXT("priority"), PrioB);
		return PrioA > PrioB;
	});

	for (const auto& Trans : Outgoing)
	{
		// 检查 conditions
		const TArray<TSharedPtr<FJsonValue>>* Conditions;
		bool bConditionsMet = true;
		if (Trans->TryGetArrayField(TEXT("conditions"), Conditions) && Conditions->Num() > 0)
		{
			for (const auto& C : *Conditions)
			{
				if (!EvaluateSingleCondition(C->AsObject()))
				{
					bConditionsMet = false;
					break;
				}
			}
		}

		if (bConditionsMet)
		{
			const FString TargetId = Trans->GetStringField(TEXT("targetNodeId"));
			UE_LOG(LogSkillRuntime, Verbose, TEXT("Transition: %s → %s (branch=%s)"),
				*NodeId, *TargetId, *BranchId);
			EnterNode(TargetId);
			return;
		}
	}

	// 没有可走的 Transition → 结束
	UE_LOG(LogSkillRuntime, Warning, TEXT("No transition from %s/%s — finishing skill"), *NodeId, *BranchId);
	bFinished = true;
}

bool USkillStateMachine::EvaluateConditionGroup(const TSharedPtr<FJsonObject>& ConditionGroup) const
{
	if (!ConditionGroup.IsValid()) return true;

	const TArray<TSharedPtr<FJsonValue>>* Conditions;
	if (!ConditionGroup->TryGetArrayField(TEXT("conditions"), Conditions)) return true;
	if (Conditions->Num() == 0) return true;

	FString Combinator = TEXT("AND");
	ConditionGroup->TryGetStringField(TEXT("combinator"), Combinator);

	// expression escape hatch（暂未实现）
	const TSharedPtr<FJsonObject>* ExprObj;
	if (ConditionGroup->TryGetObjectField(TEXT("expression"), ExprObj))
	{
		bool bEnabled = false;
		if ((*ExprObj)->TryGetBoolField(TEXT("enabled"), bEnabled) && bEnabled)
		{
			UE_LOG(LogSkillRuntime, Warning, TEXT("Expression escape hatch not yet implemented"));
			return false;
		}
	}

	if (Combinator == TEXT("OR"))
	{
		for (const auto& C : *Conditions)
		{
			if (EvaluateSingleCondition(C->AsObject())) return true;
		}
		return false;
	}
	else // AND
	{
		for (const auto& C : *Conditions)
		{
			if (!EvaluateSingleCondition(C->AsObject())) return false;
		}
		return true;
	}
}

bool USkillStateMachine::EvaluateSingleCondition(const TSharedPtr<FJsonObject>& Condition) const
{
	if (!Condition.IsValid()) return true;
	if (!Instance.IsValid()) return true;

	FString TypeId = Condition->GetStringField(TEXT("typeId"));
	bool bNegated = false;
	Condition->TryGetBoolField(TEXT("negated"), bNegated);

	// 构造上下文
	FSkillConditionContext Ctx;
	Ctx.Instance = Instance.Get();
	const TSharedPtr<FJsonObject>* Params;
	if (Condition->TryGetObjectField(TEXT("properties"), Params))
	{
		Ctx.Params = *Params;
	}

	USkillRuntimeSubsystem* Subsys = Instance->GetWorld()->GetGameInstance()->GetSubsystem<USkillRuntimeSubsystem>();
	bool bResult = Subsys->EvaluateCondition(FName(*TypeId), Ctx);

	return bNegated ? !bResult : bResult;
}

UWorld* USkillStateMachine::GetWorld() const
{
	if (Instance.IsValid() && Instance->GetOwnerCharacter())
	{
		return Instance->GetOwnerCharacter()->GetWorld();
	}
	return nullptr;
}

void USkillStateMachine::OnFlowFinished()
{
	// Flow 结束 → 触发 Transition
	if (CurrentFlow)
	{
		FString NodeId = CurrentFlow->GetNodeId();
		UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow finished: %s, trying transitions"), *NodeId);
		TryTransitions(NodeId, TEXT("out"));
	}
}
