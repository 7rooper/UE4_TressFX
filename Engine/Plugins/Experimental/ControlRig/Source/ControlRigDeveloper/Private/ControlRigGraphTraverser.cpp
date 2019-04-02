// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphTraverser.h"
#include "Units/RigUnit.h"
#include "ControlRig/Private/Units/Execution/RigUnit_BeginExecution.h"

FControlRigGraphTraverser::FControlRigGraphTraverser(UControlRigBlueprint* InBlueprint, UControlRigGraph* InGraph)
	: Blueprint(InBlueprint)
	, Graph(InGraph)
{
}

#if WITH_EDITOR
bool FControlRigGraphTraverser::IsWiredToExecution(const FName& UnitName)
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(Node))
		{
			if (RigNode->GetPropertyName().IsEqual(UnitName))
			{
				return IsWiredToExecution(RigNode);
			}
		}
	}
	return false;
}
#endif

bool FControlRigGraphTraverser::IsWiredToExecution(UControlRigGraphNode* Node)
{
	if (Node == nullptr)
	{
		return false;
	}

	const bool* Found = VisitedNodes.Find(Node);
	if (Found)
	{
		return *Found;
	}

	if (Node->GetUnitScriptStruct() == FRigUnit_BeginExecution::StaticStruct())
	{
		VisitedNodes.Add(Node, true);
		return true;
	}

	VisitedNodes.Add(Node, false);

	// is this an execution node,
	// walk upwards (to the left) to find a proper begin execution node
	if (Node->GetUnitScriptStruct()->IsChildOf(FRigUnitMutable::StaticStruct()))
	{
		const TArray<TSharedRef<FControlRigField>> ExecutionInfos = Node->GetExecutionVariableInfo();
		for (const TSharedRef<FControlRigField>& ExecutionInfo : ExecutionInfos)
		{
			if (ExecutionInfo->InputPin == nullptr)
			{
				continue;
			}
			for (UEdGraphPin* LinkedPin : ExecutionInfo->InputPin->LinkedTo)
			{
				UControlRigGraphNode* LinkedNode = Cast<UControlRigGraphNode>(LinkedPin->GetOwningNode());
				if (LinkedNode)
				{
					bool Result = IsWiredToExecution(LinkedNode);
					VisitedNodes.FindChecked(Node) = Result;
					return Result;
				}
			}
		}

		return false;
	}

	// for all other nodes walk  to the right...
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->Direction != EEdGraphPinDirection::EGPD_Output)
		{
			continue;
		}
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			UControlRigGraphNode* LinkedNode = Cast<UControlRigGraphNode>(LinkedPin->GetOwningNode());
			if (LinkedNode)
			{
				bool Result = IsWiredToExecution(LinkedNode);
				VisitedNodes.FindChecked(Node) = Result;
				return Result;
			}
		}
	}

	return false;
}

void FControlRigGraphTraverser::TraverseAndBuildPropertyLinks()
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UControlRigGraphNode* RigNode = Cast< UControlRigGraphNode>(Node);
		if (RigNode == nullptr)
		{
			continue;
		}

		if (!IsWiredToExecution(RigNode))
		{
			continue;
		}

		for (UEdGraphPin* Pin : RigNode->Pins)
		{
			if (Pin->Direction == EGPD_Output)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					int32 PinIndex = RigNode->Pins.IndexOfByKey(Pin);
					int32 LinkedPinIndex = LinkedPin->GetOwningNode()->Pins.IndexOfByKey(LinkedPin);
					Blueprint->MakePropertyLink(Pin->PinName.ToString(), LinkedPin->PinName.ToString(), PinIndex, LinkedPinIndex);
				}
			}
		}
	}
}
