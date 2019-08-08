
#include "TressFX/TressFXUtils.h"
#include "Components/SkeletalMeshComponent.h"

int32 FTressFXUtils::FindEngineBoneIndex(const USkeletalMesh* SkelMesh, const FName BoneName, bool bSuppportVirtualBones /*= true*/)
{
	// NOTE: FindBoneIndex should also include virtual bones
	const FReferenceSkeleton& MeshRefSkel = SkelMesh->RefSkeleton;
	int32 Index = MeshRefSkel.FindBoneIndex(BoneName);

	if (Index != INDEX_NONE)
	{
		return Index;
	}
	else if(bSuppportVirtualBones)
	{
		FString PossibleVirtualBoneName = "VB " + BoneName.ToString();
		const int32 VirtualBoneIndex = MeshRefSkel.FindBoneIndex(FName(*PossibleVirtualBoneName));
		if (VirtualBoneIndex != INDEX_NONE) 
		{
			const int32 ParentIndex = MeshRefSkel.GetRefBoneInfo()[VirtualBoneIndex].ParentIndex;
			return ParentIndex;
		}
	}
	return INDEX_NONE;
}

bool FTressFXUtils::IsCompatibleSkeleton(const USkeletalMesh* InSkelMesh, TMap<int32, FName>& InBoneNameIndexMap, bool bSuppportVirtualBones)
{
	const FReferenceSkeleton& MeshRefSkel = InSkelMesh->RefSkeleton;

	for (const TPair<int32, FName> TfxBone : InBoneNameIndexMap)
	{
		// if even one bone matches, return true
		if (FTressFXUtils::FindEngineBoneIndex(InSkelMesh, TfxBone.Value, bSuppportVirtualBones) != INDEX_NONE)
		{
			return true;
		}
	}
	return false;
}