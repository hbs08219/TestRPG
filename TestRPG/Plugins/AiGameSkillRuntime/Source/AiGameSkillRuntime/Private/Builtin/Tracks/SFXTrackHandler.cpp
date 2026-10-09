// SFX Track Handler — 在 Runtime 中播放/停止音效

#include "SFXTrackHandler.h"
#include "SkillInstance.h"
#include "SkillRuntimeSubsystem.h"
#include "AiGameSkillRuntime.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

void FSFXTrackHandler::OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	if (!Instance || !EventParams.IsValid()) return;

	ACharacter* Char = Instance->GetOwnerCharacter();
	if (!Char) return;

	// 读事件 id
	FString EventId;
	EventParams->TryGetStringField(TEXT("eventId"), EventId);
	if (EventId.IsEmpty())
	{
		EventId = FString::Printf(TEXT("sfx_%d_%d"), Frame, GetTypeHash(EventParams.Get()));
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
		UE_LOG(LogSkillRuntime, Warning, TEXT("SFXTrack: no asset.objectPath in event at frame %d"), Frame);
		return;
	}

	// 读挂载 socket
	FString AttachSocket;
	EventParams->TryGetStringField(TEXT("attachSocket"), AttachSocket);

	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *ObjectPath);
	if (!Sound)
	{
		UE_LOG(LogSkillRuntime, Warning, TEXT("SFXTrack: failed to load SoundBase: %s"), *ObjectPath);
		return;
	}

	USkeletalMeshComponent* MeshComp = Char->GetMesh();
	const FName SocketFName = AttachSocket.IsEmpty() ? NAME_None : FName(*AttachSocket);

	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAttached(
		Sound, MeshComp, SocketFName,
		FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		/*bStopWhenAttachedToDestroyed=*/true);

	if (Comp)
	{
		ActiveSFX.Add(EventId, Comp);
		UE_LOG(LogSkillRuntime, Log, TEXT("SFXTrack: playing %s on %s"), *ObjectPath, *AttachSocket);
	}
}

void FSFXTrackHandler::OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	if (!Instance || !EventParams.IsValid()) return;

	FString EventId;
	EventParams->TryGetStringField(TEXT("eventId"), EventId);
	if (EventId.IsEmpty())
	{
		EventId = FString::Printf(TEXT("sfx_%d_%d"), Frame, GetTypeHash(EventParams.Get()));
	}

	if (TWeakObjectPtr<UAudioComponent>* WeakComp = ActiveSFX.Find(EventId))
	{
		if (UAudioComponent* Comp = WeakComp->Get())
		{
			Comp->Stop();
			Comp->DestroyComponent();
		}
		ActiveSFX.Remove(EventId);
	}
}

void FSFXTrackHandler::OnTick(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	// SFX 不需要每帧 tick
}

void FSFXTrackHandler::Cleanup()
{
	for (auto& Pair : ActiveSFX)
	{
		if (UAudioComponent* Comp = Pair.Value.Get())
		{
			Comp->Stop();
			Comp->DestroyComponent();
		}
	}
	ActiveSFX.Empty();
}
