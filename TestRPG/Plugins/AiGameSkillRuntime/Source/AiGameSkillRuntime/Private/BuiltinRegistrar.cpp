// 内置 Handler 注册实现

#include "BuiltinRegistrar.h"
#include "SkillRuntimeSubsystem.h"
#include "AiGameSkillRuntime.h"

// Actions
#include "Builtin/Actions/PlayAnimAction.h"
#include "Builtin/Actions/SetVariableAction.h"

// Tracks
#include "Builtin/Tracks/HitBoxTrack.h"
#include "Builtin/Tracks/VFXTrackHandler.h"
#include "Builtin/Tracks/SFXTrackHandler.h"
#include "Builtin/Tracks/Sprite2DTrackHandler.h"

// Conditions
#include "Builtin/Conditions/BuiltinConditions.h"

// Listeners
#include "Builtin/Listeners/OnInputListener.h"

// Logging Action（调试用）
class FLogAction : public ISkillAction
{
public:
	virtual void Execute(USkillInstance* Instance, const TSharedPtr<FJsonObject>& Params) override
	{
		FString Msg;
		Params->TryGetStringField(TEXT("message"), Msg);
		if (Msg.IsEmpty())
		{
			// 尝试读 singleActionParams.message
			const TSharedPtr<FJsonObject>* SAP;
			if (Params->TryGetObjectField(TEXT("singleActionParams"), SAP))
			{
				(*SAP)->TryGetStringField(TEXT("message"), Msg);
			}
		}
		UE_LOG(LogSkillRuntime, Log, TEXT("[LogAction] %s"), Msg.IsEmpty() ? TEXT("(no message)") : *Msg);
	}
	virtual FName GetActionType() const override { return FName(TEXT("Log")); }
};

void SkillRuntime::Builtin::RegisterAll(USkillRuntimeSubsystem& Subsys)
{
	// === Actions ===
	Subsys.RegisterAction(FName(TEXT("PlayAnim")), []() -> TSharedPtr<ISkillAction>
	{
		return MakeShared<FPlayAnimAction>();
	});

	Subsys.RegisterAction(FName(TEXT("SetVariable")), []() -> TSharedPtr<ISkillAction>
	{
		return MakeShared<FSetVariableAction>();
	});

	Subsys.RegisterAction(FName(TEXT("Log")), []() -> TSharedPtr<ISkillAction>
	{
		return MakeShared<FLogAction>();
	});

	// === Tracks ===
	Subsys.RegisterTrack(FName(TEXT("HitBox")), []() -> TSharedPtr<ISkillTrackHandler>
	{
		return MakeShared<FHitBoxTrackHandler>();
	});

	Subsys.RegisterTrack(FName(TEXT("vfx")), []() -> TSharedPtr<ISkillTrackHandler>
	{
		return MakeShared<FVFXTrackHandler>();
	});

	Subsys.RegisterTrack(FName(TEXT("sfx")), []() -> TSharedPtr<ISkillTrackHandler>
	{
		return MakeShared<FSFXTrackHandler>();
	});

	Subsys.RegisterTrack(FName(TEXT("sprite2d")), []() -> TSharedPtr<ISkillTrackHandler>
	{
		return MakeShared<FSprite2DTrackHandler>();
	});

	// === Conditions ===
	BuiltinConditions::RegisterAll(Subsys);

	// === Listeners ===
	Subsys.RegisterListener(FName(TEXT("OnInput")), []() -> TSharedPtr<ISkillListener>
	{
		return MakeShared<FOnInputListener>();
	});

	UE_LOG(LogSkillRuntime, Log, TEXT("All built-in handlers registered"));
}
