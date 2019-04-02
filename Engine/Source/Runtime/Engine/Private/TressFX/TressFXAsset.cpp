
#include "Engine/TressFXAsset.h"
#include "Animation/AnimInstance.h"
#include "Components/TressFXComponent.h"
#include "Animation/Skeleton.h"
#include "Engine/TressFXBoneSkinningAsset.h"
#include "Uobject/UObjectIterator.h"

//#pragma optimize("", off)

DEFINE_LOG_CATEGORY(TressFXAssetLog);

UTressFXAsset::UTressFXAsset(const class FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	NumFollowStrandsPerGuide = 2.f;
	TipSeparationFactor = 0.4;
	MaxRadiusAroundGuideHair = 5.f;
	ImportData = MakeShareable(new FTressFXRuntimeData());
	bIsValid = true; // assume valid at start
}

static FVector ToFVector(const FVector4& InVec)
{
	return FVector(InVec.X, InVec.Y, InVec.Z);
}

static FVector4 ToFVector4(const FVector& InVec)
{
	return FVector4(InVec.X, InVec.Y, InVec.Z, 1);
}

static void GetTangentVectors(const FVector& n, FVector& t0, FVector& t1)
{
	if (FMath::Abs(n.Z) > 0.707f)
	{
		float a = n.Y * n.Y + n.Z * n.Z;
		float k = 1.0f / FMath::Sqrt(a);
		t0.X = 0;
		t0.Y = -n.Z * k;
		t0.Z = n.Y * k;

		t1.X = a * k;
		t1.Y = -n.X * t0.Z;
		t1.Z = n.X * t0.Y;
	}
	else
	{
		float a = n.X * n.X + n.Y* n.Y;
		float k = 1.0f / FMath::Sqrt(a);
		t0.X = -n.Y * k;
		t0.Y = n.X * k;
		t0.Z = 0;

		t1.X = -n.Z * t0.Y;
		t1.Y = n.Z* t0.X;
		t1.Z = a * k;
	}
}

void UTressFXAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	ImportData->Serialize(Ar);

	Ar << SkinningData;

}


#if WITH_EDITORONLY_DATA

void UTressFXAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;


	bool bSomethingChanged = false;

	if (Name == FName(TEXT("NumFollowStrandsPerGuide")) || Name == FName(TEXT("TipSeparationFactor")) || Name == FName(TEXT("MaxRadiusAroundGuideHair")))
	{
		ImportData->NumFollowStrandsPerGuide = this->NumFollowStrandsPerGuide;
		ImportData->ProcessAsset(TipSeparationFactor, MaxRadiusAroundGuideHair);
		bSomethingChanged = true;

	}
	else if (Name == FName(TEXT("BaseSkeleton")) || Name == FName(TEXT("TressFXBoneSkinningAsset")))
	{
		bSomethingChanged = true;
	}



	if (bSomethingChanged)
	{
		if (TressFXBoneSkinningAsset && BaseSkeleton)
		{
			bIsValid = ImportData->LoadBoneData(BaseSkeleton, TressFXBoneSkinningAsset);
		}
		else
		{
			SkinningData.Empty();
			bIsValid = false;
			return;
		}



		for (TObjectIterator<UTressFXComponent> It; It; ++It)
		{
			if (It->Asset == this)
			{
				It->MarkRenderStateDirty();
			}
		}
	}
}
#endif

static float GetRandom(float Min, float Max)
{
	return ((float(FMath::Rand()) / float(0x7fff)) * (Max - Min)) + Min;
}

#if WITH_EDITORONLY_DATA

bool FTressFXRuntimeData::GenerateFollowHairs(int32 numFollowHairsPerGuideHair /*= 2*/, float tipSeparationFactor /*= 2*/, float maxRadiusAroundGuideHair /*= 1.2*/)
{
	check(numFollowHairsPerGuideHair >= 0);

	NumFollowStrandsPerGuide = numFollowHairsPerGuideHair;

	// Recompute total number of hair strands and vertices with considering number of follow hairs per a guide hair. 
	NumTotalStrands = NumGuideStrands * (NumFollowStrandsPerGuide + 1);
	NumTotalVertices = NumTotalStrands * NumVerticesPerStrand;

	//this needs to happen even if numfollowers is 0
	Positions = ImportedPositions;

	// Nothing to do, just exit. 
	if (numFollowHairsPerGuideHair == 0)
	{
		return false;
	}

	

	// keep the old buffers until the end of this function.
	TArray<FVector4> positionsGuide = Positions;

	// We need to use the ORIGINAL ORIGINAL UVs  because
	// otherwise if this function gets called enough times, the strand UVs will eventually all end up the exact same number!
	TArray<FVector2D> strandUVGuide = StrandUV_Original;

	// re-allocate all buffers
	Positions.Empty();
	StrandUV.Empty();

	Positions.AddZeroed(NumTotalVertices);
	StrandUV.AddZeroed(NumTotalStrands);

	FollowRootOffsets.Empty();
	FollowRootOffsets.AddZeroed(NumTotalStrands);

	FVector4* pos = Positions.GetData();
	FVector4* followOffset = FollowRootOffsets.GetData();

	// Generate follow hairs
	for (int32 i = 0; i < NumGuideStrands; i++)
	{
		int32 indexGuideStrand = i * (NumFollowStrandsPerGuide + 1);
		int32 indexRootVertMaster = indexGuideStrand * NumVerticesPerStrand;

		FMemory::Memcpy(&pos[indexRootVertMaster], &positionsGuide[i*NumVerticesPerStrand], sizeof(FVector4)*NumVerticesPerStrand);
		StrandUV[indexGuideStrand] = strandUVGuide[i];

		followOffset[indexGuideStrand] = FVector4(ForceInitToZero);
		followOffset[indexGuideStrand].W = (float)indexGuideStrand;
		FVector v01 = (ToFVector(pos[indexRootVertMaster + 1]) - ToFVector(pos[indexRootVertMaster])).GetSafeNormal();
		//v01.Normalize();

		// Find two orthogonal unit tangent vectors to v01
		FVector t0, t1;
		GetTangentVectors(v01, t0, t1);

		for (int32 j = 0; j < NumFollowStrandsPerGuide; j++)
		{
			int32 indexStrandFollow = indexGuideStrand + j + 1;
			int32 indexRootVertFollow = indexStrandFollow * NumVerticesPerStrand;

			StrandUV[indexStrandFollow] = StrandUV[indexGuideStrand];

			// offset vector from the guide strand's root vertex position
			FVector offset = GetRandom(-maxRadiusAroundGuideHair, maxRadiusAroundGuideHair) * t0 + GetRandom(-maxRadiusAroundGuideHair, maxRadiusAroundGuideHair) * t1;
			followOffset[indexStrandFollow] = FVector4(offset, (float)indexGuideStrand);

			for (int32 k = 0; k < NumVerticesPerStrand; k++)
			{
				const FVector4* guideVert = &pos[indexRootVertMaster + k];
				//FVector4* followVert = &pos[indexRootVertFollow + 1];
				// not sure why it was hardcoded to 1... thanks guy
				// https://github.com/GPUOpen-Effects/TressFX/pull/39/commits/ea9086cf7368ef4027b8dc5b44e1d5fb2ca160cf
				FVector4* followVert = &pos[indexRootVertFollow + k];

				float factor = tipSeparationFactor * ((float)k / ((float)NumVerticesPerStrand)) + 1.0f;
				*followVert = FVector4(ToFVector(*guideVert) + offset * factor, guideVert->W);
			}
		}
	}
	return true;
}
#endif

void FTressFXRuntimeData::ProcessImportTransforms()
{
	if (!ImportRotation.IsNearlyZero())
	{
		for (int32 PosIndex = 0; PosIndex < ImportedPositions.Num(); PosIndex++)
		{
			float W = ImportedPositions[PosIndex].W;
			FVector4 Rotated = ImportRotation.RotateVector(ImportedPositions[PosIndex]);
			Rotated.W = W;
			ImportedPositions[PosIndex] = Rotated;
		}
	}

	if (!ImportTranslation.IsNearlyZero())
	{
		for (int32 PosIndex = 0; PosIndex < ImportedPositions.Num(); PosIndex++)
		{
			ImportedPositions[PosIndex].X += ImportTranslation.X;
			ImportedPositions[PosIndex].Y += ImportTranslation.Y;
			ImportedPositions[PosIndex].Z += ImportTranslation.Z;
		}
	}

	if (!FMath::IsNearlyEqual(1.0f, ImportScale.X) || !FMath::IsNearlyEqual(1.0f, ImportScale.Y) || !FMath::IsNearlyEqual(1.0f, ImportScale.Z))
	{
		for (int32 PosIndex = 0; PosIndex < ImportedPositions.Num(); PosIndex++)
		{
			ImportedPositions[PosIndex].X *= ImportScale.X;
			ImportedPositions[PosIndex].Y *= ImportScale.Y;
			ImportedPositions[PosIndex].Z *= ImportScale.Z;
		}		
	}
}
#if WITH_EDITORONLY_DATA
bool FTressFXRuntimeData::ProcessAsset(float TipSeparationFactor, float MaxRadiusAroundGuideHair, bool bProcessImportTransforms /*= false*/)
{
	if(bProcessImportTransforms)
	{
		ProcessImportTransforms();
	}

	FollowRootOffsets.AddZeroed(NumTotalStrands);
	GenerateFollowHairs(NumFollowStrandsPerGuide, TipSeparationFactor, MaxRadiusAroundGuideHair);

	StrandTypes.Empty();
	StrandTypes.AddZeroed(NumTotalStrands);

	Tangents.Empty();
	Tangents.AddZeroed(NumTotalVertices);

	RestLengths.Empty();
	RestLengths.AddZeroed(NumTotalVertices);

	RefVectors.Empty();
	RefVectors.AddZeroed(NumTotalVertices);

	GlobalRotations.Empty();
	GlobalRotations.AddZeroed(NumTotalVertices);

	LocalRotations.Empty();;
	LocalRotations.AddZeroed(NumTotalVertices);

	ThicknessCoeffs.Empty();
	ThicknessCoeffs.AddZeroed(NumTotalVertices);

	TriangleIndices.Empty();
	TriangleIndices.AddZeroed(GetNumHairTriangleIndices());

	// construct local and global transforms for each hair strand.
	ComputeTransforms();

	// compute tangent vectors
	ComputeStrandTangent();

	// compute thickness coefficients
	ComputeThicknessCoeffs();

	// compute rest lengths
	ComputeRestLengths();

	// triangle index
	FillTriangleIndexArray();

	for (int32 i = 0; i < NumTotalStrands; i++)
	{
		StrandTypes[i] = 0;
	}
	Bounds = this->CalcBounds();
	return true;
}
#endif

FBoxSphereBounds FTressFXRuntimeData::CalcBounds()
{
	FBox Box;
	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		Box += FVector(Positions[i]);
	}
	return FBoxSphereBounds(Box);
}


bool FTressFXRuntimeData::IsCompatibleSkeleton(const USkeletalMesh* InSkelMesh, TMap<int32, FName>& InBoneNameIndexMap)
{
	const FReferenceSkeleton& MeshRefSkel = InSkelMesh->RefSkeleton;
	const int32 NumBones = MeshRefSkel.GetRawBoneNum();

	for (int32 MeshBoneIndex = 0; MeshBoneIndex < NumBones; MeshBoneIndex++)
	{
		FName MeshBoneName = MeshRefSkel.GetBoneName(MeshBoneIndex);
		const int32 *TFXBoneIndex = InBoneNameIndexMap.FindKey(MeshBoneName);

		// if even one bone matches, just return true for now
		if (TFXBoneIndex && *TFXBoneIndex != INDEX_NONE)
		{
			return true;
		}
	}
	return false;
}

TArray<FVector> FTressFXRuntimeData::GetRootPositions()
{
	int32 VertsPerGuide = this->NumVerticesPerStrand;
	TArray<FVector> Roots;
	Roots.SetNumUninitialized(this->NumGuideStrands);

	for (int32 RootIndex = 0; RootIndex < this->NumGuideStrands; RootIndex++)
	{
		const FVector4 Vec = this->ImportedPositions[RootIndex * VertsPerGuide];
		Roots[RootIndex] = FVector(Vec.X, Vec.Y, Vec.Z);
	}
	return Roots;
}


bool FTressFXRuntimeData::LoadBoneData(const USkeletalMesh* SkeletalMesh, UTressFXBoneSkinningAsset* Asset)
{
	switch(Asset->AssetType)
	{
		case FTressFXBoneSkinngAssetType::TFXBone_Binary:
		{
			FBufferReader Reader(Asset->RawData.GetData(), Asset->RawData.Num(), false);
			int32 NumBones = 0;

			Reader.Serialize(&NumBones, sizeof(int32));

			size_t StartOfBoneNames = (sizeof(int32) * NumBones);
			size_t CurrentNamePosition = StartOfBoneNames;
			size_t CurrentOffsetPosition = 0;

			TArray<FString> BoneNames;
			BoneNames.AddZeroed(NumBones);
			BoneNameIndexMap.Empty();


			for (int32 i = 0; i < NumBones; i++)
			{
				int32 index = 0;
				Reader.Serialize(&index, sizeof(int32));

				int32 charLen = 0;
				Reader.Serialize(&charLen, sizeof(int32)); // character length includes null termination already.

				BoneNames[i].Append(TEXT(""), charLen);

				TArray<uint8> RawName;

				RawName.AddZeroed(sizeof(char)*charLen);

				Reader.Serialize(&RawName[0], sizeof(char)*charLen);

				BoneNames[i] = ANSI_TO_TCHAR((char*)RawName.GetData());

				BoneNameIndexMap.Add(i, *BoneNames[i]);
			}

			const bool bIsCompatable = IsCompatibleSkeleton(SkeletalMesh, BoneNameIndexMap);
			if (!bIsCompatable)
			{
				UE_LOG(TressFXAssetLog, Warning, TEXT("Input skeletal mesh not compatable with BoneSkinning Asset. Mesh:  %s"), *SkeletalMesh->GetFName().ToString());
				return false;
			}

			// Reading the number of strands
			int32 NumStrandsInStream = 0;
			Reader.Serialize(&NumStrandsInStream, sizeof(int32));

			int32 boneSkinningMemSize = NumTotalStrands;

			SkinningData.Empty();

			SkinningData.AddZeroed(boneSkinningMemSize);

			for (int32 i = 0; i < NumGuideStrands; ++i)
			{
				int32 index = 0; // Well, we don't really use this here.

				FTressFXBoneSkinningData SkinData;
				if (!Reader.AtEnd())
				{
					Reader.Serialize(&index, sizeof(int32));

					for (int32 j = 0; j < TRESSFX_MAX_INFLUENTIAL_BONE_COUNT; ++j)
					{
						//dont actually use this one, it will only work reliably if the skeletons exactly match
						int32 DummyBoneIndex;
						Reader.Serialize(&DummyBoneIndex, sizeof(int32));
						SkinData.BoneIndex[j] = (float)DummyBoneIndex;
						FName BoneName = *BoneNames[DummyBoneIndex];
						int32 idx = SkeletalMesh->Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
						SkinData.BoneIndex[j] = (float)idx; // Change the joint index to be what the engine wants
						if (SkinData.BoneIndex[j] == -1.f)
						{
							UE_LOG(LogTemp, Warning, TEXT("Bone index not found in reference skeleton. bone name: %s"), *BoneNames[(int)SkinData.BoneIndex[j]]);
						}
						Reader.Serialize(&SkinData.Weight[j], sizeof(float));
						if (SkinData.Weight[j] == 0.f)
						{
							SkinData.BoneIndex[j] = -1.f;
						}
					}
				}

				// If bone index is -1, then it means that there is no bone associated to this. In this case we simply replace it with zero.
				// This is safe because the corresponding weight should be zero anyway.
				SkinData.BoneIndex[0] = SkinData.BoneIndex[0] == -1.f ? 0 : SkinData.BoneIndex[0];
				SkinData.BoneIndex[1] = SkinData.BoneIndex[1] == -1.f ? 0 : SkinData.BoneIndex[1];
				SkinData.BoneIndex[2] = SkinData.BoneIndex[2] == -1.f ? 0 : SkinData.BoneIndex[2];
				SkinData.BoneIndex[3] = SkinData.BoneIndex[3] == -1.f ? 0 : SkinData.BoneIndex[3];

				//Result->BoneSkinningData.Add(SkinData);

				SkinningData[i * (NumFollowStrandsPerGuide + 1)] = SkinData;
			}
			return true;
			break;
		}
		case FTressFXBoneSkinngAssetType::TFXBone_Json:
		{
			int32 NumBones = Asset->JsonVersionImportData.NumBones;
			BoneNameIndexMap.Empty();

			for (int32 i = 0; i < NumBones; i++)
			{
				BoneNameIndexMap.Add(i, Asset->JsonVersionImportData.BoneNames[i]);
			}

			const bool bIsCompatable = IsCompatibleSkeleton(SkeletalMesh, BoneNameIndexMap);
			if (!bIsCompatable)
			{
				UE_LOG(TressFXAssetLog, Warning, TEXT("Input skeletal mesh not compatable with BoneSkinning Asset. Mesh:  %s"), *SkeletalMesh->GetFName().ToString());
				return false;
			}

			int32 boneSkinningMemSize = NumTotalStrands;
			SkinningData.Empty();
			SkinningData.AddZeroed(boneSkinningMemSize);
			for (int32 GuideStrandIndex = 0; GuideStrandIndex < NumGuideStrands; ++GuideStrandIndex)
			{
				FTressFXBoneSkinningData SkinData;

				for (int32 j = 0; j < TRESSFX_MAX_INFLUENTIAL_BONE_COUNT; ++j)
				{
					const int32 DataIndex = (GuideStrandIndex * 4) + j;

					if (DataIndex < Asset->JsonVersionImportData.JsonSkinningData.Num())
					{
						FTressFXBoneSkinningJSONImportData ImportedSkinData = Asset->JsonVersionImportData.JsonSkinningData[DataIndex];
						int32 EngineBoneIndex = SkeletalMesh->Skeleton->GetReferenceSkeleton().FindBoneIndex(ImportedSkinData.BoneName);
						SkinData.BoneIndex[j] = (float)EngineBoneIndex; // Change the joint index to be what the engine wants
						SkinData.Weight[j] = ImportedSkinData.Weight;
						if (SkinData.BoneIndex[j] == -1.f && ImportedSkinData.BoneName != NAME_None)
						{
							UE_LOG(LogTemp, Warning, TEXT("Bone name not found in reference skeleton. bone name: %s"), *ImportedSkinData.BoneName.ToString());
						}
						if (SkinData.Weight[j] == 0.f)
						{
							SkinData.BoneIndex[j] = -1.f;
						}
					}
				}

				// If bone index is -1, then it means that there is no bone associated to this. In this case we simply replace it with zero.
				// This is safe because the corresponding weight should be zero anyway.
				SkinData.BoneIndex[0] = SkinData.BoneIndex[0] == -1.f ? 0 : SkinData.BoneIndex[0];
				SkinData.BoneIndex[1] = SkinData.BoneIndex[1] == -1.f ? 0 : SkinData.BoneIndex[1];
				SkinData.BoneIndex[2] = SkinData.BoneIndex[2] == -1.f ? 0 : SkinData.BoneIndex[2];
				SkinData.BoneIndex[3] = SkinData.BoneIndex[3] == -1.f ? 0 : SkinData.BoneIndex[3];
				SkinningData[GuideStrandIndex * (NumFollowStrandsPerGuide + 1)] = SkinData;
			}
			return true;
			break;
		}
		default: { check(0); }		
	}
	return false;
}

void FTressFXRuntimeData::Clear()
{
}

void FTressFXRuntimeData::ComputeTransforms()
{
	FVector4* pos = Positions.GetData();
	FQuat* globalRot = GlobalRotations.GetData();
	FQuat* localRot = LocalRotations.GetData();
	FVector4* ref = RefVectors.GetData();

	// construct local and global transforms for all hair strands
	for (int32 iStrand = 0; iStrand < NumTotalStrands; ++iStrand)
	{
		int32 indexRootVertMaster = iStrand * NumVerticesPerStrand;

		// vertex 0
		{
			FVector4& vert_i = pos[indexRootVertMaster];
			FVector4& vert_i_plus_1 = pos[indexRootVertMaster + 1];

			const FVector vec = ToFVector(vert_i_plus_1) - ToFVector(vert_i);
			FVector       vecX = vec.GetSafeNormal();

			FVector vecZ = vecX ^ FVector(1.0, 0, 0);

			if (vecZ.SizeSquared() < 0.0001)
			{
				vecZ = vecX ^ FVector(0, 1.f, 0.f);
			}

			vecZ.Normalize();
			FVector vecY = (vecZ ^ vecX).GetSafeNormal();
			FMatrix rotL2W = FMatrix::Identity;

			rotL2W.M[0][0] = vecX.X;
			rotL2W.M[1][0] = vecY.X;
			rotL2W.M[2][0] = vecZ.X;
			rotL2W.M[0][1] = vecX.Y;
			rotL2W.M[1][1] = vecY.Y;
			rotL2W.M[2][1] = vecZ.Y;
			rotL2W.M[0][2] = vecX.Z;
			rotL2W.M[1][2] = vecY.Z;
			rotL2W.M[2][2] = vecZ.Z;

			FQuat rot(rotL2W);
			localRot[indexRootVertMaster] = globalRot[indexRootVertMaster] = rot;  // For vertex 0, local and global transforms are the same.
		}

		// vertex 1 through n-1
		for (int32 i = 1; i < (int)NumVerticesPerStrand; i++)
		{
			FVector4& vert_i_minus_1 = pos[indexRootVertMaster + i - 1];
			FVector4& vert_i = pos[indexRootVertMaster + i];
			FVector  vec = ToFVector(vert_i) - ToFVector(vert_i_minus_1);
			vec = globalRot[indexRootVertMaster + i - 1].Inverse() * vec;

			FVector vecX = vec.GetSafeNormal();
			FVector X = FVector(1.0f, 0, 0);
			FVector rotAxis = X ^ vecX;
			float angle = FMath::Acos(X | vecX);

			if (FMath::Abs(angle) < 0.001 || rotAxis.SizeSquared() < 0.001)
			{
				localRot[indexRootVertMaster + i] = FQuat::Identity;
			}
			else
			{
				rotAxis.Normalize();
				FQuat rot = FQuat(rotAxis, angle);
				localRot[indexRootVertMaster + i] = rot;
			}

			globalRot[indexRootVertMaster + i] = globalRot[indexRootVertMaster + i - 1] * localRot[indexRootVertMaster + i];
			ref[indexRootVertMaster + i] = vec;
		}
	}
}

void FTressFXRuntimeData::ComputeThicknessCoeffs()
{
	FVector4* pos = Positions.GetData();

	int32 index = 0;

	for (int32 iStrand = 0; iStrand < NumTotalStrands; ++iStrand)
	{
		int32   indexRootVertMaster = iStrand * NumVerticesPerStrand;
		float strandLength = 0;
		float tVal = 0;

		// vertex 1 through n
		for (int32 i = 1; i < (int)NumVerticesPerStrand; ++i)
		{
			FVector4& vert_i_minus_1 = pos[indexRootVertMaster + i - 1];
			FVector4& vert_i = pos[indexRootVertMaster + i];

			FVector vec = ToFVector(vert_i) - ToFVector(vert_i_minus_1);
			float        disSeg = vec.Size();

			tVal += disSeg;
			strandLength += disSeg;
		}

		for (int32 i = 0; i < (int)NumVerticesPerStrand; ++i)
		{
			tVal /= strandLength;
			ThicknessCoeffs[index++] = FMath::Sqrt(1.f - tVal * tVal);
		}
	}
}

void FTressFXRuntimeData::ComputeStrandTangent()
{
	FVector4* pos = Positions.GetData();
	FVector4* tan = Tangents.GetData();

	for (int32 iStrand = 0; iStrand < NumTotalStrands; ++iStrand)
	{
		int32 indexRootVertMaster = iStrand * NumVerticesPerStrand;

		// vertex 0
		{
			FVector4& vert_0 = pos[indexRootVertMaster];
			FVector4& vert_1 = pos[indexRootVertMaster + 1];

			FVector tangent = ToFVector(vert_1) - ToFVector(vert_0);
			tangent.Normalize();
			tan[indexRootVertMaster] = ToFVector4(tangent);
		}

		// vertex 1 through n-1
		for (int32 i = 1; i < (int)NumVerticesPerStrand - 1; i++)
		{
			FVector4& vert_i_minus_1 = pos[indexRootVertMaster + i - 1];
			FVector4& vert_i = pos[indexRootVertMaster + i];
			FVector4& vert_i_plus_1 = pos[indexRootVertMaster + i + 1];

			FVector tangent_pre = ToFVector(vert_i) - ToFVector(vert_i_minus_1);
			tangent_pre.Normalize();

			FVector tangent_next = ToFVector(vert_i_plus_1) - ToFVector(vert_i);
			tangent_next.Normalize();

			FVector tangent = ToFVector(tangent_pre) + ToFVector(tangent_next);
			tangent = tangent.GetSafeNormal();

			tan[indexRootVertMaster + i] = ToFVector4(tangent);
		}
	}
}

void FTressFXRuntimeData::ComputeRestLengths()
{
	FVector4* pos = Positions.GetData();
	float*        restLen = RestLengths.GetData();

	int32 index = 0;

	// Calculate rest lengths
	for (int32 i = 0; i < NumTotalStrands; i++)
	{
		int32 indexRootVert = i * NumVerticesPerStrand;

		for (int32 j = 0; j < NumVerticesPerStrand - 1; j++)
		{
			restLen[index++] = (ToFVector(pos[indexRootVert + j]) - ToFVector(pos[indexRootVert + j + 1])).Size();
		}

		// Since number of edges are one less than number of vertices in hair strand, below
		// line acts as a placeholder.
		restLen[index++] = 0;
	}
}

void FTressFXRuntimeData::FillTriangleIndexArray()
{
	check(NumTotalVertices == NumTotalStrands * NumVerticesPerStrand);

	int32 id = 0;
	int32 iCount = 0;

	VertexStrandLocation.Empty();

	for (int32 i = 0; i < NumTotalStrands; i++)
	{
		for (int32 j = 0; j < NumVerticesPerStrand - 1; j++)
		{
			VertexStrandLocation.Add(j == 0 ? 0 : 1.f / j);
			TriangleIndices[iCount++] = 2 * id;
			TriangleIndices[iCount++] = 2 * id + 1;
			TriangleIndices[iCount++] = 2 * id + 2;
			TriangleIndices[iCount++] = 2 * id + 2;
			TriangleIndices[iCount++] = 2 * id + 1;
			TriangleIndices[iCount++] = 2 * id + 3;
			id++;

		}

		id++;
	}

	check(iCount == 6 * NumTotalStrands * (NumVerticesPerStrand - 1));
}

//#pragma optimize("", on)