// 技能运行时数据类型 — 纯 JsonObject 包装，弱 schema，数据驱动
// 编辑器侧加字段不需要改这里的结构

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * 技能图中的单个节点数据。
 * Raw 保存完整 JSON，Id / Kind / SubType 是高频访问字段的缓存。
 */
struct FSkillDataNode
{
	TSharedPtr<FJsonObject> Raw;
	FString Id;
	FName   Kind;     // "flow" | "listener" | "action"
	FName   SubType;  // "Sequence" | "OnInput" | "PlayAnim" | "TimelineAction" 等

	/** 从 JsonObject 解析并缓存常用字段 */
	static FSkillDataNode FromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FSkillDataNode Node;
		Node.Raw = Obj;
		Node.Id = Obj->GetStringField(TEXT("id"));
		const TSharedPtr<FJsonObject>* DataObj;
		if (Obj->TryGetObjectField(TEXT("data"), DataObj))
		{
			Node.Kind = FName(*(*DataObj)->GetStringField(TEXT("nodeKind")));
			// SubType 取 flowType 或 actionType 或 listenerType
			FString Sub;
			if ((*DataObj)->TryGetStringField(TEXT("flowType"), Sub) ||
				(*DataObj)->TryGetStringField(TEXT("actionType"), Sub) ||
				(*DataObj)->TryGetStringField(TEXT("listenerType"), Sub))
			{
				Node.SubType = FName(*Sub);
			}
		}
		return Node;
	}
};

/**
 * 变量值容器 — 支持 number / bool / string 三种类型
 */
struct FSkillVarValue
{
	enum EType { Empty, Number, Bool, String } Type = Empty;
	double NumberVal = 0.0;
	bool   BoolVal = false;
	FString StringVal;

	FSkillVarValue() = default;
	explicit FSkillVarValue(double V) : Type(Number), NumberVal(V) {}
	explicit FSkillVarValue(bool V) : Type(Bool), BoolVal(V) {}
	explicit FSkillVarValue(const FString& V) : Type(String), StringVal(V) {}

	/** 从 JSON 值推断类型并构造 */
	static FSkillVarValue FromJsonValue(const TSharedPtr<FJsonValue>& Val)
	{
		if (!Val.IsValid()) return FSkillVarValue();
		switch (Val->Type)
		{
		case EJson::Number: return FSkillVarValue(Val->AsNumber());
		case EJson::Boolean: return FSkillVarValue(Val->AsBool());
		case EJson::String: return FSkillVarValue(Val->AsString());
		default: return FSkillVarValue();
		}
	}

	FString ToString() const
	{
		switch (Type)
		{
		case Number: return FString::Printf(TEXT("%g"), NumberVal);
		case Bool: return BoolVal ? TEXT("true") : TEXT("false");
		case String: return StringVal;
		default: return TEXT("∅");
		}
	}
};

/**
 * 完整技能数据 — 一次 JSON 加载的结果
 */
struct FSkillData
{
	TSharedPtr<FJsonObject> Raw;
	TArray<FSkillDataNode> Nodes;
	TArray<TSharedPtr<FJsonObject>> Transitions;
	TArray<TSharedPtr<FJsonObject>> Variables;

	FString Id;
	FString Name;
	FString Kind;     // "active" | "passive"

	/** 从完整 JSON 对象解析 */
	void ParseFromJson(const TSharedPtr<FJsonObject>& Root)
	{
		Raw = Root;
		Id = Root->GetStringField(TEXT("id"));
		Name = Root->GetStringField(TEXT("name"));
		Kind = Root->GetStringField(TEXT("kind"));

		// 解析 graph
		const TSharedPtr<FJsonObject>* GraphObj;
		if (Root->TryGetObjectField(TEXT("graph"), GraphObj))
		{
			const TArray<TSharedPtr<FJsonValue>>* NodeArr;
			if ((*GraphObj)->TryGetArrayField(TEXT("nodes"), NodeArr))
			{
				for (const auto& N : *NodeArr)
				{
					Nodes.Add(FSkillDataNode::FromJson(N->AsObject()));
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* TransArr;
			if ((*GraphObj)->TryGetArrayField(TEXT("transitions"), TransArr))
			{
				for (const auto& T : *TransArr)
				{
					Transitions.Add(T->AsObject());
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* VarArr;
			if ((*GraphObj)->TryGetArrayField(TEXT("variables"), VarArr))
			{
				for (const auto& V : *VarArr)
				{
					Variables.Add(V->AsObject());
				}
			}
		}
	}

	/** 按 Id 查找节点 */
	const FSkillDataNode* FindNode(const FString& InId) const
	{
		for (const auto& N : Nodes)
		{
			if (N.Id == InId) return &N;
		}
		return nullptr;
	}

	/** 查找从某节点某分支出发的所有 Transition */
	TArray<TSharedPtr<FJsonObject>> FindOutgoing(const FString& NodeId, const FString& BranchId = TEXT("out")) const
	{
		TArray<TSharedPtr<FJsonObject>> Result;
		for (const auto& T : Transitions)
		{
			if (T->GetStringField(TEXT("sourceNodeId")) == NodeId)
			{
				// sourceHandleId 可能为空，默认 "out"
				FString Handle = TEXT("out");
				FString Temp;
				if (T->TryGetStringField(TEXT("sourceHandleId"), Temp))
				{
					Handle = Temp;
				}
				if (Handle == BranchId)
				{
					Result.Add(T);
				}
			}
		}
		return Result;
	}

	/** 查找 Entry 节点（nodeKind=flow, flowType=Entry） */
	const FSkillDataNode* FindEntry() const
	{
		for (const auto& N : Nodes)
		{
			if (N.Kind == TEXT("flow") && N.SubType == TEXT("Entry"))
			{
				return &N;
			}
		}
		return nullptr;
	}
};
