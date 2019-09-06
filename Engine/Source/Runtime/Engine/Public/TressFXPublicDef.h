// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "../Private/SkeletalRenderGPUSkin.h"

#ifndef TRESSFX_RAYTRACING
	#define TRESSFX_RAYTRACING 0
#endif
class ITressFXSceneProxy : public FPrimitiveSceneProxy
{
public:
	ITressFXSceneProxy(class UPrimitiveComponent* InComponent, FName ResourceName = NAME_None, class FTressFXHairObject* InHairObject = nullptr)
		: FPrimitiveSceneProxy(InComponent, ResourceName)
	{
	
	}
	virtual FRHIUniformBuffer* GetHairObjectShaderUniformBufferParam() = 0;
};