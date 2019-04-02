
#include "Engine/TressFXBoneSkinningAsset.h"
#include "Engine/TressFXAsset.h"
#include "TressFX/TressFXTypes.h"
#include "Animation/Skeleton.h"

struct FTressFXTFXBoneFileHeader
{
	float version;
	uint32 numHairStrands;
	uint32 numInfluenceBones;
	uint32 offsetBoneNames;
	uint32 offsetSkinningData;
	uint32 reserved[32];
};

UTressFXBoneSkinningAsset::UTressFXBoneSkinningAsset(const class FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{

}

void UTressFXBoneSkinningAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << RawData << AssetType << JsonVersionImportData;
}
