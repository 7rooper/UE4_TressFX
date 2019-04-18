// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_Layers.h"
#include "IDetailChildrenBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Brushes/SlateColorBrush.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"
#include "Landscape.h"

#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "AssetRegistryModule.h"

#include "LandscapeRender.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "LandscapeEdit.h"
#include "IDetailGroup.h"
#include "Widgets/SBoxPanel.h"
#include "Editor/EditorStyle/Private/SlateEditorStyle.h"
#include "LandscapeEditorDetailCustomization_TargetLayers.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "LandscapeEditorCommands.h"
#include "Settings/EditorExperimentalSettings.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.Layers"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_Layers::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_Layers);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_Layers::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& LayerCategory = DetailBuilder.EditCategory("Layers");

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolMode != nullptr)
	{
		if (LandscapeEdMode->GetLandscape())
		{
			LayerCategory.AddCustomBuilder(MakeShareable(new FLandscapeEditorCustomNodeBuilder_Layers(DetailBuilder.GetThumbnailPool().ToSharedRef())));

			LayerCategory.AddCustomRow(FText())
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([]() { return ShoudShowLayersErrorMessageTip() ? EVisibility::Visible : EVisibility::Collapsed; })))
				[
					SNew(SMultiLineEditableTextBox)
					.Font(DetailBuilder.GetDetailFontBold())
					.BackgroundColor(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([]() { return FEditorStyle::GetColor("ErrorReporting.WarningBackgroundColor"); })))
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&FLandscapeEditorDetailCustomization_Layers::GetLayersErrorMessageText)))
					.AutoWrapText(true)
				];
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool FLandscapeEditorDetailCustomization_Layers::ShoudShowLayersErrorMessageTip()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		return !LandscapeEdMode->CanEditLayer();
	}
	return false;
}

FText FLandscapeEditorDetailCustomization_Layers::GetLayersErrorMessageText()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	FText Reason;
	if (LandscapeEdMode && !LandscapeEdMode->CanEditLayer(&Reason))
	{
		return Reason;
	}
	return FText::GetEmpty();
}

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorCustomNodeBuilder_Layers::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FLandscapeEditorCustomNodeBuilder_Layers::FLandscapeEditorCustomNodeBuilder_Layers(TSharedRef<FAssetThumbnailPool> InThumbnailPool)
	: ThumbnailPool(InThumbnailPool)
	, CurrentEditingInlineTextBlock(INDEX_NONE)
	, CurrentSlider(INDEX_NONE)
{
}

FLandscapeEditorCustomNodeBuilder_Layers::~FLandscapeEditorCustomNodeBuilder_Layers()
{
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
}

void FLandscapeEditorCustomNodeBuilder_Layers::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode == NULL)
	{
		return;
	}

	NodeRow.NameWidget
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(FText::FromString(TEXT("Layers")))
		];
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorCustomNodeBuilder_Layers::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		TSharedPtr<SDragAndDropVerticalBox> LayerList = SNew(SDragAndDropVerticalBox)
			.OnCanAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_Layers::HandleCanAcceptDrop)
			.OnAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_Layers::HandleAcceptDrop)
			.OnDragDetected(this, &FLandscapeEditorCustomNodeBuilder_Layers::HandleDragDetected);

		LayerList->SetDropIndicator_Above(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Above"));
		LayerList->SetDropIndicator_Below(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Below"));

		ChildrenBuilder.AddCustomRow(FText::FromString(FString(TEXT("Layers"))))
			.Visibility(EVisibility::Visible)
			[
				LayerList.ToSharedRef()
			];

		InlineTextBlocks.Empty();
		InlineTextBlocks.AddDefaulted(LandscapeEdMode->GetLayerCount());
		for (int32 i = 0; i < LandscapeEdMode->GetLayerCount(); ++i)
		{
			TSharedPtr<SWidget> GeneratedRowWidget = GenerateRow(i);

			if (GeneratedRowWidget.IsValid())
			{
				LayerList->AddSlot()
					.AutoHeight()
					[
						GeneratedRowWidget.ToSharedRef()
					];
			}
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_Layers::GenerateRow(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	TSharedPtr<SWidget> RowWidget = SNew(SLandscapeEditorSelectableBorder)
		.Padding(0)
		.VAlign(VAlign_Center)
		.OnContextMenuOpening(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnLayerContextMenuOpening, InLayerIndex)
		.OnSelected(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnLayerSelectionChanged, InLayerIndex)
		.IsSelected(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerSelected, InLayerIndex)))
		.Visibility(EVisibility::Visible)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.OnClicked(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnToggleLock, InLayerIndex)
				.ToolTipText(LOCTEXT("LandscapeLayerLock", "Locks the current layer"))
				[
					SNew(SImage)
					.Image(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLockBrushForLayer, InLayerIndex)
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.OnClicked(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnToggleVisibility, InLayerIndex)
				.ToolTipText(LOCTEXT("LandscapeLayerVisibility", "Toggle Layer Visibility"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Content()
				[
					SNew(SImage)
			.Image(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetVisibilityBrushForLayer, InLayerIndex)
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0)
			.VAlign(VAlign_Center)
			.Padding(4, 0)
			[
				SAssignNew(InlineTextBlocks[InLayerIndex], SInlineEditableTextBlock)
				.IsEnabled(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled, InLayerIndex)
				.Text(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerDisplayName, InLayerIndex)
				.ToolTipText(LOCTEXT("LandscapeLayers_tooltip", "Name of the Layer"))
				.OnVerifyTextChanged(FOnVerifyTextChanged::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::CanRenameLayerTo, InLayerIndex))
				.OnEnterEditingMode(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnBeginNameTextEdit)
				.OnExitEditingMode(this, &FLandscapeEditorCustomNodeBuilder_Layers::OnEndNameTextEdit)
				.OnTextCommitted(FOnTextCommitted::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_Layers::SetLayerName, InLayerIndex))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0, 2)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0)
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.IsEnabled(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled, InLayerIndex)
					.Visibility(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlphaVisibility, InLayerIndex)
					.Text(LOCTEXT("LandscapeLayerAlpha", "Alpha"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(0, 2)
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.0f)
					.MaxValue(100.0f)
					.MaxSliderValue(100.0f)
					.MinDesiredValueWidth(60.0f)
					.IsEnabled(this, &FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled, InLayerIndex)
					.Visibility(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlphaVisibility, InLayerIndex)
					.Value(this, &FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlpha, InLayerIndex)
					.OnValueChanged_Lambda([=](float InValue) { SetLayerAlpha(InValue, InLayerIndex, false); })
					.OnValueCommitted_Lambda([=](float InValue, ETextCommit::Type InCommitType) { SetLayerAlpha(InValue, InLayerIndex, true); })
					.OnBeginSliderMovement_Lambda([=]()
					{
						CurrentSlider = InLayerIndex;
						GEditor->BeginTransaction(LOCTEXT("Landscape_Layers_SetAlpha", "Set Layer Alpha"));
					})
					.OnEndSliderMovement_Lambda([=](double)
					{
						GEditor->EndTransaction();
						CurrentSlider = INDEX_NONE;
					})
				]		
			]
		];	

	return RowWidget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText FLandscapeEditorCustomNodeBuilder_Layers::GetLayerDisplayName(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (LandscapeEdMode && Landscape)
	{
		const FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		const FLandscapeLayer* ReservedLayer = Landscape->GetLandscapeSplinesReservedLayer();
		bool bIsReserved = Layer && Layer == ReservedLayer && InLayerIndex != CurrentEditingInlineTextBlock;
		return FText::Format(LOCTEXT("LayerDisplayName", "{0}{1}"), FText::FromName(LandscapeEdMode->GetLayerName(InLayerIndex)), bIsReserved ? LOCTEXT("ReservedForSplines", " (Reserved for Splines)") : FText::GetEmpty());
	}

	return FText::FromString(TEXT("None"));
}

bool FLandscapeEditorCustomNodeBuilder_Layers::IsLayerSelected(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		return LandscapeEdMode->GetCurrentLayerIndex() == InLayerIndex;
	}

	return false;
}

void FLandscapeEditorCustomNodeBuilder_Layers::OnBeginNameTextEdit()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	CurrentEditingInlineTextBlock = LandscapeEdMode ? LandscapeEdMode->GetCurrentLayerIndex() : INDEX_NONE;
}

void FLandscapeEditorCustomNodeBuilder_Layers::OnEndNameTextEdit()
{
	CurrentEditingInlineTextBlock = INDEX_NONE;
}

bool FLandscapeEditorCustomNodeBuilder_Layers::CanRenameLayerTo(const FText& InNewText, FText& OutErrorMessage, int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		if (!LandscapeEdMode->CanRenameLayerTo(InLayerIndex, *InNewText.ToString()))
		{
			OutErrorMessage = LOCTEXT("Landscape_Layers_RenameFailed_AlreadyExists", "This layer already exists");
			return false;
		}
	}
	return true;
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetLayerName(const FText& InText, ETextCommit::Type InCommitType, int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Rename", "Rename Layer"));
		LandscapeEdMode->SetLayerName(InLayerIndex, *InText.ToString());
	}
}

TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_Layers::OnLayerContextMenuOpening(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (LandscapeEdMode && Landscape)
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		TSharedRef<FLandscapeEditorCustomNodeBuilder_Layers> SharedThis = AsShared();
		FMenuBuilder MenuBuilder(true, NULL);
		MenuBuilder.BeginSection("LandscapeEditorLayerActions", LOCTEXT("LandscapeEditorLayerActions.Heading", "Layers"));
		{
			// Create Layer
			FUIAction CreateLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis] { SharedThis->CreateLayer(); }));
			MenuBuilder.AddMenuEntry(LOCTEXT("CreateLayer", "Create"), LOCTEXT("CreateLayerTooltip", "Create Layer"), FSlateIcon(), CreateLayerAction);

			if (Layer)
			{
				// Rename Layer
				FUIAction RenameLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->RenameLayer(InLayerIndex); }));
				MenuBuilder.AddMenuEntry(LOCTEXT("RenameLayer", "Rename..."), LOCTEXT("RenameLayerTooltip", "Rename Layer"), FSlateIcon(), RenameLayerAction);

				if (!Layer->bLocked)
				{
					// Clear Layer
					FUIAction ClearLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->ClearLayer(InLayerIndex); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("ClearLayer", "Clear..."), LOCTEXT("ClearLayerTooltip", "Clear Layer"), FSlateIcon(), ClearLayerAction);

					if (Landscape->LandscapeLayers.Num() > 1)
					{
						// Delete Layer
						FUIAction DeleteLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->DeleteLayer(InLayerIndex); } ));
						MenuBuilder.AddMenuEntry(LOCTEXT("DeleteLayer", "Delete..."), LOCTEXT("DeleteLayerTooltip", "Delete Layer"), FSlateIcon(), DeleteLayerAction);
					}
				}

				if (Landscape->GetLandscapeSplinesReservedLayer() != Landscape->GetLayer(InLayerIndex))
				{
					// Reserve for Landscape Splines
					FUIAction ReserveLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->SetLandscapeSplinesReservedLayer(InLayerIndex); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("ReserveLayerForSplines", "Reserve for Splines"), LOCTEXT("ReserveLayerForSplinesTooltip", "Reserve Layer for Landscape Splines"), FSlateIcon(), ReserveLayerAction);
				}
				else
				{
					FUIAction RemoveReserveLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->SetLandscapeSplinesReservedLayer(INDEX_NONE); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("RemoveReserveLayerForSplines", "Remove Reserve for Splines"), LOCTEXT("RemoveReserveLayerForSplinesTooltip", "Remove reservation of Layer for Landscape Splines"), FSlateIcon(), RemoveReserveLayerAction);
				}
			}
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("LandscapeEditorLayerVisibility", LOCTEXT("LandscapeEditorLayerVisibility.Heading", "Visibility"));
		{
			if (Layer)
			{
				if (Layer->bVisible)
				{
					// Hide Selected Layer
					FUIAction HideSelectedLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->OnToggleVisibility(InLayerIndex); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("HideSelectedLayer", "Hide Selected"), LOCTEXT("HideSelectedLayerTooltip", "Hide Selected Layer"), FSlateIcon(), HideSelectedLayerAction);
				}
				else
				{
					// Show Selected Layer
					FUIAction ShowSelectedLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->OnToggleVisibility(InLayerIndex); }));
					MenuBuilder.AddMenuEntry(LOCTEXT("ShowSelectedLayer", "Show Selected"), LOCTEXT("ShowSelectedLayerTooltip", "Show Selected Layer"), FSlateIcon(), ShowSelectedLayerAction);
				}

				// Show Only Selected Layer
				FUIAction ShowOnlySelectedLayerAction = FUIAction(FExecuteAction::CreateLambda([SharedThis, InLayerIndex] { SharedThis->ShowOnlySelectedLayer(InLayerIndex); }));
				MenuBuilder.AddMenuEntry(LOCTEXT("ShowOnlySelectedLayer", "Show Only Selected"), LOCTEXT("ShowOnlySelectedLayerTooltip", "Show Only Selected Layer"), FSlateIcon(), ShowOnlySelectedLayerAction);
			}

			// Show All Layers
			FUIAction ShowAllLayersAction = FUIAction(FExecuteAction::CreateLambda([SharedThis] { SharedThis->ShowAllLayers(); }));
			MenuBuilder.AddMenuEntry(LOCTEXT("ShowAllLayers", "Show All Layers"), LOCTEXT("ShowAllLayersTooltip", "Show All Layers"), FSlateIcon(), ShowAllLayersAction);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}
	return NULL;
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetLandscapeSplinesReservedLayer(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		const FLandscapeLayer* CurrenReservedLayer = Landscape->GetLandscapeSplinesReservedLayer();
		const FLandscapeLayer* NewReservedLayer = Landscape->GetLayer(InLayerIndex);
		if (NewReservedLayer != CurrenReservedLayer)
		{
			EAppReturnType::Type Result = EAppReturnType::No;
			if (NewReservedLayer)
			{
				Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_SetReservedForSplines_Message", "Reserving layer {0} for landscape splines will clear it from its content and no edition will be allowed.  Continue?"), FText::FromName(NewReservedLayer->Name)));
			}
			else if (CurrenReservedLayer)
			{
				Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_UnsetReservedForSplines_Message", "Removing reservation of layer {0} for landscape splines will clear its content.  Continue?"), FText::FromName(CurrenReservedLayer->Name)));
			}

			if (Result == EAppReturnType::Yes)
			{
				const FScopedTransaction Transaction(LOCTEXT("ReserveForSplines", "Reserve Landscape Layer for Splines"));
				Landscape->SetLandscapeSplinesReservedLayer(InLayerIndex);
				LandscapeEdMode->RefreshDetailPanel();
				LandscapeEdMode->AutoUpdateDirtyLandscapeSplines();
			}
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::RenameLayer(int32 InLayerIndex)
{
	if (InlineTextBlocks.IsValidIndex(InLayerIndex) && InlineTextBlocks[InLayerIndex].IsValid())
	{
		InlineTextBlocks[InLayerIndex]->EnterEditingMode();
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::ClearLayer(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		if (Layer)
		{
			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_ClearLayer_Message", "The layer {0} content will be completely cleared.  Continue?"), FText::FromName(Layer->Name)));
			if (Result == EAppReturnType::Yes)
			{
				const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Clean", "Clear Layer"));
				Landscape->ClearLayer(InLayerIndex);
			}
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::DeleteLayer(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape && Landscape->LandscapeLayers.Num() > 1)
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(InLayerIndex);
		if (Layer)
		{
			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("Landscape_DeleteLayer_Message", "The layer {0} will be deleted.  Continue?"), FText::FromName(Layer->Name)));
			if (Result == EAppReturnType::Yes)
			{
				const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Delete", "Delete Layer"));
				Landscape->DeleteLayer(InLayerIndex);
				int32 NewLayerSelectionIndex = Landscape->GetLayer(InLayerIndex) ? InLayerIndex : 0;
				OnLayerSelectionChanged(NewLayerSelectionIndex);
				LandscapeEdMode->RefreshDetailPanel();
			}
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::ShowOnlySelectedLayer(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		const FScopedTransaction Transaction(LOCTEXT("ShowOnlySelectedLayer", "Show Only Selected Layer"));
		Landscape->ShowOnlySelectedLayer(InLayerIndex);
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::ShowAllLayers()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		const FScopedTransaction Transaction(LOCTEXT("ShowAllLayers", "Show All Layers"));
		Landscape->ShowAllLayers();
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::CreateLayer()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	if (Landscape)
	{
		{
			const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Create", "Create Layer"));
			Landscape->CreateLayer();
		}
		LandscapeEdMode->RefreshDetailPanel();
	}
}

void FLandscapeEditorCustomNodeBuilder_Layers::OnLayerSelectionChanged(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_SetCurrentLayer", "Set Current Layer"));
		LandscapeEdMode->SetCurrentLayer(InLayerIndex);
		LandscapeEdMode->UpdateTargetList();
	}
}

TOptional<float> FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlpha(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode)
	{
		return LandscapeEdMode->GetLayerAlpha(InLayerIndex);
	}

	return 1.0f;
}

void FLandscapeEditorCustomNodeBuilder_Layers::SetLayerAlpha(float InAlpha, int32 InLayerIndex, bool bCommit)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		// We get multiple commits when editing through the text box
		if (LandscapeEdMode->GetLayerAlpha(InLayerIndex) == InAlpha)
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_SetAlpha", "Set Layer Alpha"), CurrentSlider == INDEX_NONE && bCommit);
		// Set Value when using slider or when committing text
		LandscapeEdMode->SetLayerAlpha(InLayerIndex, InAlpha);
	}
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::OnToggleVisibility(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_SetVisibility", "Set Layer Visibility"));
		LandscapeEdMode->SetLayerVisibility(!LandscapeEdMode->IsLayerVisible(InLayerIndex), InLayerIndex);
	}
	return FReply::Handled();
}

const FSlateBrush* FLandscapeEditorCustomNodeBuilder_Layers::GetVisibilityBrushForLayer(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	bool bIsVisible = LandscapeEdMode && LandscapeEdMode->IsLayerVisible(InLayerIndex);
	return bIsVisible ? FEditorStyle::GetBrush("Level.VisibleIcon16x") : FEditorStyle::GetBrush("Level.NotVisibleIcon16x");
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::OnToggleLock(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Locked", "Set Layer Locked"));
		LandscapeEdMode->SetLayerLocked(InLayerIndex, !LandscapeEdMode->IsLayerLocked(InLayerIndex));
	}
	return FReply::Handled();
}

EVisibility FLandscapeEditorCustomNodeBuilder_Layers::GetLayerAlphaVisibility(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	bool bIsVisible = LandscapeEdMode && LandscapeEdMode->IsLayerAlphaVisible(InLayerIndex);
	return bIsVisible ? EVisibility::Visible : EVisibility::Hidden;
}

bool FLandscapeEditorCustomNodeBuilder_Layers::IsLayerEditionEnabled(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
	const FLandscapeLayer* Layer = LandscapeEdMode ? LandscapeEdMode->GetLayer(InLayerIndex) : nullptr;
	const FLandscapeLayer* LayerReservedForSplines = Landscape ? Landscape->GetLandscapeSplinesReservedLayer() : nullptr;
	return Layer && !Layer->bLocked && (Layer != LayerReservedForSplines);
}

const FSlateBrush* FLandscapeEditorCustomNodeBuilder_Layers::GetLockBrushForLayer(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	bool bIsLocked = LandscapeEdMode && LandscapeEdMode->IsLayerLocked(InLayerIndex);
	return bIsLocked ? FEditorStyle::GetBrush(TEXT("PropertyWindow.Locked")) : FEditorStyle::GetBrush(TEXT("PropertyWindow.Unlocked"));
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		FLandscapeLayer* Layer = LandscapeEdMode->GetLayer(SlotIndex);
		if (Layer && !Layer->bLocked)
		{
			TSharedPtr<SWidget> Row = GenerateRow(SlotIndex);
			if (Row.IsValid())
			{
				return FReply::Handled().BeginDragDrop(FLandscapeListElementDragDropOp::New(SlotIndex, Slot, Row));
			}
		}
	}
	return FReply::Unhandled();
}

TOptional<SDragAndDropVerticalBox::EItemDropZone> FLandscapeEditorCustomNodeBuilder_Layers::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FLandscapeListElementDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FLandscapeListElementDragDropOp>();
	if (DragDropOperation.IsValid())
	{
		return DropZone;
	}
	return TOptional<SDragAndDropVerticalBox::EItemDropZone>();
}

FReply FLandscapeEditorCustomNodeBuilder_Layers::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FLandscapeListElementDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FLandscapeListElementDragDropOp>();

	if (DragDropOperation.IsValid())
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		ALandscape* Landscape = LandscapeEdMode ? LandscapeEdMode->GetLandscape() : nullptr;
		if (Landscape)
		{
			int32 StartingLayerIndex = DragDropOperation->SlotIndexBeingDragged;
			int32 DestinationLayerIndex = SlotIndex;
			const FScopedTransaction Transaction(LOCTEXT("Landscape_Layers_Reorder", "Reorder Layer"));
			if (Landscape->ReorderLayer(StartingLayerIndex, DestinationLayerIndex))
			{
				LandscapeEdMode->SetCurrentLayer(DestinationLayerIndex);
				LandscapeEdMode->RefreshDetailPanel();
				LandscapeEdMode->RequestLayersContentUpdate();
				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

TSharedRef<FLandscapeListElementDragDropOp> FLandscapeListElementDragDropOp::New(int32 InSlotIndexBeingDragged, SVerticalBox::FSlot* InSlotBeingDragged, TSharedPtr<SWidget> WidgetToShow)
{
	TSharedRef<FLandscapeListElementDragDropOp> Operation = MakeShareable(new FLandscapeListElementDragDropOp);

	Operation->MouseCursor = EMouseCursor::GrabHandClosed;
	Operation->SlotIndexBeingDragged = InSlotIndexBeingDragged;
	Operation->SlotBeingDragged = InSlotBeingDragged;
	Operation->WidgetToShow = WidgetToShow;

	Operation->Construct();

	return Operation;
}

TSharedPtr<SWidget> FLandscapeListElementDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
		.Content()
		[
			WidgetToShow.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE