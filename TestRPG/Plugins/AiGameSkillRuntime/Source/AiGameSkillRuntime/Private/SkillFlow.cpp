// USkillFlow 实现 — 三层架构 Layer 2

#include "SkillFlow.h"
#include "AiGameSkillRuntime.h"
#include "SkillStateMachine.h"
#include "SkillInstance.h"
#include "SkillRuntimeSubsystem.h"
#include "Interfaces/ISkillAction.h"
#include "Interfaces/ISkillTrackHandler.h"
#include "Interfaces/ISkillListener.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

// ============================================
// USkillFlow 基类
// ============================================

void USkillFlow::Initialize(USkillStateMachine* InOwnerSM, const FSkillDataNode& InNodeData)
{
	OwnerSM = InOwnerSM;
	NodeData = InNodeData;
	NodeId = InNodeData.Id;
	bFinished = false;
}

void USkillFlow::Enter()
{
	// 默认：立即标记完成（叶子节点重写此函数）
	MarkFinished();
}

void USkillFlow::Tick(float DeltaSeconds)
{
	// 默认：无逻辑（叶子节点重写此函数）
}

void USkillFlow::Exit()
{
	// 默认：无清理
}

void USkillFlow::MarkFinished()
{
	if (bFinished) return;
	bFinished = true;
	OnFinished.Broadcast();
}

USkillInstance* USkillFlow::GetInstance() const
{
	if (OwnerSM != nullptr)
	{
		return OwnerSM->GetInstance();
	}
	return nullptr;
}

// ============================================
// USkillFlow_Entry
// ============================================

void USkillFlow_Entry::Enter()
{
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Entry]: %s"), *NodeId);
	// Entry → 走 "out" 分支
	USkillStateMachine* SM = GetSM();
	if (SM != nullptr)
	{
		SM->TryTransitions(NodeId, TEXT("out"));
	}
	MarkFinished();
}

// ============================================
// USkillFlow_Exit
// ============================================

void USkillFlow_Exit::Enter()
{
	UE_LOG(LogSkillRuntime, Log, TEXT("Flow[Exit]: %s — skill finished"), *NodeId);
	// 标记 Skill 结束（StateMachine 会检测到 bFinished）
	USkillStateMachine* SM = GetSM();
	if (SM != nullptr)
	{
		SM->Terminate();
	}
	MarkFinished();
}

// ============================================
// USkillFlow_Sequence
// ============================================

void USkillFlow_Sequence::Enter()
{
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Sequence]: %s"), *NodeId);

	const TSharedPtr<FJsonObject>* DataObj;
	if (!NodeData.Raw->TryGetObjectField(TEXT("data"), DataObj))
	{
		MarkFinished();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Branches;
	if (!(*DataObj)->TryGetArrayField(TEXT("branches"), Branches) || Branches->Num() == 0)
	{
		MarkFinished();
		return;
	}

	// P0：只走第一个分支（后续扩展序列执行）
	const TSharedPtr<FJsonObject> FirstBranch = (*Branches)[0]->AsObject();
	const FString BranchId = FirstBranch->GetStringField(TEXT("id"));

	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Sequence]: taking first branch %s"), *BranchId);
	USkillStateMachine* SM = GetSM();
	if (SM != nullptr)
	{
		SM->TryTransitions(NodeId, BranchId);
	}
	MarkFinished();
}

// ============================================
// USkillFlow_Conditional
// ============================================

void USkillFlow_Conditional::Enter()
{
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Conditional]: %s"), *NodeId);

	const TSharedPtr<FJsonObject>* DataObj;
	if (!NodeData.Raw->TryGetObjectField(TEXT("data"), DataObj))
	{
		MarkFinished();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Branches;
	if (!(*DataObj)->TryGetArrayField(TEXT("branches"), Branches) || Branches->Num() == 0)
	{
		MarkFinished();
		return;
	}

	USkillStateMachine* SM = GetSM();
	if (SM == nullptr) { MarkFinished(); return; }

	for (const auto& B : *Branches)
	{
		const TSharedPtr<FJsonObject> Branch = B->AsObject();
		const FString BranchId = Branch->GetStringField(TEXT("id"));

		// 检查 condition
		const TSharedPtr<FJsonObject>* CondObj;
		bool bConditionMet = true;
		if (Branch->TryGetObjectField(TEXT("condition"), CondObj))
		{
			bConditionMet = SM->EvaluateConditionGroup(*CondObj);
		}

		if (bConditionMet)
		{
			UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Conditional]: branch %s satisfied"), *BranchId);
			SM->TryTransitions(NodeId, BranchId);
			MarkFinished();
			return;
		}
	}

	// 所有分支都不满足 → 结束
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Conditional]: no branch satisfied"));
	MarkFinished();
}

// ============================================
// USkillFlow_Random
// ============================================

void USkillFlow_Random::Enter()
{
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Random]: %s"), *NodeId);

	const TSharedPtr<FJsonObject>* DataObj;
	if (!NodeData.Raw->TryGetObjectField(TEXT("data"), DataObj))
	{
		MarkFinished();
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Branches;
	if (!(*DataObj)->TryGetArrayField(TEXT("branches"), Branches) || Branches->Num() == 0)
	{
		MarkFinished();
		return;
	}

	// 收集权重
	TArray<TPair<FString, float>> WeightedBranches;
	float TotalWeight = 0.0f;
	for (const auto& B : *Branches)
	{
		const TSharedPtr<FJsonObject> Branch = B->AsObject();
		const FString BranchId = Branch->GetStringField(TEXT("id"));
		float Weight = 1.0f;
		Branch->TryGetNumberField(TEXT("weight"), Weight);
		WeightedBranches.Add(TPair<FString, float>(BranchId, Weight));
		TotalWeight += Weight;
	}

	// 随机选
	float Roll = FMath::FRand() * TotalWeight;
	float Cumulative = 0.0f;
	FString SelectedBranch;
	for (const auto& Elem : WeightedBranches)
	{
		Cumulative += Elem.Value;
		if (Roll < Cumulative)
		{
			SelectedBranch = Elem.Key;
			break;
		}
	}
	if (SelectedBranch.IsEmpty())
	{
		SelectedBranch = WeightedBranches.Last().Key;
	}

	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Random]: selected branch %s"), *SelectedBranch);
	USkillStateMachine* SM = GetSM();
	if (SM != nullptr)
	{
		SM->TryTransitions(NodeId, SelectedBranch);
	}
	MarkFinished();
}

// ============================================
// USkillFlow_Loop
// ============================================

void USkillFlow_Loop::Enter()
{
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Loop]: %s"), *NodeId);
	// P0：简单实现，走 "body" 分支
	USkillStateMachine* SM = GetSM();
	if (SM != nullptr)
	{
		SM->TryTransitions(NodeId, TEXT("body"));
	}
	MarkFinished();
}

void USkillFlow_Loop::Tick(float DeltaSeconds)
{
	// P0：Loop 暂未完整实现，Tick 中不做额外处理
}

// ============================================
// USkillFlow_Action
// ============================================

void USkillFlow_Action::Enter()
{
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Action]: %s (sub=%s)"), *NodeId, *NodeData.SubType.ToString());

	const TSharedPtr<FJsonObject>* DataObj;
	if (!NodeData.Raw->TryGetObjectField(TEXT("data"), DataObj))
	{
		MarkFinished();
		return;
	}

	if (NodeData.SubType == TEXT("TimelineAction"))
	{
		// Timeline 行动：启动时间轴
		InitializeTimeline(*DataObj);
	}
	else
	{
		// 非 Timeline 行动：立即执行
		ExecuteImmediateAction(*DataObj);
		MarkFinished();
	}
}

void USkillFlow_Action::Tick(float DeltaSeconds)
{
	if (bActionDone) return;

	// 如果 Timeline 活跃，推进时间轴
	if (TotalFrames > 0 && CurrentFrame < TotalFrames)
	{
		AdvanceTimeline(DeltaSeconds);
	}
}

void USkillFlow_Action::Exit()
{
	// 清理 TrackHandler 缓存
	for (auto& Pair : TrackHandlerCache)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Cleanup();
		}
	}
	TrackHandlerCache.Empty();
	ActiveRangeEvents.Empty();
}

void USkillFlow_Action::InitializeTimeline(const TSharedPtr<FJsonObject>& ActionData)
{
	CurrentFrame = 0;
	AccumulatedTime = 0.0f;
	ActiveRangeEvents.Empty();
	bActionDone = false;

	// 读 duration / frameRate
	TotalFrames = 30;
	ActionData->TryGetNumberField(TEXT("duration"), TotalFrames);

	FrameRate = 30.0f;
	PlayRate = 1.0f;

	// 从 animation 轨道读 frameRate / playRate
	const TArray<TSharedPtr<FJsonValue>>* Tracks;
	if (ActionData->TryGetArrayField(TEXT("tracks"), Tracks))
	{
		for (const auto& TrackVal : *Tracks)
		{
			const TSharedPtr<FJsonObject> Track = TrackVal->AsObject();
			FString TrackType;
			if (!Track->TryGetStringField(TEXT("typeId"), TrackType))
				Track->TryGetStringField(TEXT("trackType"), TrackType);
			if (TrackType != TEXT("animation")) continue;

			const TArray<TSharedPtr<FJsonValue>>* Evts;
			if (Track->TryGetArrayField(TEXT("events"), Evts) && Evts->Num() > 0)
			{
				const TSharedPtr<FJsonObject> FirstEvt = (*Evts)[0]->AsObject();
				const TSharedPtr<FJsonObject>* Props;
				if (FirstEvt->TryGetObjectField(TEXT("properties"), Props))
				{
					double ClipPlayRate = 1.0;
					if ((*Props)->TryGetNumberField(TEXT("playRate"), ClipPlayRate))
					{
						PlayRate = static_cast<float>(ClipPlayRate);
					}

					const TSharedPtr<FJsonObject>* ClipObj;
					if ((*Props)->TryGetObjectField(TEXT("clip"), ClipObj))
					{
						double ClipFrameRate = 30.0;
						if ((*ClipObj)->TryGetNumberField(TEXT("frameRate"), ClipFrameRate))
						{
							FrameRate = static_cast<float>(ClipFrameRate);
						}

						bool bUseAssetDuration = false;
						FirstEvt->TryGetBoolField(TEXT("useAssetDuration"), bUseAssetDuration);
						if (bUseAssetDuration)
						{
							double ClipNumFrames = 0;
							if ((*ClipObj)->TryGetNumberField(TEXT("numFrames"), ClipNumFrames))
							{
								TotalFrames = static_cast<int32>(ClipNumFrames);
							}
						}
					}
				}
			}
			break;
		}
	}

	UE_LOG(LogSkillRuntime, Log, TEXT("Flow[Action/Timeline]: %s (frames=%d, rate=%.1f, playRate=%.2f)"),
		*NodeId, TotalFrames, FrameRate, PlayRate);

	// 执行 PlayAnim（如果有 animation 轨道）
	USkillInstance* Inst = GetInstance();
	if (Inst)
	{
		USkillRuntimeSubsystem* Subsys = Inst->GetWorld()->GetGameInstance()->GetSubsystem<USkillRuntimeSubsystem>();
		TSharedPtr<ISkillAction> PlayAnim = Subsys->CreateAction(FName(TEXT("PlayAnim")));
		if (PlayAnim.IsValid())
		{
			PlayAnim->Execute(Inst, ActionData);
		}
	}

	// 处理第 0 帧事件
	ProcessTimelineEvents();
}

void USkillFlow_Action::ExecuteImmediateAction(const TSharedPtr<FJsonObject>& ActionData)
{
	USkillInstance* Inst = GetInstance();
	if (!Inst) return;

	USkillRuntimeSubsystem* Subsys = Inst->GetWorld()->GetGameInstance()->GetSubsystem<USkillRuntimeSubsystem>();
	TSharedPtr<ISkillAction> Action = Subsys->CreateAction(NodeData.SubType);
	if (Action.IsValid())
	{
		Action->Execute(Inst, ActionData);
	}
	else
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("Unknown action type: %s"), *NodeData.SubType.ToString());
	}
}

void USkillFlow_Action::AdvanceTimeline(float DeltaSeconds)
{
	AccumulatedTime += DeltaSeconds * PlayRate;
	int32 NewFrame = FMath::FloorToInt(AccumulatedTime * FrameRate);

	if (NewFrame != CurrentFrame)
	{
		CurrentFrame = NewFrame;
		ProcessTimelineEvents();

		// 时间轴走完
		if (CurrentFrame >= TotalFrames)
		{
			UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Action/Timeline]: finished %s"), *NodeId);
			bActionDone = true;
			MarkFinished();
		}
	}
}

void USkillFlow_Action::ProcessTimelineEvents()
{
	USkillInstance* Inst = GetInstance();
	if (!Inst) return;

	const TSharedPtr<FJsonObject>* DataObj;
	if (!NodeData.Raw->TryGetObjectField(TEXT("data"), DataObj)) return;

	const TArray<TSharedPtr<FJsonValue>>* Tracks;
	if (!(*DataObj)->TryGetArrayField(TEXT("tracks"), Tracks)) return;

	USkillRuntimeSubsystem* Subsys = Inst->GetWorld()->GetGameInstance()->GetSubsystem<USkillRuntimeSubsystem>();

	for (const auto& TrackVal : *Tracks)
	{
		const TSharedPtr<FJsonObject> Track = TrackVal->AsObject();

		FString TrackType;
		if (!Track->TryGetStringField(TEXT("typeId"), TrackType))
			Track->TryGetStringField(TEXT("trackType"), TrackType);
		if (TrackType.IsEmpty() || TrackType == TEXT("animation")) continue;

		TSharedPtr<ISkillTrackHandler> Handler;
		if (TSharedPtr<ISkillTrackHandler>* Cached = TrackHandlerCache.Find(FName(*TrackType)))
		{
			Handler = *Cached;
		}
		else
		{
			Handler = Subsys->CreateTrackHandler(FName(*TrackType));
			if (!Handler.IsValid()) continue;
			TrackHandlerCache.Add(FName(*TrackType), Handler);
		}

		const TArray<TSharedPtr<FJsonValue>>* Events;
		if (!Track->TryGetArrayField(TEXT("events"), Events)) continue;

		for (const auto& EventVal : *Events)
		{
			const TSharedPtr<FJsonObject> Event = EventVal->AsObject();

			FString EventId;
			if (!Event->TryGetStringField(TEXT("id"), EventId) || EventId.IsEmpty())
			{
				EventId = TrackType + TEXT("_") + FString::Printf(TEXT("%p"), Event.Get());
			}

			int32 StartFrame = 0, EndFrame = 0;
			Event->TryGetNumberField(TEXT("startFrame"), StartFrame);
			Event->TryGetNumberField(TEXT("endFrame"), EndFrame);

			const bool bIsRange = (EndFrame > StartFrame);
			const bool bIsInRange = (CurrentFrame >= StartFrame && CurrentFrame <= EndFrame);
			const bool bIsAtStart = (CurrentFrame == StartFrame);

			auto BuildEventParams = [&]() -> TSharedPtr<FJsonObject>
			{
				const TSharedPtr<FJsonObject>* Props;
				TSharedPtr<FJsonObject> Params;
				if (Event->TryGetObjectField(TEXT("properties"), Props))
				{
					Params = MakeShared<FJsonObject>(*(*Props));
				}
				else
				{
					Params = MakeShared<FJsonObject>();
				}
				Params->SetStringField(TEXT("eventId"), EventId);
				return Params;
			};

			if (bIsRange)
			{
				const bool bWasActive = ActiveRangeEvents.Contains(EventId);
				TSharedPtr<FJsonObject> EventParams = BuildEventParams();

				if (bIsInRange && !bWasActive)
				{
					ActiveRangeEvents.Add(EventId);
					Handler->OnEnter(Inst, EventParams, CurrentFrame);
				}
				else if (!bIsInRange && bWasActive)
				{
					ActiveRangeEvents.Remove(EventId);
					Handler->OnExit(Inst, EventParams, CurrentFrame);
				}
				else if (bIsInRange && bWasActive)
				{
					Handler->OnTick(Inst, EventParams, CurrentFrame);
				}
			}
			else
			{
				if (bIsAtStart)
				{
					TSharedPtr<FJsonObject> EventParams = BuildEventParams();
					Handler->OnEnter(Inst, EventParams, CurrentFrame);
				}
			}
		}
	}
}

// ============================================
// USkillFlow_Listener
// ============================================

void USkillFlow_Listener::Enter()
{
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Listener]: %s (sub=%s)"), *NodeId, *NodeData.SubType.ToString());

	USkillInstance* Inst = GetInstance();
	if (!Inst) { MarkFinished(); return; }

	USkillRuntimeSubsystem* Subsys = Inst->GetWorld()->GetGameInstance()->GetSubsystem<USkillRuntimeSubsystem>();
	TSharedPtr<ISkillListener> Listener = Subsys->CreateListener(NodeData.SubType);

	if (Listener.IsValid())
	{
		const TSharedPtr<FJsonObject>* DataObj;
		if (NodeData.Raw->TryGetObjectField(TEXT("data"), DataObj))
		{
			FOnListenerTriggered OnTrigger;
			OnTrigger.BindLambda([this, Inst]()
			{
				UE_LOG(LogSkillRuntime, Verbose, TEXT("Flow[Listener]: triggered %s"), *NodeId);
				USkillStateMachine* SM = GetSM();
				if (SM != nullptr)
				{
					SM->TryTransitions(NodeId, TEXT("out"));
				}
				MarkFinished();
			});
			Listener->Subscribe(Inst, *DataObj, OnTrigger);
			ActiveListeners.Add(Listener);
			return;  // 等待事件触发
		}
	}

	// Listener 创建失败 → 立即结束
	UE_LOG(LogSkillRuntime, Warning, TEXT("Flow[Listener]: failed to create listener %s"), *NodeData.SubType.ToString());
	MarkFinished();
}

void USkillFlow_Listener::Exit()
{
	for (auto& Listener : ActiveListeners)
	{
		if (Listener.IsValid())
		{
			Listener->Unsubscribe();
		}
	}
	ActiveListeners.Empty();
}
