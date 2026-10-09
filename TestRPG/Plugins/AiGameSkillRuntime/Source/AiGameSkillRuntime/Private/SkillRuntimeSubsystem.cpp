// USkillRuntimeSubsystem 实现

#include "SkillRuntimeSubsystem.h"
#include "AiGameSkillRuntime.h"
#include "BuiltinRegistrar.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"

void USkillRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 注册所有内置 Handler
	SkillRuntime::Builtin::RegisterAll(*this);

	UE_LOG(LogSkillRuntime, Log, TEXT("SkillRuntimeSubsystem initialized"));
}

void USkillRuntimeSubsystem::Deinitialize()
{
	Super::Deinitialize();
	FlushCache();
	ActionRegistry.Empty();
	ConditionRegistry.Empty();
	TrackRegistry.Empty();
	ListenerRegistry.Empty();
	UE_LOG(LogSkillRuntime, Log, TEXT("SkillRuntimeSubsystem deinitialized"));
}

// ============================================
// 注册
// ============================================

void USkillRuntimeSubsystem::RegisterAction(FName Type, TFunction<TSharedPtr<ISkillAction>()> Factory)
{
	ActionRegistry.Add(Type, MoveTemp(Factory));
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Registered action: %s"), *Type.ToString());
}

void USkillRuntimeSubsystem::RegisterCondition(FName Type, FSkillConditionEvaluator Evaluator)
{
	ConditionRegistry.Add(Type, MoveTemp(Evaluator));
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Registered condition: %s"), *Type.ToString());
}

void USkillRuntimeSubsystem::RegisterTrack(FName Type, TFunction<TSharedPtr<ISkillTrackHandler>()> Factory)
{
	TrackRegistry.Add(Type, MoveTemp(Factory));
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Registered track: %s"), *Type.ToString());
}

void USkillRuntimeSubsystem::RegisterListener(FName Type, TFunction<TSharedPtr<ISkillListener>()> Factory)
{
	ListenerRegistry.Add(Type, MoveTemp(Factory));
	UE_LOG(LogSkillRuntime, Verbose, TEXT("Registered listener: %s"), *Type.ToString());
}

// ============================================
// 创建 / 查询
// ============================================

TSharedPtr<ISkillAction> USkillRuntimeSubsystem::CreateAction(FName Type) const
{
	const auto* Factory = ActionRegistry.Find(Type);
	if (Factory && *Factory)
	{
		return (*Factory)();
	}
	UE_LOG(LogSkillRuntime, Warning, TEXT("Unknown action type: %s"), *Type.ToString());
	return nullptr;
}

bool USkillRuntimeSubsystem::EvaluateCondition(FName Type, const FSkillConditionContext& Ctx) const
{
	const auto* Evaluator = ConditionRegistry.Find(Type);
	if (Evaluator && *Evaluator)
	{
		return (*Evaluator)(Ctx);
	}
	UE_LOG(LogSkillRuntime, Warning, TEXT("Unknown condition type: %s"), *Type.ToString());
	return false;
}

TSharedPtr<ISkillTrackHandler> USkillRuntimeSubsystem::CreateTrackHandler(FName Type) const
{
	const auto* Factory = TrackRegistry.Find(Type);
	if (Factory && *Factory)
	{
		return (*Factory)();
	}
	UE_LOG(LogSkillRuntime, Warning, TEXT("Unknown track type: %s"), *Type.ToString());
	return nullptr;
}

TSharedPtr<ISkillListener> USkillRuntimeSubsystem::CreateListener(FName Type) const
{
	const auto* Factory = ListenerRegistry.Find(Type);
	if (Factory && *Factory)
	{
		return (*Factory)();
	}
	UE_LOG(LogSkillRuntime, Warning, TEXT("Unknown listener type: %s"), *Type.ToString());
	return nullptr;
}

// ============================================
// 技能数据加载
// ============================================

FString USkillRuntimeSubsystem::GetSkillRootDir()
{
	return FPaths::ProjectContentDir() / TEXT("Data/ToolGen/Skill");
}

TSharedPtr<FSkillData> USkillRuntimeSubsystem::LoadSkillData(const FString& SkillPath)
{
	// 查缓存
	const auto* Cached = SkillDataCache.Find(SkillPath);
	if (Cached && Cached->IsValid())
	{
		return *Cached;
	}

	// 拼完整路径
	const FString FullPath = FPaths::Combine(GetSkillRootDir(), SkillPath) + TEXT(".json");

	if (!FPaths::FileExists(FullPath))
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("Skill file not found: %s"), *FullPath);
		return nullptr;
	}

	// 读文件
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *FullPath))
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("Failed to read skill file: %s"), *FullPath);
		return nullptr;
	}

	// 解析 JSON
	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogSkillRuntime, Error, TEXT("Failed to parse skill JSON: %s"), *FullPath);
		return nullptr;
	}

	// 构建 FSkillData
	TSharedPtr<FSkillData> Data = MakeShared<FSkillData>();
	Data->ParseFromJson(RootObj);

	// 缓存
	SkillDataCache.Add(SkillPath, Data);

	UE_LOG(LogSkillRuntime, Log, TEXT("Loaded skill: %s (id=%s, %d nodes)"),
		*SkillPath, *Data->Id, Data->Nodes.Num());

	return Data;
}

void USkillRuntimeSubsystem::FlushCache()
{
	SkillDataCache.Empty();
	UE_LOG(LogSkillRuntime, Log, TEXT("Skill data cache flushed"));
}
