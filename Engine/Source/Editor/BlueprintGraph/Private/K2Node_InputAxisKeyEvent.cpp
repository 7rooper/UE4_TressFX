// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "CompilerResultsLog.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "Engine/InputAxisKeyDelegateBinding.h"

#define LOCTEXT_NAMESPACE "UK2Node_InputAxisKeyEvent"

UK2Node_InputAxisKeyEvent::UK2Node_InputAxisKeyEvent(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bConsumeInput = true;
	bOverrideParentBinding = true;
	bInternalEvent = true;

	EventSignatureName = TEXT("InputAxisHandlerDynamicSignature__DelegateSignature");
	EventSignatureClass = UInputComponent::StaticClass();
}

void UK2Node_InputAxisKeyEvent::Initialize(const FKey InAxisKey)
{
	AxisKey = InAxisKey;
	CustomFunctionName = FName(*FString::Printf(TEXT("InpAxisKeyEvt_%s_%s"), *AxisKey.ToString(), *GetName()));
}

FText UK2Node_InputAxisKeyEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return AxisKey.GetDisplayName();
}

FString UK2Node_InputAxisKeyEvent::GetTooltip() const
{
	return FString::Printf(*NSLOCTEXT("K2Node", "InputAxisKey_Tooltip", "Event that provides the current value of the %s axis once per frame when input is enabled for the containing actor.").ToString(), *AxisKey.GetDisplayName().ToString());
}

void UK2Node_InputAxisKeyEvent::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (!AxisKey.IsValid())
	{
		MessageLog.Warning(*FText::Format(NSLOCTEXT("KismetCompiler", "Invalid_InputAxisKey_Warning", "InputAxisKey Event specifies invalid FKey'{0}' for @@"), FText::FromString(AxisKey.ToString())).ToString(), this);
	}
	else if (!AxisKey.IsFloatAxis())
	{
		MessageLog.Warning(*FText::Format(NSLOCTEXT("KismetCompiler", "NotAxis_InputAxisKey_Warning", "InputAxisKey Event specifies FKey'{0}' which is not a float axis for @@"), FText::FromString(AxisKey.ToString())).ToString(), this);
	}
	else if (!AxisKey.IsBindableInBlueprints())
	{
		MessageLog.Warning(*FText::Format(NSLOCTEXT("KismetCompiler", "NotBindable_InputAxisKey_Warning", "InputAxisKey Event specifies FKey'{0}' that is not blueprint bindable for @@"), FText::FromString(AxisKey.ToString())).ToString(), this);
	}
}

UClass* UK2Node_InputAxisKeyEvent::GetDynamicBindingClass() const
{
	return UInputAxisKeyDelegateBinding::StaticClass();
}

FName UK2Node_InputAxisKeyEvent::GetPaletteIcon(FLinearColor& OutColor) const
{
	if (AxisKey.IsMouseButton())
	{
		return TEXT("GraphEditor.MouseEvent_16x");
	}
	else if (AxisKey.IsGamepadKey())
	{
		return TEXT("GraphEditor.PadEvent_16x");
	}
	else
	{
		return TEXT("GraphEditor.KeyEvent_16x");
	}
}

void UK2Node_InputAxisKeyEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UInputAxisKeyDelegateBinding* InputAxisKeyBindingObject = CastChecked<UInputAxisKeyDelegateBinding>(BindingObject);

	FBlueprintInputAxisKeyDelegateBinding Binding;
	Binding.AxisKey = AxisKey;
	Binding.bConsumeInput = bConsumeInput;
	Binding.bExecuteWhenPaused = bExecuteWhenPaused;
	Binding.bOverrideParentBinding = bOverrideParentBinding;
	Binding.FunctionNameToBind = CustomFunctionName;

	InputAxisKeyBindingObject->InputAxisKeyDelegateBindings.Add(Binding);
}

bool UK2Node_InputAxisKeyEvent::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	// By default, to be safe, we don't allow events to be pasted, except under special circumstances (see below)
	bool bIsCompatible = false;
	
	// Find the Blueprint that owns the target graph
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	if (Blueprint && Blueprint->SkeletonGeneratedClass)
	{
		bIsCompatible = Blueprint->ParentClass->IsChildOf(AActor::StaticClass());
	}

	return bIsCompatible && Super::IsCompatibleWithGraph(TargetGraph);
}

void UK2Node_InputAxisKeyEvent::GetMenuActions(TArray<UBlueprintNodeSpawner*>& ActionListOut) const
{
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);

	for (FKey const Key : AllKeys)
	{
		if (!Key.IsBindableInBlueprints() || !Key.IsFloatAxis())
		{
			continue;
		}

		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		auto CustomizeInputNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FKey Key)
		{
			UK2Node_InputAxisKeyEvent* InputNode = CastChecked<UK2Node_InputAxisKeyEvent>(NewNode);
			InputNode->Initialize(Key);
		};

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeInputNodeLambda, Key);
		ActionListOut.Add(NodeSpawner);
	}
}

FText UK2Node_InputAxisKeyEvent::GetMenuCategory() const
{
	FText SubCategory;
	if (AxisKey.IsGamepadKey())
	{
		SubCategory = LOCTEXT("GamepadCategory", "Gamepad Events");
	}
	else if (AxisKey.IsMouseButton())
	{
		SubCategory = LOCTEXT("MouseCategory", "Mouse Events");
	}
	else
	{
		SubCategory = LOCTEXT("KeyEventsCategory", "Key Events");
	}

	return FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Input, SubCategory);

}

#undef LOCTEXT_NAMESPACE
