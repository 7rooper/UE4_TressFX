// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;

/** Details customization for rigid body selection. */
class FSelectedRigidBodyCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	FReply OnPick(TSharedRef<IPropertyHandle> PropertyHandleId) const;
};
