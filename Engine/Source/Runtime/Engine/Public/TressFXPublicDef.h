// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Tickable.h: Interface for tickable objects.

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "../Private/SkeletalRenderGPUSkin.h"

class ITressFXSceneProxy
{
public:
	virtual FUniformBufferRHIParamRef GetHairObjectShaderUniformBufferParam() = 0;
};