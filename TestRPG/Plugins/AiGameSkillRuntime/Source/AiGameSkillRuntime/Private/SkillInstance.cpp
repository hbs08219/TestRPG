// USkillInstance 实现（改造后）
// 执行逻辑已迁移到 SkillStateMachine + SkillFlow

#include "SkillInstance.h"
#include "SkillStateMachine.h"
#include "SkillRuntimeSubsystem.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void USkillInstance::Initialize(TSharedPtr<FSkillData> InData, ACharacter* InOwner)
{
	Data = InData;
	Owner = InOwner;

	// 创建 StateMachine
	UWorld* World = InOwner ? InOwner->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("SkillInstance: World is null, cannot create StateMachine"));
		return;
	}

	StateMachine = NewObject<USkillStateMachine>(World, USkillStateMachine::StaticClass());
	if (StateMachine)
	{
		StateMachine->Initialize(this);

		// 初始化变量
		Variables.Empty();
		for (const auto& VarObj : Data->Variables)
		{
			const FString Name = VarObj->GetStringField(TEXT("name"));
			const FString Type = VarObj->GetStringField(TEXT("type"));
			FSkillVarValue Val;
			if (Type == TEXT("number"))
			{
				Val = FSkillVarValue(VarObj->GetNumberField(TEXT("defaultValue")));
			}
			else if (Type == TEXT("bool"))
			{
				Val = FSkillVarValue(VarObj->GetBoolField(TEXT("defaultValue")));
			}
			else
			{
				Val = FSkillVarValue(VarObj->GetStringField(TEXT("defaultValue")));
			}
			Variables.Add(Name, Val);
		}

		UE_LOG(LogSkillRuntime, Log, TEXT("SkillInstance initialized: %s (%d vars)"),
			*Data->Id, Variables.Num());

		// 进入 Entry 节点
		const FSkillDataNode* Entry = Data->FindEntry();
		if (Entry)
		{
			StateMachine->EnterNode(Entry->Id);
		}
		else
		{
			UE_LOG(LogSkillRuntime, Error, TEXT("No Entry node found in skill: %s"), *Data->Id);
			StateMachine->Terminate();
		}
	}
}

void USkillInstance::Tick(float DeltaSeconds)
{
	if (!StateMachine || StateMachine->IsFinished())
	{
		// 清除输入标记（原逻辑：每帧末尾调用）
		ClearInputFlags();
		return;
	}

	StateMachine->Tick(DeltaSeconds);

	// 每帧末尾清除输入标记
	ClearInputFlags();
}

void USkillInstance::Terminate()
{
	if (StateMachine && !StateMachine->IsFinished())
	{
		UE_LOG(LogSkillRuntime, Log, TEXT("SkillInstance terminated: %s"), *Data->Id);
		StateMachine->Terminate();
	}
	StateMachine = nullptr;

	// 恢复动画模式
	if (ACharacter* Char = Owner.Get())
	{
		if (USkeletalMeshComponent* MeshComp = Char->GetMesh())
		{
			if (MeshComp->GetAnimationMode() == EAnimationMode::AnimationSingleNode)
			{
				MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				UE_LOG(LogSkillRuntime, Log, TEXT("SkillInstance: restored AnimationBlueprint mode for %s"), *Char->GetName());
			}
		}
	}
}

bool USkillInstance::IsFinished() const
{
	return !StateMachine || StateMachine->IsFinished();
}

bool USkillInstance::IsRunning() const
{
	return StateMachine && StateMachine->IsRunning();
}

ESkillState USkillInstance::GetState() const
{
	if (!StateMachine) return ESkillState::Idle;
	if (StateMachine->IsFinished()) return ESkillState::Finished;
	if (StateMachine->IsRunning()) return ESkillState::Running;
	return ESkillState::Idle;
}

// ============================================
// 变量容器
// ============================================

FSkillVarValue USkillInstance::GetVariable(const FString& Name) const
{
	const auto* Val = Variables.Find(Name);
	if (Val) return *Val;
	return FSkillVarValue();
}

void USkillInstance::SetVariable(const FString& Name, const FSkillVarValue& Value)
{
	Variables.Add(Name, Value);
}

FSkillVarValue USkillInstance::ResolveValue(const TSharedPtr<FJsonValue>& JsonValue) const
{
	if (!JsonValue.IsValid()) return FSkillVarValue();

	// 检查 VarRef: { "__varRef": "X" }
	if (JsonValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
		FString VarName;
		if (Obj->TryGetStringField(TEXT("__varRef"), VarName))
		{
			return GetVariable(VarName);
		}
	}

	// 字面量
	return FSkillVarValue::FromJsonValue(JsonValue);
}

FSkillVarValue USkillInstance::ResolveField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName) const
{
	TSharedPtr<FJsonValue> Val = Obj->TryGetField(FieldName);
	if (Val.IsValid())
	{
		return ResolveValue(Val);
	}
	return FSkillVarValue();
}

// ============================================
// 输入标记
// ============================================

void USkillInstance::SetInputFlag(const FString& ButtonName, bool bPressed)
{
	InputFlags.Add(ButtonName, bPressed);
}

bool USkillInstance::HasInputFlag(const FString& ButtonName) const
{
	const bool* Val = InputFlags.Find(ButtonName);
	return Val && *Val;
}

void USkillInstance::ClearInputFlags()
{
	InputFlags.Empty();
}
