// VFX Track Handler — 在 Runtime 中 spawn/destroy Niagara 特效

#include "VFXTrackHandler.h"
#include "SkillInstance.h"
#include "SkillRuntimeSubsystem.h"
#include "AiGameSkillRuntime.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void FVFXTrackHandler::OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	if (!Instance || !EventParams.IsValid()) return;

	ACharacter* Char = Instance->GetOwnerCharacter();
	if (!Char) return;

	// 读事件 id（用于管理 Range 事件的生命周期）
	FString EventId;
	EventParams->TryGetStringField(TEXT("eventId"), EventId);
	if (EventId.IsEmpty())
	{
		EventId = FString::Printf(TEXT("vfx_%d_%d"), Frame, GetTypeHash(EventParams.Get()));
	}

	// 读资产路径
	const TSharedPtr<FJsonObject>* AssetObj;
	FString ObjectPath;
	if (EventParams->TryGetObjectField(TEXT("asset"), AssetObj))
	{
		(*AssetObj)->TryGetStringField(TEXT("objectPath"), ObjectPath);
	}
	if (ObjectPath.IsEmpty())
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("VFXTrack: no asset.objectPath in event at frame %d"), Frame);
		return;
	}

	// 读挂载 socket
	FString AttachSocket;
	EventParams->TryGetStringField(TEXT("attachSocket"), AttachSocket);

	SpawnVFX(Instance, EventId, ObjectPath, AttachSocket);
}

void FVFXTrackHandler::OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	if (!Instance || !EventParams.IsValid()) return;

	FString EventId;
	EventParams->TryGetStringField(TEXT("eventId"), EventId);
	if (EventId.IsEmpty())
	{
		EventId = FString::Printf(TEXT("vfx_%d_%d"), Frame, GetTypeHash(EventParams.Get()));
	}

	DestroyVFX(EventId);
}

void FVFXTrackHandler::OnTick(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	// VFX 不需要每帧 tick
}

void FVFXTrackHandler::Cleanup()
{
	for (auto& Pair : ActiveVFX)
	{
		if (UNiagaraComponent* Comp = Pair.Value.Get())
		{
			Comp->DeactivateImmediate();
			Comp->DestroyComponent();
		}
	}
	ActiveVFX.Empty();
}

void FVFXTrackHandler::SpawnVFX(USkillInstance* Instance, const FString& EventId, const FString& ObjectPath, const FString& SocketName)
{
	if (!Instance) return;

	ACharacter* Char = Instance->GetOwnerCharacter();
	if (!Char) return;

	// Load Niagara System
	UNiagaraSystem* VFXSystem = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath);
	if (!VFXSystem)
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("VFXTrack: failed to load Niagara system: %s"), *ObjectPath);
		return;
	}

	// Spawn Niagara component attached to character mesh
	USkeletalMeshComponent* MeshComp = Char->GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("VFXTrack: character has no mesh component"));
		return;
	}

	UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		VFXSystem,
		MeshComp,
		SocketName.IsEmpty() ? NAME_None : FName(*SocketName),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true
	);

	if (NiagaraComp)
	{
		ActiveVFX.Add(EventId, NiagaraComp);
	}
}

void FVFXTrackHandler::DestroyVFX(const FString& EventId)
{
	TWeakObjectPtr<UNiagaraComponent>* Found = ActiveVFX.Find(EventId);
	if (!Found) return;

	if (UNiagaraComponent* Comp = Found->Get())
	{
		Comp->DeactivateImmediate();
		Comp->DestroyComponent();
	}

	ActiveVFX.Remove(EventId);
}
