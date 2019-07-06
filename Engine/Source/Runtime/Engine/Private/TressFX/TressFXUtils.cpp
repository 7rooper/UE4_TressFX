
#include "TressFX/TressFXUtils.h"
#include "Components/SkeletalMeshComponent.h"


int32 FTressFXUtils::FindEngineBoneIndex(const USkeletalMesh* SkelMesh, const FName BoneName)
{
	// NOTE: FindBoneIndex should also include virtual bones

	const FReferenceSkeleton& MeshRefSkel = SkelMesh->RefSkeleton;
	int32 Index = MeshRefSkel.FindBoneIndex(BoneName) != INDEX_NONE;

	if (Index != INDEX_NONE)
	{
		return Index;
	}
	else
	{
		FString PossibleVirtualBoneName = "VB " + BoneName.ToString();
		return MeshRefSkel.FindBoneIndex(FName(*PossibleVirtualBoneName));
	}
}

bool FTressFXUtils::IsCompatibleSkeleton(const USkeletalMesh* InSkelMesh, TMap<int32, FName>& InBoneNameIndexMap)
{
	const FReferenceSkeleton& MeshRefSkel = InSkelMesh->RefSkeleton;

	for (const TPair<int32, FName> TfxBone : InBoneNameIndexMap)
	{
		// if even one bone matches, return true
		if (FTressFXUtils::FindEngineBoneIndex(InSkelMesh, TfxBone.Value))
		{
			return true;
		}
	}
	return false;
}