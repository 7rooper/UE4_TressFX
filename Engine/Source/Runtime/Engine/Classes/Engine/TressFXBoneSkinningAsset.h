
#pragma once
#include "EditorFramework/AssetImportData.h"
#include "CoreMinimal.h"
#include "TressFX/TressFXTypes.h"
#include "UObject/ObjectMacros.h"
#include "TressFXBoneSkinningAsset.generated.h"

struct ENGINE_API FTressFXBoneSkinningJSONImportData
{
public:

	FTressFXBoneSkinningJSONImportData() :BoneName(NAME_None), Weight(-1.0f) {}

	FName BoneName;
	float Weight;

	friend FArchive& operator << (FArchive& Ar, FTressFXBoneSkinningJSONImportData & Data)
	{
		return Ar << Data.BoneName << Data.Weight;
	}

	FTressFXBoneSkinningJSONImportData& operator = (const FTressFXBoneSkinningJSONImportData& Other)
	{
		this->BoneName = Other.BoneName;
		this->Weight = Other.Weight;
		return *this;
	}

};


/**
 *TressFX bone skinning data, literally just maps bones to bone weights in the parent mesh
 */
UCLASS(BlueprintType, Blueprintable)
class ENGINE_API UTressFXBoneSkinningAsset : public UObject
{

	GENERATED_UCLASS_BODY()

public:

	bool GenerateSkinningData(class UTressFXAsset* BaseAsset, class USkeleton* BaseSkeleton, TArray<FTressFXBoneSkinningData> &OutSkinningData);

	TArray<FTressFXBoneSkinningData> BoneSkinningData;

	//FTressFXBoneSkinngAssetType
	int32 AssetType;

	//only one of thse two will ever have data (RawData or JsonVersionImportData), switch on the AssetType enum
	TArray<uint8> RawData;

	struct FJsonVersionImportData
	{
		TArray<FTressFXBoneSkinningJSONImportData> JsonSkinningData;
		int32 NumBones;
		TArray<FName> BoneNames;
		FJsonVersionImportData& operator = (const FJsonVersionImportData& Other)
		{
			JsonSkinningData = Other.JsonSkinningData;
			NumBones = Other.NumBones;
			BoneNames = Other.BoneNames;
			return *this;
		}

		friend FArchive& operator << (FArchive& Ar, FJsonVersionImportData & Data)
		{
			return Ar << Data.JsonSkinningData << Data.NumBones << Data.BoneNames;
		}
	};

	FJsonVersionImportData JsonVersionImportData;


public:

	virtual void Serialize(FArchive& Ar) override;

protected:

private:

};
