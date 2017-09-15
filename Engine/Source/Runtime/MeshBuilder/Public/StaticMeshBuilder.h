// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshBuilder.h"

class MESHBUILDER_API FStaticMeshBuilder : public FMeshBuilder
{
public:
	FStaticMeshBuilder();

	virtual bool Build(class UStaticMesh* StaticMesh) override;
	virtual ~FStaticMeshBuilder() {}

private:

	void OnBuildRenderMeshStart(class UStaticMesh* StaticMesh, const bool bInvalidateLighting);
	void OnBuildRenderMeshFinish(class UStaticMesh* StaticMesh, const bool bRebuildBoundsAndCollision);

	/** Used to refresh all components in the scene that may be using a mesh we're editing */
	TSharedPtr<class FStaticMeshComponentRecreateRenderStateContext> RecreateRenderStateContext;
};

