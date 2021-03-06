// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SubclassOf.h"
#include "FoliageType.h"
#include "FoliageType_Actor.generated.h"

UCLASS(hidecategories = Object, editinlinenew, MinimalAPI)
class UFoliageType_Actor : public UFoliageType
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(EditAnywhere, Category = Actor)
	TSubclassOf<AActor> ActorClass;
			
	virtual UObject* GetSource() const override { return ActorClass; }

#if WITH_EDITOR
	virtual void UpdateBounds();
	virtual bool IsSourcePropertyChange(const UProperty* Property) const override
	{
		return Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UFoliageType_Actor, ActorClass);
	}
	virtual void SetSource(UObject* InSource) override
	{
		ActorClass = Cast<UClass>(InSource);
		UpdateBounds();
	}
#endif
};