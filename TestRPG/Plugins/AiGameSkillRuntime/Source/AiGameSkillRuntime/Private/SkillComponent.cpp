// USkillComponent 实现

#include "SkillComponent.h"
#include "SkillInstance.h"
#include "SkillRuntimeSubsystem.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USkillComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActiveInstance && ActiveInstance->IsRunning())
	{
		ActiveInstance->Tick(DeltaTime);

		// 检查是否结束
		if (ActiveInstance->IsFinished())
		{
			UE_LOG(LogSkillRuntime, Verbose, TEXT("SkillComponent: skill finished, clearing instance"));
			OnSkillFinished.Broadcast();
			ActiveInstance = nullptr;
		}
	}
}

bool USkillComponent::TryActivateSkill(const FString& SkillPath)
{
	if (IsBusy())
	{
		UE_LOG(LogSkillRuntime, Verbose, TEXT("SkillComponent: busy, rejecting skill %s"), *SkillPath);
		return false;
	}

	USkillRuntimeSubsystem* Subsys = GetWorld()->GetGameInstance()->GetSubsystem<USkillRuntimeSubsystem>();
	if (!Subsys) return false;

	TSharedPtr<FSkillData> Data = Subsys->LoadSkillData(SkillPath);
	if (!Data.IsValid())
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("SkillComponent: failed to load skill %s"), *SkillPath);
		return false;
	}

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar)
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("SkillComponent: owner is not a Character"));
		return false;
	}

	// 创建 SkillInstance
	ActiveInstance = NewObject<USkillInstance>(GetOwner());
	ActiveInstance->Initialize(Data, OwnerChar);

	UE_LOG(LogSkillRuntime, Log, TEXT("SkillComponent: activated skill %s"), *SkillPath);
	return true;
}

void USkillComponent::SetActiveSkillInputFlag(const FString& ButtonName, bool bPressed)
{
	if (ActiveInstance && ActiveInstance->IsRunning() && !ButtonName.IsEmpty())
	{
		ActiveInstance->SetInputFlag(ButtonName, bPressed);
	}
}

bool USkillComponent::IsBusy() const
{
	return ActiveInstance != nullptr && ActiveInstance->IsRunning();
}
