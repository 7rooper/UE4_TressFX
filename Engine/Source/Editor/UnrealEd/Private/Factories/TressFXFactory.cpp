// ----------------------------------------------------------------------------
//
//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//This code was modified for use with Unreal Engine 4 by Leon Rosengarten 2018.

#include "Factories/TressFXFactory.h"
#include "Engine/TressFXAsset.h"
#include "Engine/TressFXBoneSkinningAsset.h"
#include "Factories/TressFXImportUI.h"
#include "Animation/Skeleton.h"

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/SecureHash.h"

#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "IDocumentation.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#include "Editor.h"
#include "Misc/PackageName.h"
#include "Interfaces/IMainFrameModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "Misc/FileHelper.h"

#include "Runtime/Json/Public/Dom/JsonObject.h"
#include "Runtime/Json/Public/Serialization/JsonReader.h"
#include "Runtime/Json/Public/Serialization/JsonSerializer.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"

struct FTressFXTFXFileHeader
{
	// Specifies TressFX version number
	float version;
	// Number of hair strands in this file. All strands in this file are guide strands.
	uint32 numHairStrands;        
	// Follow hair strands are generated procedurally.
	// From 4 to 64 inclusive (POW2 only). This should be a fixed value within tfx value.
	// The total vertices from the tfx file is numHairStrands * numVerticesPerStrand.
	uint32 numVerticesPerStrand; 										

	// Offsets to array data starts here. Offset values are in bytes, aligned on 8 bytes boundaries,
	// and relative to beginning of the .tfx file

	// Array size: FLOAT4[numHairStrands]
	uint32 offsetVertexPosition;
	// Array size: FLOAT2[numHairStrands], if 0 no texture coordinates
	uint32 offsetStrandUV;
	// Array size: FLOAT2[numHairStrands * numVerticesPerStrand], if 0, no per vertex texture coordinates
	uint32 offsetVertexUV;
	// Array size: float[numHairStrands]
	uint32 offsetStrandThickness;
	// Array size: FLOAT4[numHairStrands * numVerticesPerStrand], if 0, no vertex colors
	uint32 offsetVertexColor;      
	// Reserved for future versions
	uint32 reserved[32];           
};

UTressFXBoneSkinningAsset* ParseTFXBoneJson(TSharedPtr<FJsonObject> TFXBoneDataJson, UObject* InOuter, FString AssetName, int32& OutNumStrandsInfile)
{
	const TArray<TSharedPtr<FJsonValue>>* SkinDataArray;
	if (!(TFXBoneDataJson)->TryGetArrayField("skinningData", SkinDataArray))
	{
		return nullptr;
	}

	TArray<FString> BonesListArray;

	if (!(TFXBoneDataJson)->TryGetStringArrayField("bonesList", BonesListArray))
	{
		return nullptr;
	}

	int32 NumStrandsInBoneFile;

	if (!(TFXBoneDataJson)->TryGetNumberField("numGuideStrands", NumStrandsInBoneFile))
	{
		return nullptr;
	}
	OutNumStrandsInfile = NumStrandsInBoneFile;

	EObjectFlags BoneFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
	UTressFXBoneSkinningAsset* BoneAsset = NewObject<UTressFXBoneSkinningAsset>(InOuter, *AssetName, BoneFlags);

	BoneAsset->AssetType = FTressFXBoneSkinngAssetType::TFXBone_Json;

	for (int32 i = 0; i < BonesListArray.Num(); i++)
	{
		BoneAsset->JsonVersionImportData.BoneNames.Add(*BonesListArray[i]);
	}

	BoneAsset->JsonVersionImportData.NumBones = BoneAsset->JsonVersionImportData.BoneNames.Num();

	for (const TSharedPtr<FJsonValue> SkinDataVal : *SkinDataArray)
	{
		const TSharedPtr<FJsonObject> SkinDataObject = SkinDataVal->AsObject();

		FString BoneName;
		if (!SkinDataObject->TryGetStringField("boneName", BoneName))
		{
			return nullptr;
		}

		double Weight = -1;

		if (!SkinDataObject->TryGetNumberField("weight", Weight))
		{
			return nullptr;
		}
		FTressFXBoneSkinningJSONImportData BSData;
		BSData.BoneName = *BoneName;
		BSData.Weight = (float)Weight;
		BoneAsset->JsonVersionImportData.JsonSkinningData.Add(BSData);
	}
	return BoneAsset;
}

UTressFXBoneSkinningAsset* ParseTFXBoneJson(FString JsonString, UObject* InOuter, FString AssetName, int32& OutNumStrandsInfile)
{

	TSharedPtr<FJsonObject> TFXBoneDataJson;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, TFXBoneDataJson))
	{
		return ParseTFXBoneJson(TFXBoneDataJson, InOuter, AssetName, OutNumStrandsInfile);
	}
	return nullptr;
}

//////////////////////////////////////////////
//UTressFXFactory - tfx
/////////////////////////////////////////////

UTressFXFactory::UTressFXFactory(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bCreateNew = false;
	SupportedClass = UTressFXAsset::StaticClass();
	bEditorImport = true;
	Formats.Add(TEXT("tfx;Tress FX asset"));
}

bool UTressFXFactory::ConfigureProperties()
{
	TSharedRef<STressFXImportWindow> ImportWindow = SNew(STressFXImportWindow);
	if (ImportWindow->ShowModal() != EAppReturnType::Cancel)
	{
		this->ImportConfig = ImportWindow->GetImportOptions();
		return true;
	}

	return false;
}


bool UTressFXFactory::CanCreateNew() const
{
	return false;
}

bool UTressFXFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}

UObject* UTressFXFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{

	Flags |= RF_Transactional;

	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	FString CurrentSourcePath;
	FString FilenameNoExtension;
	FString UnusedExtension;
	FPaths::Split(UFactory::GetCurrentFilename(), CurrentSourcePath, FilenameNoExtension, UnusedExtension);

	const FString LongPackagePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName());

	const FString NameForErrors(InName.ToString());


	TArray<uint8> FileContent;

	if (!FFileHelper::LoadFileToArray(FileContent, *UFactory::GetCurrentFilename()))
	{
		return nullptr;
	}

	//Process TFX file header

	FBufferReader Reader(FileContent.GetData(), FileContent.Num(), false);
	FTressFXTFXFileHeader Header;
	FMemory::Memzero(&Header, sizeof(FTressFXTFXFileHeader));
	Reader.Serialize(&Header, sizeof(FTressFXTFXFileHeader));

	if (Header.version < AMD_TRESSFX_VERSION_MAJOR)
	{
		return nullptr;
	}

	uint32 NumStrandsInFile = Header.numHairStrands;

	UTressFXAsset* Result = nullptr;

	FString NewName = FString::Printf(TEXT("%s_TFX"), *InName.ToString());
	Result = NewObject<UTressFXAsset>(InParent, *NewName, Flags);


	// We make the number of strands be multiple of TRESSFX_SIM_THREAD_GROUP_SIZE. 
	if (NumStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE == 0)
	{
		Result->ImportData->NumGuideStrands = NumStrandsInFile;
	}
	else
	{
		Result->ImportData->NumGuideStrands = (NumStrandsInFile - NumStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE) + TRESSFX_SIM_THREAD_GROUP_SIZE;
	}	

	Result->ImportData->NumVerticesPerStrand = Header.numVerticesPerStrand;

	Result->ImportData->NumFollowStrandsPerGuide = Result->NumFollowStrandsPerGuide;
	// Until we call GenerateFollowHairs, the number of total strands is equal to the number of guide strands. 
	Result->ImportData->NumTotalStrands = Result->ImportData->NumGuideStrands; 
	Result->ImportData->NumGuideVertices = Result->ImportData->NumGuideStrands * Result->ImportData->NumVerticesPerStrand;
	// Again, the total number of vertices is equal to the number of guide vertices here. 
	int32 NumTotalVertices = Result->ImportData->NumGuideVertices; 

	Result->ImportData->ImportedPositions.AddZeroed(NumTotalVertices);

	Reader.Seek(Header.offsetVertexPosition);

	Reader.Serialize(Result->ImportData->ImportedPositions.GetData(), NumStrandsInFile * Result->ImportData->NumVerticesPerStrand * sizeof(FVector4));

	Result->ImportData->ImportRotation = ImportConfig.PickedImportRotation;
	Result->ImportData->ImportTranslation = ImportConfig.PickedImportTranslation;
	Result->ImportData->ImportScale = ImportConfig.PickedScale;

	int32 numStrandsToMakeUp = Result->ImportData->NumGuideStrands - NumStrandsInFile;

	for (int32 i = 0; i < numStrandsToMakeUp; ++i)
	{
		for (int32 j = 0; j < Result->ImportData->NumVerticesPerStrand; ++j)
		{
			int32 indexLastVertex = (NumStrandsInFile - 1) * Result->ImportData->NumVerticesPerStrand + j;
			int32 indexVertex = (NumStrandsInFile + i) * Result->ImportData->NumVerticesPerStrand + j;
			Result->ImportData->ImportedPositions[indexVertex] = Result->ImportData->ImportedPositions[indexLastVertex];
		}
	}

	Result->ImportData->StrandUV.AddZeroed(Result->ImportData->NumTotalStrands);
	Reader.Seek(Header.offsetStrandUV);
	Reader.Serialize(Result->ImportData->StrandUV.GetData(), NumStrandsInFile * sizeof(FVector2D));

	int32 indexLastStrand = (NumStrandsInFile - 1);

	for (int32 i = 0; i < numStrandsToMakeUp; ++i)
	{
		int32 indexStrand = (NumStrandsInFile + i);
		Result->ImportData->StrandUV[indexStrand] = Result->ImportData->StrandUV[indexLastStrand];
	}

	// make copy of the strandUVS for using when regenerating follow hairs.
	// This is the only time that StrandUV_Original should ever be edited.
	Result->ImportData->StrandUV_Original = Result->ImportData->StrandUV;
	Result->ImportData->ProcessAsset(Result->TipSeparationFactor, Result->MaxRadiusAroundGuideHair, true);

	return Result;
}


//////////////////////////////////////////////
//UTressFXJSONFactory - tfxjson
/////////////////////////////////////////////

UTressFXJSONFactory::UTressFXJSONFactory(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bCreateNew = false;
	SupportedClass = UTressFXAsset::StaticClass();
	bEditorImport = true;
	bText = true;
	Formats.Add(TEXT("tfxjson;Tress FX Combined asset"));
}

bool UTressFXJSONFactory::ConfigureProperties()
{
	TSharedRef<STressFXImportWindow> ImportWindow = SNew(STressFXImportWindow);
	if (ImportWindow->ShowModal() != EAppReturnType::Cancel)
	{
		this->ImportConfig = ImportWindow->GetImportOptions();
		return true;
	}

	return false;
}


bool UTressFXJSONFactory::CanCreateNew() const
{
	return false;
}

bool UTressFXJSONFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}

//Json version which will also have tfxbone data inside it
UObject* UTressFXJSONFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	Flags |= RF_Transactional;
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);
	const int32 BufferSize = BufferEnd - Buffer;

	const FString FileContent(BufferEnd - Buffer, Buffer);

	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	FString CurrentSourcePath;
	FString FilenameNoExtension;
	FString UnusedExtension;
	FPaths::Split(UFactory::GetCurrentFilename(), CurrentSourcePath, FilenameNoExtension, UnusedExtension);


	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContent);
	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{

		//retrieve all the json fields
		const TArray<TSharedPtr<FJsonValue>>* JsonPositions;
		if (!JsonObject->TryGetArrayField("positions", JsonPositions))
		{
			return nullptr;
		}

		const TArray<TSharedPtr<FJsonValue>>* JsonUVs;
		if (!JsonObject->TryGetArrayField("uvs", JsonUVs))
		{
			return nullptr;
		}

		int32 NumStrandsInFile;
		if (!JsonObject->TryGetNumberField("numHairStrands", NumStrandsInFile))
		{
			return nullptr;
		}

		int32 NumVertsPerStrand;
		if (!JsonObject->TryGetNumberField("numVerticesPerStrand", NumVertsPerStrand))
		{
			return nullptr;
		}

		if (NumStrandsInFile != JsonPositions->Num() || NumStrandsInFile != JsonUVs->Num())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject>* TFXBoneDataJson;
		const bool bHasBoneData = JsonObject->TryGetObjectField("tfxBoneData", TFXBoneDataJson);

		UTressFXAsset* Result = nullptr;
		FString NewName = FString::Printf(TEXT("%s_TFX"), *InName.ToString());
		Result = NewObject<UTressFXAsset>(InParent, *NewName, Flags);
		Result->ImportData->ImportRotation = ImportConfig.PickedImportRotation;
		Result->ImportData->ImportTranslation = ImportConfig.PickedImportTranslation;
		Result->ImportData->ImportScale = ImportConfig.PickedScale;

		// We make the number of strands be multiple of TRESSFX_SIM_THREAD_GROUP_SIZE, if its not already. 
		if (NumStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE == 0)
		{
			Result->ImportData->NumGuideStrands = NumStrandsInFile;
		}
		else
		{
			Result->ImportData->NumGuideStrands = (NumStrandsInFile - NumStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE) + TRESSFX_SIM_THREAD_GROUP_SIZE;
		}

		Result->ImportData->NumVerticesPerStrand = NumVertsPerStrand;
		Result->ImportData->NumFollowStrandsPerGuide = Result->NumFollowStrandsPerGuide;
		// Until we call GenerateFollowHairs, the number of total strands is equal to the number of guide strands. 
		Result->ImportData->NumTotalStrands = Result->ImportData->NumGuideStrands;
		Result->ImportData->NumGuideVertices = Result->ImportData->NumGuideStrands * Result->ImportData->NumVerticesPerStrand;
		// Again, the total number of vertices is equal to the number of guide vertices here. 
		int32 NumTotalVertices = Result->ImportData->NumGuideVertices;

		//Positions
		Result->ImportData->ImportedPositions.AddZeroed(NumTotalVertices);
		for (int32 GuideStrandIndex = 0; GuideStrandIndex < JsonPositions->Num(); GuideStrandIndex++)
		{

			TArray<TSharedPtr<FJsonValue>> VertArray = (*JsonPositions)[GuideStrandIndex]->AsArray();
			if (VertArray.Num() != NumVertsPerStrand)
			{
				return nullptr;
			}

			for(int32 VertIndex = 0; VertIndex < VertArray.Num(); VertIndex++)
			{
				const TSharedPtr<FJsonObject>* VertObj;
				if(!VertArray[VertIndex]->TryGetObject(VertObj))
				{
					return nullptr;
				}
				double x = -1;
				double y = -1;
				double z = -1;
				double w =- 1;
				if(
					!VertObj->Get()->TryGetNumberField("x",x) ||
					!VertObj->Get()->TryGetNumberField("y", y) ||
					!VertObj->Get()->TryGetNumberField("z", z) ||
					!VertObj->Get()->TryGetNumberField("w", w)
				)
				{
					return nullptr;
				}

				FVector4 Pos;
				Pos.X = (float)x;
				Pos.Y = (float)y;
				Pos.Z = (float)z;
				Pos.W = (float)w;
				Result->ImportData->ImportedPositions[(GuideStrandIndex * NumVertsPerStrand) + VertIndex] = Pos;
			}
		}
		//UVs
		Result->ImportData->StrandUV.AddZeroed(Result->ImportData->NumTotalStrands);

		for (int32 UVIndex = 0; UVIndex < JsonUVs->Num(); UVIndex++)
		{
			const TSharedPtr<FJsonObject>* UVObj;
			if (!((*JsonUVs)[UVIndex]->TryGetObject(UVObj)))
			{
				return nullptr;
			}

			double x = -1;
			double y = -1;

			if (
				!UVObj->Get()->TryGetNumberField("x", x) ||
				!UVObj->Get()->TryGetNumberField("y", y)
			)
			{
				return nullptr;
			}

			Result->ImportData->StrandUV[UVIndex] = FVector2D((float)x, (float)y);
		}

		///////////////////////////////////////////////
		//tfxbonedata
		///////////////////////////////////////////////

		UTressFXBoneSkinningAsset* BoneAsset = nullptr;
		if(bHasBoneData)
		{
			if (!TFXBoneDataJson)
			{
				return nullptr;
			}
			//create TFXBone asset and package objects
			FString BoneAssetName = FString::Printf(TEXT("%s_BoneMap"), *InName.ToString());
			FString PackagePath = FPaths::GetPath(InParent->GetOutermost()->GetPathName()) + "/";
			FString PackageNameStr;
			FString SaveName;
			AssetToolsModule.Get().CreateUniqueAssetName(PackagePath, BoneAssetName, PackageNameStr /*out*/, SaveName /*out*/);
			EObjectFlags BoneFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
			UPackage *Package = CreatePackage(nullptr, *PackageNameStr);
			int32 NumStrandsInBoneFile;

			BoneAsset = ParseTFXBoneJson(*TFXBoneDataJson, Package, BoneAssetName, NumStrandsInBoneFile);

			if (NumStrandsInBoneFile != NumStrandsInFile)
			{
				return nullptr;
			}			
		}

		int32 NumStrandsToMakeUp = Result->ImportData->NumGuideStrands - NumStrandsInFile;

		// evenly distribute the strands we need to make up, instead of just using the last one like the sample does.
		int32 DistributionInterval = FMath::FloorToInt( (NumStrandsInFile / NumStrandsToMakeUp) );

		for (int32 MakeUpStrandIndex = 0; MakeUpStrandIndex < NumStrandsToMakeUp; ++MakeUpStrandIndex)
		{
			int32 SourceStrandIndex = MakeUpStrandIndex * DistributionInterval;

			int32 TargetStrandUV = (NumStrandsInFile + MakeUpStrandIndex);
			Result->ImportData->StrandUV[TargetStrandUV] = Result->ImportData->StrandUV[SourceStrandIndex];

			for (int32 j = 0; j < Result->ImportData->NumVerticesPerStrand; ++j)
			{
				int32 SourceVertex = (SourceStrandIndex * Result->ImportData->NumVerticesPerStrand) + j;
				int32 TargetVertex = ((NumStrandsInFile + MakeUpStrandIndex) * Result->ImportData->NumVerticesPerStrand) + j;
				Result->ImportData->ImportedPositions[TargetVertex] = Result->ImportData->ImportedPositions[SourceVertex];
			}

			if (bHasBoneData && BoneAsset)
			{
				//each strand needs TRESSFX_MAX_INFLUENTIAL_BONE_COUNT weight datas, even if they are empty
				for (int32 BoneInfluenceIndex = 0; BoneInfluenceIndex < TRESSFX_MAX_INFLUENTIAL_BONE_COUNT; BoneInfluenceIndex++)
				{
					int32 BoneDataIndex = (SourceStrandIndex * TRESSFX_MAX_INFLUENTIAL_BONE_COUNT) + BoneInfluenceIndex;
					FTressFXBoneSkinningJSONImportData Duplicated = BoneAsset->JsonVersionImportData.JsonSkinningData[BoneDataIndex];
					BoneAsset->JsonVersionImportData.JsonSkinningData.Add(Duplicated);
				}
			}
		}

		// make copy of the strandUVS for using when regenerating follow hairs.
		// This is the ONLY time that StrandUV_Original should ever be edited.
		Result->ImportData->StrandUV_Original = Result->ImportData->StrandUV;
		Result->ImportData->ProcessAsset(Result->TipSeparationFactor, Result->MaxRadiusAroundGuideHair, true);

		if (bHasBoneData && BoneAsset)
		{
			FAssetRegistryModule::AssetCreated(BoneAsset);
			BoneAsset->MarkPackageDirty();
		}

		return Result;
	}
	else
	{
		return nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////////
//Bone skinning assets .tfxbone
//////////////////////////////////////////////////////////////////////////////

UTressFXBoneSkinningFactory::UTressFXBoneSkinningFactory(const class FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	bCreateNew = false;
	SupportedClass = UTressFXBoneSkinningAsset::StaticClass();

	bEditorImport = true;
	bText = false;
	Formats.Add(TEXT("tfxbone;Tress FX bone skinning asset"));
}


bool UTressFXBoneSkinningFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}

bool UTressFXBoneSkinningFactory::CanCreateNew() const
{
	return false;
}


void UTressFXBoneSkinningFactory::PostInitProperties()
{
	Super::PostInitProperties();
}

UObject * UTressFXBoneSkinningFactory::FactoryCreateBinary(UClass * InClass, UObject * InParent, FName InName, EObjectFlags Flags, UObject * Context, const TCHAR * Type, const uint8 *& Buffer, const uint8 * BufferEnd, FFeedbackContext * Warn)
{
	Flags |= RF_Transactional;

	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	bool bLoadedSuccessfully = true;

	FString CurrentSourcePath;
	FString FilenameNoExtension;
	FString UnusedExtension;
	FPaths::Split(UFactory::GetCurrentFilename(), CurrentSourcePath, FilenameNoExtension, UnusedExtension);

	const FString LongPackagePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName());

	const FString NameForErrors(InName.ToString());

	TArray<uint8> FileContent;

	if (!FFileHelper::LoadFileToArray(FileContent, *UFactory::GetCurrentFilename()))
	{
		return nullptr;
	}

	FBufferReader Reader(FileContent.GetData(), FileContent.Num(), false);
	
	UTressFXBoneSkinningAsset* Result = nullptr;
	FString NewName = FString::Printf(TEXT("%s_BoneMap"), *InName.ToString());

	Result = NewObject<UTressFXBoneSkinningAsset>(InParent, *NewName, Flags);

	if (!Result)
	{
		return nullptr;
	}
	Result->RawData = FileContent;
	Result->AssetType = FTressFXBoneSkinngAssetType::TFXBone_Binary;
	return Result;

}

///////////////////////////////////////////////////////////////////////////////
//Bone skinning assets .tfxbonejson
//////////////////////////////////////////////////////////////////////////////

UTressFXBoneSkinningJSONFactory::UTressFXBoneSkinningJSONFactory(const class FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	bCreateNew = false;
	SupportedClass = UTressFXBoneSkinningAsset::StaticClass();

	bEditorImport = true;
	bText = true;
	Formats.Add(TEXT("tfxbonejson;Tress FX bone skinning asset"));
}


bool UTressFXBoneSkinningJSONFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}

bool UTressFXBoneSkinningJSONFactory::CanCreateNew() const
{
	return false;
}


void UTressFXBoneSkinningJSONFactory::PostInitProperties()
{
	Super::PostInitProperties();
}

UObject* UTressFXBoneSkinningJSONFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{

	Flags |= RF_Transactional;
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);
	const int32 BufferSize = BufferEnd - Buffer;
	const FString FileContent(BufferEnd - Buffer, Buffer);

	int32 NumStrandsInFile;

	FString NewName = FString::Printf(TEXT("%s_BoneMap"), *InName.ToString());
	UTressFXBoneSkinningAsset* ResultAsset = ParseTFXBoneJson(FileContent,InParent, NewName, NumStrandsInFile);

	if (ResultAsset)
	{

		int32 NumGuideStrands;
		if (NumStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE == 0)
		{
			NumGuideStrands = NumStrandsInFile;
		}
		else
		{
			NumGuideStrands = (NumStrandsInFile - NumStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE) + TRESSFX_SIM_THREAD_GROUP_SIZE;
		}
		//logic here needs to match the tfx asset logic
		int32 NumStrandsToMakeup = NumGuideStrands - NumStrandsInFile;

		int32 indexLastStrand = (ResultAsset->JsonVersionImportData.JsonSkinningData.Num() - (TRESSFX_MAX_INFLUENTIAL_BONE_COUNT));
		for (int32 i = 0; i < NumStrandsToMakeup; ++i)
		{
			//each strand needs 4 weight datas, even if they are empty
			for (int32 j = 0; j < TRESSFX_MAX_INFLUENTIAL_BONE_COUNT; j++)
			{
				FTressFXBoneSkinningJSONImportData Duplicated = ResultAsset->JsonVersionImportData.JsonSkinningData[indexLastStrand + j];
				ResultAsset->JsonVersionImportData.JsonSkinningData.Add(Duplicated);
			}
		}

		return ResultAsset;
	}
	else
	{
		return nullptr;
	}
}
