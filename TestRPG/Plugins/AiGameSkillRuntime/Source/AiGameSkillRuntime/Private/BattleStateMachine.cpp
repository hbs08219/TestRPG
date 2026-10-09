// UBattleStateMachine 实现

#include "BattleStateMachine.h"
#include "BattleFlow.h"
#include "Components/AttributeComponent.h"
#include "BattleDamageCalculator.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void UBattleStateMachine::Initialize(ACharacter* InPlayerChar, const TArray<ACharacter*>& InEnemies)
{
	PlayerChar = InPlayerChar;
	bFinished = false;
	Result = EBattleResult::Ongoing;
	bHasPendingCommand = false;
	bPlayerDefending = false;

	Enemies.Empty();
	for (ACharacter* Enemy : InEnemies)
	{
		if (Enemy) Enemies.Add(Enemy);
	}

	if (PlayerChar.IsValid())
	{
		IdleLocation = PlayerChar->GetActorLocation();
	}

	UE_LOG(LogSkillRuntime, Log, TEXT("BattleStateMachine initialized (enemies=%d)"), Enemies.Num());

	// P0：直接进入 PlayerAction（跳过 Idle，Idle 主要是预留给"战斗开始动画"等）
	RequestTransition(TEXT("PlayerAction"));
}

void UBattleStateMachine::Tick(float DeltaSeconds)
{
	if (bFinished) return;
	if (bUseScriptFlow) return;   // TS 全权驱动,跳过 C++ Flow
	if (!CurrentFlow) return;
	CurrentFlow->Tick(DeltaSeconds);
}

void UBattleStateMachine::Terminate()
{
	if (bFinished) return;
	UE_LOG(LogSkillRuntime, Log, TEXT("BattleStateMachine terminated"));
	if (CurrentFlow)
	{
		if (FlowFinishedHandle.IsValid())
		{
			CurrentFlow->OnFinished.Remove(FlowFinishedHandle);
			FlowFinishedHandle.Reset();
		}
		CurrentFlow->Exit();
		CurrentFlow = nullptr;
	}
	bFinished = true;
}

FString UBattleStateMachine::GetCurrentStateName() const
{
	if (!CurrentFlow) return TEXT("None");
	return CurrentFlow->GetFlowName();
}

void UBattleStateMachine::RequestTransition(const FString& NextStateName)
{
	const FString FromName = CurrentFlow ? CurrentFlow->GetFlowName() : TEXT("None");
	UE_LOG(LogSkillRuntime, Log, TEXT("BattleStateMachine: Transition %s → %s"),
		*FromName, *NextStateName);

	if (CurrentFlow)
	{
		if (FlowFinishedHandle.IsValid())
		{
			CurrentFlow->OnFinished.Remove(FlowFinishedHandle);
			FlowFinishedHandle.Reset();
		}
		CurrentFlow->Exit();
		CurrentFlow = nullptr;
	}

	// 战斗终止状态："Finished" 或空串
	if (NextStateName.IsEmpty() || NextStateName.Equals(TEXT("Finished"), ESearchCase::IgnoreCase))
	{
		bFinished = true;
		OnBattleEnded.Broadcast(Result);
		OnStateChanged.Broadcast(FromName, TEXT("Finished"));
		return;
	}

	CurrentFlow = CreateFlow(NextStateName);
	if (CurrentFlow)
	{
		CurrentFlow->Initialize(this);
		// 监听 Flow 完成，自动推进
		FlowFinishedHandle = CurrentFlow->OnFinished.AddUObject(
			this, &UBattleStateMachine::OnFlowFinished);
		OnStateChanged.Broadcast(FromName, NextStateName);
		CurrentFlow->Enter();
	}
	else
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("Failed to create flow: %s"), *NextStateName);
		bFinished = true;
		OnBattleEnded.Broadcast(Result);
	}
}

UBattleFlow* UBattleStateMachine::CreateFlow(const FString& FlowTypeName)
{
	if (!PlayerChar.IsValid()) return nullptr;

	UWorld* World = PlayerChar->GetWorld();
	if (!World) return nullptr;

	// 显式映射 状态名 → Flow 类(不用 FindObject 按名查找：UE5 已弃用 ANY_PACKAGE，
	// nullptr-outer 按短名查 UClass 不可靠，会回退到 abstract 基类导致战斗空转)
	UClass* FlowClass = nullptr;
	if      (FlowTypeName == TEXT("Idle"))         FlowClass = UBattleFlow_Idle::StaticClass();
	else if (FlowTypeName == TEXT("PlayerAction")) FlowClass = UBattleFlow_PlayerAction::StaticClass();
	else if (FlowTypeName == TEXT("EnemyAction"))  FlowClass = UBattleFlow_EnemyAction::StaticClass();
	else if (FlowTypeName == TEXT("Victory"))      FlowClass = UBattleFlow_Victory::StaticClass();
	else if (FlowTypeName == TEXT("Defeat"))       FlowClass = UBattleFlow_Defeat::StaticClass();
	else if (FlowTypeName == TEXT("Reward"))       FlowClass = UBattleFlow_Reward::StaticClass();

	if (!FlowClass)
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("Unknown BattleFlow type: %s"), *FlowTypeName);
		return nullptr;
	}

	return NewObject<UBattleFlow>(World, FlowClass);
}

void UBattleStateMachine::OnFlowFinished()
{
	if (!CurrentFlow) return;

	const FString NextState = CurrentFlow->GetNextStateName();
	if (NextState.IsEmpty())
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("BattleFlow finished but no next state specified"));
		bFinished = true;
		OnBattleEnded.Broadcast(Result);
		return;
	}

	RequestTransition(NextState);
}

// ============================================
// 战斗上下文接口
// ============================================

ACharacter* UBattleStateMachine::GetCurrentEnemy() const
{
	for (const auto& Enemy : Enemies)
	{
		if (Enemy.IsValid())
		{
			UAttributeComponent* Attr = Cast<UAttributeComponent>(
				Enemy->FindComponentByClass(UAttributeComponent::StaticClass()));
			if (Attr && !FBattleDamageCalculator::IsDead(Attr))
			{
				return Enemy.Get();
			}
		}
	}
	return nullptr;
}

ACharacter* UBattleStateMachine::GetEnemyAt(int32 Index) const
{
	if (!Enemies.IsValidIndex(Index)) return nullptr;
	if (!Enemies[Index].IsValid()) return nullptr;
	ACharacter* E = Enemies[Index].Get();
	if (UAttributeComponent* Attr = Cast<UAttributeComponent>(
			E->FindComponentByClass(UAttributeComponent::StaticClass())))
	{
		if (FBattleDamageCalculator::IsDead(Attr)) return nullptr;
	}
	return E;
}

UAttributeComponent* UBattleStateMachine::GetPlayerAttr() const
{
	if (!PlayerChar.IsValid()) return nullptr;
	return Cast<UAttributeComponent>(
		PlayerChar->FindComponentByClass(UAttributeComponent::StaticClass()));
}

UAttributeComponent* UBattleStateMachine::GetCurrentEnemyAttr() const
{
	ACharacter* Enemy = GetCurrentEnemy();
	if (!Enemy) return nullptr;
	return Cast<UAttributeComponent>(
		Enemy->FindComponentByClass(UAttributeComponent::StaticClass()));
}

TArray<ACharacter*> UBattleStateMachine::GetAliveEnemies() const
{
	TArray<ACharacter*> Out;
	for (const auto& Enemy : Enemies)
	{
		if (!Enemy.IsValid()) continue;
		ACharacter* E = Enemy.Get();
		UAttributeComponent* Attr = Cast<UAttributeComponent>(
			E->FindComponentByClass(UAttributeComponent::StaticClass()));
		if (Attr && !FBattleDamageCalculator::IsDead(Attr))
		{
			Out.Add(E);
		}
	}
	return Out;
}

bool UBattleStateMachine::CheckBattleEnd()
{
	UAttributeComponent* PlayerAttr = GetPlayerAttr();
	if (PlayerAttr && FBattleDamageCalculator::IsDead(PlayerAttr))
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("CheckBattleEnd: Player defeated"));
		Result = EBattleResult::Defeat;
		RequestTransition(TEXT("Defeat"));
		return true;
	}

	if (GetAliveEnemies().Num() == 0)
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("CheckBattleEnd: All enemies dead"));
		Result = EBattleResult::Victory;
		RequestTransition(TEXT("Victory"));
		return true;
	}

	return false;
}

// ============================================
// 玩家指令通道
// ============================================

void UBattleStateMachine::SubmitPlayerCommand(const FBattleCommandPayload& Cmd)
{
	PendingCommand = Cmd;
	bHasPendingCommand = true;
	UE_LOG(LogSkillRuntime, Log, TEXT("[BattleSM] Player command submitted: %s, target=%d"),
		*UEnum::GetValueAsString(Cmd.Command), Cmd.TargetIndex);
}

FBattleCommandPayload UBattleStateMachine::ConsumePendingPlayerCommand()
{
	if (bHasPendingCommand)
	{
		bHasPendingCommand = false;
		return PendingCommand;
	}
	// 没有指令时返回默认普攻（自动 demo 模式）
	FBattleCommandPayload Default;
	Default.Command = EBattleCommand::Attack;
	Default.TargetIndex = -1;
	return Default;
}
