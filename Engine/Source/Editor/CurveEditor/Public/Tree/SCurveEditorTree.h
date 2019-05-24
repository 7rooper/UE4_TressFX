// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "CurveEditorTypes.h"

class FCurveEditor;
class ITableRow;
class SHeaderRow;
class STableViewBase;
class SCurveEditorTreeView;

class CURVEEDITOR_API SCurveEditorTree : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCurveEditorTree){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FCurveEditor> InCurveEditor);

private:

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:

	TSharedRef<ITableRow> GenerateRow(FCurveEditorTreeItemID ItemID, const TSharedRef<STableViewBase>& OwnerTable);

	void GetTreeItemChildren(FCurveEditorTreeItemID Parent, TArray<FCurveEditorTreeItemID>& OutChildren);

	void OnTreeSelectionChanged(FCurveEditorTreeItemID, ESelectInfo::Type);

	void SetItemExpansionRecursive(FCurveEditorTreeItemID Model, bool bInExpansionState);

private:

	TSharedPtr<FCurveEditor> CurveEditor;

	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedPtr<SCurveEditorTreeView> TreeView;
};