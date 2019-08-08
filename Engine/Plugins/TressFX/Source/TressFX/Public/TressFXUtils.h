#pragma once
#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Containers/ResourceArray.h"
#include "Animation/Skeleton.h"
#include "RHIResources.h"


class TRESSFX_API FTressFXUtils
{
public:

	// finds engine bone index, supports virtual bones if passed
	static int32 FindEngineBoneIndex(const USkeletalMesh* SkelMesh, const FName BoneName, bool bSuppportVirtualBones);

	// Return true if even at least one bone is a match  supports virtual bones if passed
	static bool IsCompatibleSkeleton(const USkeletalMesh* InSkelMesh, TMap<int32, FName>& InBoneNameIndexMap, bool bSuppportVirtualBones);
};
