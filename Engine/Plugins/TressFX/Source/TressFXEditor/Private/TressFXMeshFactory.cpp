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

#include "Factories/TressFXMeshFactory.h"
#include "Engine/TressFXMesh.h"
#include "TressFX/TressFXTypes.h"
#include "Engine/SkeletalMesh.h"
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"

#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "Editor.h"


#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "TressFXMeshFactory"


UTressFXMeshFactory::UTressFXMeshFactory(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bCreateNew = false;
	//bEditAfterNew = true;
	SupportedClass = UTressFXMesh::StaticClass();

	bEditorImport = true;
	bText = true;

	Formats.Add(TEXT("tfxmesh;Tress FX mesh asset"));
}

bool UTressFXMeshFactory::CanCreateNew() const
{
	return false;
}

bool UTressFXMeshFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}

UObject* UTressFXMeshFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{

	Flags |= RF_Transactional;

	const bool bCullEmptys = true;

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);

	FString NewName = FString::Printf(TEXT("%s_TFXMesh"), *InName.ToString());
	const int32 BufferSize = BufferEnd - Buffer;

	const FString FileContent(BufferEnd - Buffer, Buffer);

	TArray<FString> Lines;

	FileContent.ParseIntoArrayLines(Lines, bCullEmptys);

	TArray<FString> BoneNames;
	int32 NumOfBones = 0;

	TArray<FString> Tokens;

	uint32 NumOfVerts = 0;

	TArray<FTressFXBoneSkinningData> TressFXBoneSkinningData;
	TArray<FVector> TempVerts;
	TArray<FVector> TempNormals;

	int32 NumTriangles = 0;
	TArray<int32> TempIndices;

	UTressFXMesh* Result = nullptr;

	for (auto Itr = Lines.CreateConstIterator(); Itr; ++Itr)
	{
		FString Line = *Itr;

		if (Line.Len() == 0)
		{
			continue;
		}

		if (Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		Tokens.Empty();
		const TCHAR* Delims[] = { TEXT(" ") };
		Line.ParseIntoArray(Tokens, Delims, 1, bCullEmptys);

		int32 NumFound = Tokens.Num();

		FString Token;

		if (NumFound > 0)
		{
			Token = Tokens[0];
		}
		else
		{
			Token = Line;
		}

		if (Token.Find(TEXT("numOfBones")) != INDEX_NONE)
		{
			NumOfBones = FCString::Atoi(*Tokens[1]);
			int32 CountBone = 0;

			while (true)
			{
				Itr++;
				Line = *Itr;

				if (Line.Len() == 0)
				{
					continue;
				}

				if (Line.StartsWith(TEXT("#")))
				{
					continue;
				}

				Tokens.Empty();
				Line.ParseIntoArray(Tokens, Delims, 1, bCullEmptys);
				NumFound = Tokens.Num();
				BoneNames.Add(Tokens[1]);
				CountBone++;

				if (CountBone == NumOfBones)
				{
					break;
				}
			}
		}


		if (Token.Find(TEXT("numOfVertices")) != INDEX_NONE)
		{
			NumOfVerts = (uint32)(FCString::Atoi(*Tokens[1]));
			TressFXBoneSkinningData.AddDefaulted(NumOfVerts);
			TempVerts.AddZeroed(NumOfVerts);
			TempNormals.AddZeroed(NumOfVerts);

			int32 Index = 0;

			while (true)
			{

				Itr++;
				Line = *Itr;

				if (Line.Len() == 0)
				{
					continue;
				}

				if (Line.StartsWith(TEXT("#")))
				{
					continue;
				}

				Tokens.Empty();
				Line.ParseIntoArray(Tokens, Delims, 1, bCullEmptys);
				if (Tokens.Num() != 15)
				{
					return nullptr;
				}

				int32 VertexIndex = FCString::Atoi(*Tokens[0]);
				if (VertexIndex != Index)
				{
					return nullptr;
				}

				FVector &Pos = TempVerts[Index];
				Pos.X = FCString::Atof(*Tokens[1]);
				Pos.Y = FCString::Atof(*Tokens[2]);
				Pos.Z = FCString::Atof(*Tokens[3]);

				FVector &Normal = TempNormals[Index];
				Normal.X = FCString::Atof(*Tokens[4]);
				Normal.Y = FCString::Atof(*Tokens[5]);
				Normal.Z = FCString::Atof(*Tokens[6]);

				FTressFXBoneSkinningData SkinData;

				int32 BoneIndex = FCString::Atoi(*Tokens[7]);
				SkinData.BoneIndex[0] = (float)TargetSkeleton->RefSkeleton.FindBoneIndex(*BoneNames[BoneIndex]);

				BoneIndex = FCString::Atoi(*Tokens[8]);
				SkinData.BoneIndex[1] = (float)TargetSkeleton->RefSkeleton.FindBoneIndex(*BoneNames[BoneIndex]);

				BoneIndex = FCString::Atoi(*Tokens[9]);
				SkinData.BoneIndex[2] = (float)TargetSkeleton->RefSkeleton.FindBoneIndex(*BoneNames[BoneIndex]);

				BoneIndex = FCString::Atoi(*Tokens[10]);
				SkinData.BoneIndex[3] = (float)TargetSkeleton->RefSkeleton.FindBoneIndex(*BoneNames[BoneIndex]);

				SkinData.Weight[0] = FCString::Atof(*Tokens[11]);
				SkinData.Weight[1] = FCString::Atof(*Tokens[12]);
				SkinData.Weight[2] = FCString::Atof(*Tokens[13]);
				SkinData.Weight[3] = FCString::Atof(*Tokens[14]);

				TressFXBoneSkinningData[Index] = SkinData;

				Index++;

				if (Index == NumOfVerts)
				{
					break;
				}
			}

		}
		else if (Token.Find(TEXT("numOfTriangles")) != INDEX_NONE)
		{
			NumTriangles = FCString::Atoi(*Tokens[1]);
			int NumIndices = NumTriangles * 3;
			TempIndices.AddZeroed(NumIndices);
			int Index = 0;

			while (true)
			{
				Itr++;
				Line = *Itr;

				if (Line.Len() == 0)
				{
					continue;
				}

				if (Line.StartsWith(TEXT("#")))
				{
					continue;
				}

				Tokens.Empty();
				Line.ParseIntoArray(Tokens, Delims, 1, bCullEmptys);
				if (Tokens.Num() != 4)
				{
					return nullptr;
				}

				int32 TriangleIndex = FCString::Atoi(*Tokens[0]);
				if (TriangleIndex != Index)
				{
					return nullptr;
				}

				TempIndices[Index * 3 + 0] = FCString::Atoi(*Tokens[1]);
				TempIndices[Index * 3 + 1] = FCString::Atoi(*Tokens[2]);
				TempIndices[Index * 3 + 2] = FCString::Atoi(*Tokens[3]);

				Index++;

				if (Index == NumTriangles)
				{
					break;
				}
			}
		}
	}

	// Create the new tile map asset and import basic/global data
	Result = NewObject<UTressFXMesh>(InParent, *NewName, Flags);

	FRotator ImportRotation = ImportConfig.PickedImportRotation;
	FVector ImportTranslation = ImportConfig.PickedImportTranslation;

	Result->LoadData(TempVerts, TempNormals, TempIndices, TressFXBoneSkinningData, NumTriangles, ImportRotation, ImportTranslation, true);

	return Result;
}

bool UTressFXMeshFactory::ConfigureProperties()
{
	// Null the skeleton so we can check for selection later
	TargetSkeleton = NULL;

	// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig AssetPickerConfig;

	/** The asset picker will only show skeletons */
	AssetPickerConfig.Filter.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UTressFXMeshFactory::OnTargetSkeletonSelected);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("CreateTressFXMeshOptions", "Pick Skeleton"))
		.ClientSize(FVector2D(500, 600))
		.SupportsMinimize(false).SupportsMaximize(false)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
		];

	GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
	PickerWindow.Reset();

	if (TargetSkeleton == NULL)
	{
		return false;
	}
	

	TSharedRef<STressFXImportWindow> ImportWindow = SNew(STressFXImportWindow);
	if (ImportWindow->ShowModal() != EAppReturnType::Cancel)
	{
		this->ImportConfig = ImportWindow->GetImportOptions();
		return TargetSkeleton != NULL;
	}

	return false;
}


void UTressFXMeshFactory::OnTargetSkeletonSelected(const FAssetData& SelectedAsset)
{
	TargetSkeleton = Cast<USkeletalMesh>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}


#undef LOCTEXT_NAMESPACE