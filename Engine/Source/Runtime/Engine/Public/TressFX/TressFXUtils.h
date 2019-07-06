#pragma once
#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Containers/ResourceArray.h"
#include "Animation/Skeleton.h"
#include "RHIResources.h"


class ENGINE_API FTressFXUtils
{
public:

	// finds engine bone engine, supports virtual bones
	static int32 FindEngineBoneIndex(const USkeletalMesh* SkelMesh, const FName BoneName);

	// Return true if even at least one bone is a match
	static bool IsCompatibleSkeleton(const USkeletalMesh* InSkelMesh, TMap<int32, FName>& InBoneNameIndexMap);
};
