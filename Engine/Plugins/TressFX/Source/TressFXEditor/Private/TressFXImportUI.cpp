#pragma once

#include "Factories/TressFXImportUI.h"
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SSeparator.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/SecureHash.h"

#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "IDocumentation.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"

#include "Editor.h"
#include "Misc/PackageName.h"
#include "Interfaces/IMainFrameModule.h"
#include "AssetToolsModule.h"
#include "Misc/FileHelper.h"


#define LOCTEXT_NAMESPACE "TressFXFactory"

class SButton;

void STressFXImportWindow::Construct(const FArguments& InArgs, bool AllowCancel/* = true*/, FString InTitle /*= "TressFX Import"*/)
{
	AllowCancelClick = AllowCancel;
	ImportOptions.PickedImportRotation = FRotator(0, 0, 0);
	ImportOptions.PickedImportTranslation = FVector(0, 0, 0);

	SWindow::Construct(SWindow::FArguments()
		.Title(FText::FromString(InTitle))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		//.SizingRule( ESizingRule::Autosized )
		.ClientSize(FVector2D(450, 450))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot() // Add user input block
			.Padding(2)
			[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Import Rotation", "Import Rotation"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					]

				+ SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(3)
					[
						SNew(SRotatorInputBox)
						.Roll(this, &STressFXImportWindow::GetRotation, EAxis::X)
						.OnRollChanged(this, &STressFXImportWindow::SetRotation, EAxis::X)
						.Pitch(this, &STressFXImportWindow::GetRotation, EAxis::Y)
						.OnPitchChanged(this, &STressFXImportWindow::SetRotation, EAxis::Y)
						.Yaw(this, &STressFXImportWindow::GetRotation, EAxis::Z)
						.OnYawChanged(this, &STressFXImportWindow::SetRotation, EAxis::Z)
					]
				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Import Translation", "Import Translation"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					]

				+ SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(3)
					[
						SNew(SVectorInputBox)
						.X(this, &STressFXImportWindow::GetTranslation, EAxis::X)
						.OnXChanged(this, &STressFXImportWindow::SetTranslation, EAxis::X)
						.Y(this, &STressFXImportWindow::GetTranslation, EAxis::Y)
						.OnYChanged(this, &STressFXImportWindow::SetTranslation, EAxis::Y)
						.Z(this, &STressFXImportWindow::GetTranslation, EAxis::Z)
						.OnZChanged(this, &STressFXImportWindow::SetTranslation, EAxis::Z)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Import Scale", "Import Scale"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					]

				+ SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(3)
					[
						SNew(SVectorInputBox)
						.X(this, &STressFXImportWindow::GetScale, EAxis::X)
						.OnXChanged(this, &STressFXImportWindow::SetScale, EAxis::X)
						.Y(this, &STressFXImportWindow::GetScale, EAxis::Y)
						.OnYChanged(this, &STressFXImportWindow::SetScale, EAxis::Y)
						.Z(this, &STressFXImportWindow::GetScale, EAxis::Z)
						.OnZChanged(this, &STressFXImportWindow::SetScale, EAxis::Z)
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SSeparator)
					]
				]
			]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.Padding(5)
					[
						SNew(SUniformGridPanel)
							.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
							.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
							.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
							+ SUniformGridPanel::Slot(0, 0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
								.Text(LOCTEXT("Import", "Import"))
								.OnClicked(this, &STressFXImportWindow::OnButtonClick, EAppReturnType::Ok)
							]
							+ SUniformGridPanel::Slot(1, 0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.Text(LOCTEXT("Cancel", "Cancel"))
							.OnClicked(this, &STressFXImportWindow::OnButtonClick, EAppReturnType::Cancel)
							.IsEnabled(AllowCancelClick)
							]
					]
		]);
}

TOptional<float> STressFXImportWindow::GetRotation(EAxis::Type Axis) const
{
	if (Axis == EAxis::X)
	{
		return ImportOptions.PickedImportRotation.Roll;
	}
	else if (Axis == EAxis::Y)
	{
		return ImportOptions.PickedImportRotation.Pitch;
	}
	else if (Axis == EAxis::Z)
	{
		return ImportOptions.PickedImportRotation.Yaw;
	}
	return 0.0f;
}

void STressFXImportWindow::SetRotation(float NewRot, EAxis::Type Axis)
{
	if (Axis == EAxis::X)
	{
		ImportOptions.PickedImportRotation.Roll = NewRot;
	}
	else if (Axis == EAxis::Y)
	{
		ImportOptions.PickedImportRotation.Pitch = NewRot;
	}
	else if (Axis == EAxis::Z)
	{
		ImportOptions.PickedImportRotation.Yaw = NewRot;
	}
}

void STressFXImportWindow::SetTranslation(float NewTrans, EAxis::Type Axis)
{
	if (Axis == EAxis::X)
	{
		ImportOptions.PickedImportTranslation.X = NewTrans;
	}
	else if (Axis == EAxis::Y)
	{
		ImportOptions.PickedImportTranslation.Y = NewTrans;
	}
	else if (Axis == EAxis::Z)
	{
		ImportOptions.PickedImportTranslation.Z = NewTrans;
	}
}

TOptional<float> STressFXImportWindow::GetTranslation(EAxis::Type Axis) const
{
	if (Axis == EAxis::X)
	{
		return ImportOptions.PickedImportTranslation.X;
	}
	else if (Axis == EAxis::Y)
	{
		return ImportOptions.PickedImportTranslation.Y;
	}
	else if (Axis == EAxis::Z)
	{
		return ImportOptions.PickedImportTranslation.Z;
	}
	return 0.0f;
}

void STressFXImportWindow::SetScale(float NewScale, EAxis::Type Axis)
{
	if (Axis == EAxis::X)
	{
		ImportOptions.PickedScale.X = NewScale;
	}
	else if (Axis == EAxis::Y)
	{
		ImportOptions.PickedScale.Y = NewScale;
	}
	else if (Axis == EAxis::Z)
	{
		ImportOptions.PickedScale.Z = NewScale;
	}
}

TOptional<float> STressFXImportWindow::GetScale(EAxis::Type Axis) const
{
	if (Axis == EAxis::X)
	{
		return ImportOptions.PickedScale.X;
	}
	else if (Axis == EAxis::Y)
	{
		return ImportOptions.PickedScale.Y;
	}
	else if (Axis == EAxis::Z)
	{
		return ImportOptions.PickedScale.Z;
	}
	return 1.0f;
}


FReply STressFXImportWindow::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	if (ButtonID != EAppReturnType::Cancel)
	{
		//what do i want to do here?
	}

	RequestDestroyWindow();

	return FReply::Handled();
}

EAppReturnType::Type STressFXImportWindow::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

#undef LOCTEXT_NAMESPACE
