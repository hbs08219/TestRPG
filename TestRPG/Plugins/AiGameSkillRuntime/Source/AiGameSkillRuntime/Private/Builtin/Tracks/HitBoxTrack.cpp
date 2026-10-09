// HitBox Track Handler 实现

#include "HitBoxTrack.h"
#include "SkillInstance.h"
#include "AiGameSkillRuntime.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/OverlapResult.h"

void FHitBoxTrackHandler::OnEnter(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	UE_LOG(LogSkillRuntime, Log, TEXT("HitBox: ENTER at frame %d"), Frame);
	DrawDebug(Instance, EventParams, Frame, true);
	DoOverlapTest(Instance, EventParams);
}

void FHitBoxTrackHandler::OnExit(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	UE_LOG(LogSkillRuntime, Log, TEXT("HitBox: EXIT at frame %d"), Frame);
	DrawDebug(Instance, EventParams, Frame, false);
}

void FHitBoxTrackHandler::OnTick(USkillInstance* Instance, const TSharedPtr<FJsonObject>& EventParams, int32 Frame)
{
	DrawDebug(Instance, EventParams, Frame, true);
	DoOverlapTest(Instance, EventParams);
}

void FHitBoxTrackHandler::DrawDebug(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params, int32 Frame, bool bActive)
{
	ACharacter* Char = Instance ? Instance->GetOwnerCharacter() : nullptr;
	if (!Char) return;

	UWorld* World = Char->GetWorld();
	if (!World) return;

	// 读参数
	float Radius = 50.0f;
	Params->TryGetNumberField(TEXT("radius"), Radius);

	FVector Offset(0, 0, 0);
	const TSharedPtr<FJsonObject>* OffsetObj;
	if (Params->TryGetObjectField(TEXT("offset"), OffsetObj))
	{
		double X = 0, Y = 0, Z = 0;
		(*OffsetObj)->TryGetNumberField(TEXT("x"), X);
		(*OffsetObj)->TryGetNumberField(TEXT("y"), Y);
		(*OffsetObj)->TryGetNumberField(TEXT("z"), Z);
		Offset = FVector(X, Y, Z);
	}

	// 挂载点（默认骨骼名）
	FString SocketName = TEXT("hand_r");
	Params->TryGetStringField(TEXT("socket"), SocketName);

	FVector Location = Char->GetMesh()->GetSocketLocation(FName(*SocketName)) + Offset;
	FColor Color = bActive ? FColor::Red : FColor::Green;

	DrawDebugSphere(World, Location, Radius, 12, Color, false, 0.0f, 0, 1.0f);
}

void FHitBoxTrackHandler::DoOverlapTest(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params)
{
	ACharacter* Char = Instance ? Instance->GetOwnerCharacter() : nullptr;
	if (!Char) return;

	float Radius = 50.0f;
	Params->TryGetNumberField(TEXT("radius"), Radius);

	FString SocketName = TEXT("hand_r");
	Params->TryGetStringField(TEXT("socket"), SocketName);

	FVector Location = Char->GetMesh()->GetSocketLocation(FName(*SocketName));

	TArray<FOverlapResult> Overlaps;
	FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Char);

	bool bHit = Char->GetWorld()->OverlapMultiByChannel(
		Overlaps, Location, FQuat::Identity, ECC_Pawn, Shape, QueryParams);

	if (bHit)
	{
		for (const auto& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != Char)
			{
				UE_LOG(LogSkillRuntime, Log, TEXT("HitBox HIT: %s"), *HitActor->GetName());
			}
		}
	}
}
