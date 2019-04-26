#pragma once

#include "RenderResource.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Containers/ResourceArray.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "Math/IntVector.h"
#include "ShaderParameters.h"

#pragma warning(push)
//no
#pragma warning( disable : 5038)



#define TRESSFX_MAX_INFLUENTIAL_BONE_COUNT 4

#define AMD_TRESSFX_VERSION_MAJOR                    4
#define AMD_TRESSFX_VERSION_MINOR                    0
#define AMD_TRESSFX_VERSION_PATCH                    0

//for nvidia, might want to change these to 32 ...
#define TRESSFX_SIM_THREAD_GROUP_SIZE 64
#define TRESSFX_MIN_VERTS_PER_STRAND_FOR_GPU_ITERATION 64

#define TRESSFX_DQ 0
#define TRESSFX_COLLISION_CAPSULES 1

#if TRESSFX_COLLISION_CAPSULES
	#define  TRESSFX_MAX_NUM_COLLISION_CAPSULES 10
#else 
	#define TRESSFX_MAX_NUM_COLLISION_CAPSULES 0
#endif

#ifndef AMD_TRESSFX_MAX_NUM_BONES
	#define AMD_TRESSFX_MAX_NUM_BONES 256
#endif 

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(FTressFXShadeParametersUniformBuffer, ENGINE_API)
	SHADER_PARAMETER(float, g_HairShadowAlpha)
	SHADER_PARAMETER(float, g_FiberRadius)
	SHADER_PARAMETER(float, g_FiberSpacing)
	SHADER_PARAMETER(int32, g_NumVerticesPerStrand)
	SHADER_PARAMETER(float, g_ratio)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(FTressFXSimParametersUniformBuffer, ENGINE_API)
SHADER_PARAMETER(FVector4, g_Wind)
SHADER_PARAMETER(FVector4, g_Wind1)
SHADER_PARAMETER(FVector4, g_Wind2)
SHADER_PARAMETER(FVector4, g_Wind3)
SHADER_PARAMETER(FVector4, g_Shape)
SHADER_PARAMETER(FVector4, g_GravTimeTip)
SHADER_PARAMETER(FIntVector4, g_SimInts)
SHADER_PARAMETER(FIntVector4, g_Counts)
SHADER_PARAMETER(FVector4, g_VSP)
#if TRESSFX_DQ
SHADER_PARAMETER_ARRAY(FVector4, g_BoneSkinningDQ, [AMD_TRESSFX_MAX_NUM_BONES * 2])
#else
SHADER_PARAMETER_ARRAY(FMatrix, g_BoneSkinningMatrix, [AMD_TRESSFX_MAX_NUM_BONES])
#endif

#if TRESSFX_COLLISION_CAPSULES
SHADER_PARAMETER_ARRAY(FVector4, g_centerAndRadius0, [TRESSFX_MAX_NUM_COLLISION_CAPSULES])
SHADER_PARAMETER_ARRAY(FVector4, g_centerAndRadius1, [TRESSFX_MAX_NUM_COLLISION_CAPSULES])
SHADER_PARAMETER(FIntVector4, g_numCollisionCapsules)
#endif

END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(FTressFXBoneSkinningUniformBuffer, ENGINE_API)
SHADER_PARAMETER_ARRAY(FMatrix, g_BoneSkinningMatrix, [AMD_TRESSFX_MAX_NUM_BONES])
SHADER_PARAMETER(int32, g_NumMeshVertices)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(FTressFXSDFUniformBuffer, ENGINE_API)
SHADER_PARAMETER(FVector4, g_Origin)
SHADER_PARAMETER(float, g_CellSize)
SHADER_PARAMETER(int32, g_NumCellsX)
SHADER_PARAMETER(int32, g_NumCellsY)
SHADER_PARAMETER(int32, g_NumCellsZ)
SHADER_PARAMETER(int32, g_MaxMarchingCubesVertices)
SHADER_PARAMETER(float, g_MarchingCubesIsolevel)
SHADER_PARAMETER(float, g_CollisionMargin)
SHADER_PARAMETER(int32, g_NumHairVerticesPerStrand)
SHADER_PARAMETER(int32, g_NumTotalHairVertices)
SHADER_PARAMETER(float, pad1)
SHADER_PARAMETER(float, pad2)
SHADER_PARAMETER(float, pad3)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

UENUM()
enum ETressFXCollisionType
{
	TFXCollsion_None UMETA(DisplayName = "No Collision"),
	TFXCollsion_Capsule UMETA(DisplayName = "Capsule Collision"),
	TFXCollsion_SDF UMETA(DisplayName = "Signed Distance Field Collision")
};


struct ETressFXRenderType
{
	enum Type
	{
		Opaque,
		ShortCut,
		KBuffer,
		AdaptiveTransparency,
		///////
		Num,
		Max = (Num - 1)
	};

};


struct FTressFXBoneSkinngAssetType
{
	enum Type
	{
		TFXBone_Binary = 0,
		TFXBone_Json = 1
	};
};


//Literally just a map of bone index, to bone Weights in the parent skel
struct ENGINE_API FTressFXBoneSkinningData
{
public:

	FTressFXBoneSkinningData() :BoneIndex(-1, -1, -1, -1), Weight(0, 0, 0, 0){}

	FVector4 BoneIndex;
	FVector4 Weight;

	friend FArchive& operator << (FArchive& Ar, FTressFXBoneSkinningData & Data)
	{
		return Ar << Data.BoneIndex << Data.Weight;
	}

	FTressFXBoneSkinningData& operator = (const FTressFXBoneSkinningData& Other)
	{
		this->BoneIndex = Other.BoneIndex;
		this->Weight = Other.Weight;
		return *this;
	}

};

class ENGINE_API FTressFXImportData
{

public:

	FTressFXImportData() {}

	virtual ~FTressFXImportData() {}

	int32 NumGuideStrands = 0;
	int32 NumVerticesPerStrand = 0;
	int32 NumTotalStrands = 0;
	int32 NumGuideVertices = 0;

	TArray<FVector4>	ImportedPositions;
	TArray<FVector4>	Positions;
	TArray<FVector2D>	StrandUV;

#if WITH_EDITORONLY_DATA
	//NEVER edit these after the intial import! They are needed for easy UV regeneration when changing number of followers.
	TArray<FVector2D>	StrandUV_Original;
#endif

	FRotator			ImportRotation;
	FVector				ImportTranslation;
	FVector				ImportScale;

	void Serialize(FArchive& Ar)
	{
		Ar << NumGuideVertices << NumVerticesPerStrand << NumTotalStrands << NumGuideStrands << Positions << StrandUV << ImportedPositions;
		Ar << ImportRotation << ImportTranslation;
		Ar << ImportScale;
// JAKETODO, i doubt this will deserialize correctly outside of the editor
#if WITH_EDITORONLY_DATA
		Ar << StrandUV_Original;
#endif
	}
};

/** Index Buffer */
class ENGINE_API FTressFXIndexBuffer : public FIndexBuffer
{
public:
	TArray<int32> Indices;
	int32 NumTotalIndices;

	virtual void InitDynamicRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), NumTotalIndices * sizeof(int32), BUF_Dynamic, CreateInfo, Buffer);

		// Write the indices to the index buffer.		
		FMemory::Memcpy(Buffer, Indices.GetData(), NumTotalIndices * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};


class ENGINE_API FTressFXRuntimeData : public FTressFXImportData
{
public:

	//explicit FTressFXRuntimeData(const FTressFXImportData& ImportData);
	virtual ~FTressFXRuntimeData() {}

	TArray<FVector4>	RefVectors;
	TArray<FQuat>		GlobalRotations;
	TArray<FQuat>		LocalRotations;
	TArray<FVector4>	Tangents;
	TArray<FVector4>	FollowRootOffsets;
	TArray<int32>		StrandTypes;
	TArray<float>		ThicknessCoeffs;
	TArray<float>		RestLengths;
	TArray<int32>		TriangleIndices;
	TArray<float>		VertexStrandLocation;

	FBoxSphereBounds	Bounds;

	TMap<int32, FName> BoneNameIndexMap;

	TArray<FTressFXBoneSkinningData> SkinningData;

	int32 NumTotalVertices = 0;
	int32 NumFollowStrandsPerGuide = 0;

	void Serialize(FArchive& Ar)
	{
		FTressFXImportData::Serialize(Ar);
		Ar << RefVectors;
		Ar << GlobalRotations;
		Ar << LocalRotations;
		Ar << Tangents;
		Ar << FollowRootOffsets;
		Ar << StrandTypes;
		Ar << ThicknessCoeffs;
		Ar << RestLengths;
		Ar << TriangleIndices;
		Ar << NumTotalVertices;
		Ar << NumFollowStrandsPerGuide;
		Ar << BoneNameIndexMap;
		Ar << SkinningData;
		Ar << Bounds;
	}

	void ProcessImportTransforms();

#if WITH_EDITORONLY_DATA
	//Generates follow hairs procedually.  If numFollowHairsPerGuideHair is zero, then this function won't do anything. 
	bool GenerateFollowHairs(int32 numFollowHairsPerGuideHair = 2, float tipSeparationFactor = 2, float maxRadiusAroundGuideHair = 1.2);
	// Computes various parameters for simulation and rendering. After calling this function, data is ready to be passed to hair object. 
	bool ProcessAsset(float TipSeparationFactor, float MaxRadiusAroundGuideHair, bool bProcessImportTransforms = false);
#endif
	FBoxSphereBounds CalcBounds();

	// Loads *.tfxbone data
	bool LoadBoneData(const class USkeletalMesh* SkeletalMesh, class UTressFXBoneSkinningAsset* Asset);

	inline uint32 GetNumHairSegments() const { return NumTotalStrands * (NumVerticesPerStrand - 1); }
	inline uint32 GetNumHairTriangleIndices() const { return 6 * GetNumHairSegments(); }
	inline uint32 GetNumHairLineIndices() const { return 2 * GetNumHairSegments(); }


	// Resets variables and clears up allocate buffers. 
	void Clear();

	// Helper functions for ProcessAsset. 
	void ComputeTransforms();
	void ComputeThicknessCoeffs();
	void ComputeStrandTangent();
	void ComputeRestLengths();
	void FillTriangleIndexArray();
	// Return true if even at least one bone is a match
	bool IsCompatibleSkeleton(const USkeletalMesh* InSkelMesh, TMap<int32,FName>& InBoneNameIndexMap);
	TArray<FVector> GetRootPositions();
	FTressFXRuntimeData() {}

};

class ENGINE_API FReadStructedBuffer
{

public:

	FStructuredBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;

	uint32 NumBytes;

	FReadStructedBuffer() : NumBytes(0) {}

	void Initialize(uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, uint32 AdditionalUsage = 0)
	{
		check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4);
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo;
		Buffer = RHICreateStructuredBuffer(BytesPerElement, NumBytes, BUF_ShaderResource | AdditionalUsage, CreateInfo);
		SRV = RHICreateShaderResourceView(Buffer);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		SRV.SafeRelease();
	}

};

class ENGINE_API FTressFXPosTanCollection
{

public:

	FTressFXPosTanCollection() {}

	FTressFXPosTanCollection(FTressFXRuntimeData* Data)
	{
		PositionsData = Data->Positions;
		TangentsData = Data->Tangents;

	}

	void InitResources(uint32 NumOfVerts);

	void ReleaseResources();

	void UAVBarrier(FRHICommandList& RHICmdList, FComputeFenceRHIParamRef Fence);

	void SetUAVs(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader);

	void UnsetUAVs(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader);

public:
	uint32 NumOfVerts;

	FRWBufferStructured Positions;
	FRWBufferStructured Tangents;
	FRWBufferStructured PositionsPrev;
	FRWBufferStructured PositionsPrevPrev;
	FRWBufferStructured TempTangents;

	TArray<FVector4> PositionsData;
	TArray<FVector4> TangentsData;
	TArray<FVector4> PositionsPrevData;
	TArray<FVector4> PositionsPrevPrevData;

};

extern void UploadGPUData(FStructuredBufferRHIParamRef Buffer, int32 ElementSize, int32 ElementCount, void* InData);

class ENGINE_API FTressFXHairObject : public FRenderResource
{

public:
	FTressFXHairObject(FTressFXRuntimeData* InAssetData = nullptr)
	{
		if (InAssetData)
		{
			UpdateTressFXData(InAssetData);
		}
	}

	void UpdateTressFXData(FTressFXRuntimeData* InAssetData)
	{
		AssetData = InAssetData;
		PosTanCollection = FTressFXPosTanCollection(InAssetData);
		NumTotalVertice = AssetData->NumTotalVertices;
		NumTotalStrands = AssetData->NumTotalStrands;
		NumVerticePerStrand = AssetData->NumVerticesPerStrand;

		InitialHairPositionsBufferData = AssetData->Positions;
		GlobalRotationsBufferData = AssetData->GlobalRotations;
		HairRestLengthSRVBufferData = AssetData->RestLengths;
		HairStrandTypeBufferData = AssetData->StrandTypes;
		HairRefVecsInLocalFrameBufferData = AssetData->RefVectors;
		FollowHairRootOffsetBufferData = AssetData->FollowRootOffsets;
		BoneSkinningDataBufferData = AssetData->SkinningData;

		HairVertexRenderParamsData = AssetData->ThicknessCoeffs;
		HairTexCoordsData = AssetData->StrandUV;

		mtotalIndices = AssetData->GetNumHairTriangleIndices();
		IndexBufferData = AssetData->TriangleIndices;
	}
	FTressFXRuntimeData* AssetData;

	int32 NumTotalVertice;
	int32 NumTotalStrands;
	int32 NumVerticePerStrand;
	int32 CPULocalShapeIterations;

	FTressFXPosTanCollection PosTanCollection;

	FReadStructedBuffer HairThicknessCoeffs;
	FReadStructedBuffer InitialHairPositionsBuffer;
	FReadStructedBuffer GlobalRotationsBuffer;
	FReadStructedBuffer HairRestLengthSRVBuffer;
	FReadStructedBuffer HairStrandTypeBuffer;
	FReadStructedBuffer HairRefVecsInLocalFrameBuffer;
	FReadStructedBuffer FollowHairRootOffsetBuffer;
	FReadStructedBuffer BoneSkinningDataBuffer;


	TArray<FVector4>					InitialHairPositionsBufferData;
	TArray<FQuat>						GlobalRotationsBufferData;
	TArray<float>						HairRestLengthSRVBufferData;
	TArray<int32>						HairStrandTypeBufferData;
	TArray<FVector4>					HairRefVecsInLocalFrameBufferData;
	TArray<FVector4>					FollowHairRootOffsetBufferData;
	TArray<FTressFXBoneSkinningData>	BoneSkinningDataBufferData;

	FTressFXIndexBuffer					IndexBuffer;
	uint32								mtotalIndices;
	ETressFXCollisionType				CollisionType;

	TArray<int32>						IndexBufferData;

	// SRVs for rendering
	FReadStructedBuffer					HairVertexRenderParams;
	FReadStructedBuffer					HairTexCoords;

	TArray<float>						HairVertexRenderParamsData;
	TArray<FVector2D>					HairTexCoordsData;

	TUniformBufferRef<FTressFXShadeParametersUniformBuffer> ShadeParametersUniformBuffer;
	TUniformBufferRef<FTressFXSimParametersUniformBuffer>  SimParametersUniformBuffer;
	TUniformBufferRef<FTressFXBoneSkinningUniformBuffer>  BoneSkinningUniformBuffer;
	TUniformBufferRef<FTressFXSDFUniformBuffer>  SDFUniformBuffer;

	virtual void InitDynamicRHI() override
	{
		PosTanCollection.InitResources(NumTotalVertice);

		FStructuredBufferRHIParamRef Resources[] = {
			PosTanCollection.Positions.Buffer,
			PosTanCollection.PositionsPrev.Buffer,
			PosTanCollection.PositionsPrevPrev.Buffer,
			PosTanCollection.Tangents.Buffer
		};

		{
			const uint32 SizeInBytes = NumTotalVertice * sizeof(FVector4);
			void*LockedData = RHILockStructuredBuffer(PosTanCollection.Positions.Buffer, 0, SizeInBytes, RLM_WriteOnly);
			FMemory::Memcpy(LockedData, (void *)PosTanCollection.PositionsData.GetData(), SizeInBytes);
			RHIUnlockStructuredBuffer(PosTanCollection.Positions.Buffer);
		}

		{
			const uint32 SizeInBytes = NumTotalVertice * sizeof(FVector4);
			void*LockedData = RHILockStructuredBuffer(PosTanCollection.PositionsPrev.Buffer, 0, SizeInBytes, RLM_WriteOnly);
			FMemory::Memcpy(LockedData, (void *)PosTanCollection.PositionsData.GetData(), SizeInBytes);
			RHIUnlockStructuredBuffer(PosTanCollection.PositionsPrev.Buffer);
		}

		{
			const uint32 SizeInBytes = NumTotalVertice * sizeof(FVector4);
			void*LockedData = RHILockStructuredBuffer(PosTanCollection.PositionsPrevPrev.Buffer, 0, SizeInBytes, RLM_WriteOnly);
			FMemory::Memcpy(LockedData, (void *)PosTanCollection.PositionsData.GetData(), SizeInBytes);
			RHIUnlockStructuredBuffer(PosTanCollection.PositionsPrevPrev.Buffer);
		}

		{
			const uint32 SizeInBytes = NumTotalVertice * sizeof(FVector4);
			void*LockedData = RHILockStructuredBuffer(PosTanCollection.Tangents.Buffer, 0, SizeInBytes, RLM_WriteOnly);
			FMemory::Memcpy(LockedData, (void *)PosTanCollection.TangentsData.GetData(), SizeInBytes);
			RHIUnlockStructuredBuffer(PosTanCollection.Tangents.Buffer);
		}

		InitialHairPositionsBuffer.Initialize(sizeof(FVector4), NumTotalVertice, PF_Unknown);
		GlobalRotationsBuffer.Initialize(sizeof(FVector4), NumTotalVertice, PF_Unknown);
		HairRestLengthSRVBuffer.Initialize(sizeof(float), NumTotalVertice, PF_Unknown);
		HairStrandTypeBuffer.Initialize(sizeof(int32), NumTotalStrands, PF_Unknown);
		HairRefVecsInLocalFrameBuffer.Initialize(sizeof(FVector4), NumTotalVertice, PF_Unknown);
		FollowHairRootOffsetBuffer.Initialize(sizeof(FVector4), NumTotalStrands, PF_Unknown);
		HairThicknessCoeffs.Initialize(sizeof(float), NumTotalVertice, PF_Unknown);
		BoneSkinningDataBuffer.Initialize(sizeof(FTressFXBoneSkinningData), NumTotalStrands, PF_Unknown);

		UploadGPUData(InitialHairPositionsBuffer.Buffer, sizeof(FVector4), NumTotalVertice, PosTanCollection.PositionsData.GetData());
		UploadGPUData(GlobalRotationsBuffer.Buffer, sizeof(FVector4), NumTotalVertice, GlobalRotationsBufferData.GetData());
		UploadGPUData(HairRestLengthSRVBuffer.Buffer, sizeof(float), NumTotalVertice, HairRestLengthSRVBufferData.GetData());
		UploadGPUData(HairStrandTypeBuffer.Buffer, sizeof(int), NumTotalStrands, HairStrandTypeBufferData.GetData());
		UploadGPUData(HairRefVecsInLocalFrameBuffer.Buffer, sizeof(FVector4), NumTotalVertice, HairRefVecsInLocalFrameBufferData.GetData());
		UploadGPUData(FollowHairRootOffsetBuffer.Buffer, sizeof(FVector4), NumTotalStrands, FollowHairRootOffsetBufferData.GetData());
		UploadGPUData(HairThicknessCoeffs.Buffer, sizeof(float), NumTotalVertice, HairVertexRenderParamsData.GetData());
		UploadGPUData(BoneSkinningDataBuffer.Buffer, sizeof(FTressFXBoneSkinningData), NumTotalStrands, BoneSkinningDataBufferData.GetData());

		IndexBuffer.Indices = IndexBufferData;
		IndexBuffer.NumTotalIndices = mtotalIndices;
		IndexBuffer.InitResource();

		HairTexCoords.Initialize(sizeof(FVector2D), NumTotalStrands, PF_Unknown);

		UploadGPUData(HairTexCoords.Buffer, sizeof(FVector2D), NumTotalStrands, HairTexCoordsData.GetData());
	}


	virtual void ReleaseDynamicRHI() override
	{
		InitialHairPositionsBuffer.Release();
		GlobalRotationsBuffer.Release();
		HairRestLengthSRVBuffer.Release();
		HairStrandTypeBuffer.Release();
		HairRefVecsInLocalFrameBuffer.Release();
		FollowHairRootOffsetBuffer.Release();
		HairThicknessCoeffs.Release();
		BoneSkinningDataBuffer.Release();
		PosTanCollection.ReleaseResources();
		IndexBuffer.ReleaseResource();
		HairTexCoords.Release();
	}
};


struct ENGINE_API FTressFXMeshVertex
{

public:

	FTressFXMeshVertex() {}

	FTressFXMeshVertex(const FVector &InPosition, const FVector &InNormal) : Position(InPosition), Normal(InNormal) {}

public:

	FVector Position;
	FVector Normal;

public:

	friend FArchive& operator << (FArchive& Ar, FTressFXMeshVertex & Data)
	{
		return Ar << Data.Position << Data.Normal;
	}

};

struct ENGINE_API FTressFXMeshData
{

public:

	FTressFXMeshData() {}

	FTressFXMeshData(const FTressFXMeshData& Other)
	{

		this->Vertices = Other.Vertices;
		this->Indices = Other.Indices;
		this->SkinningData = Other.SkinningData;
		this->BoundingBox = Other.BoundingBox;
		this->NumTriangles = Other.NumTriangles;
	}

	FTressFXMeshData & operator = (const FTressFXMeshData& Other)
	{
		this->Vertices = Other.Vertices;
		this->Indices = Other.Indices;
		this->SkinningData = Other.SkinningData;
		this->BoundingBox = Other.BoundingBox;
		this->NumTriangles = Other.NumTriangles;
		return *this;
	}


	void LoadData(
		const TArray<FVector>& InPositions,
		const TArray<FVector>& InNormals,
		const TArray<int32>& InIndices,
		const TArray<FTressFXBoneSkinningData> &InSkinningData,
		int32 InNumTriangles,
		FRotator InImportRotation = FRotator(0, 0, 0),
		FVector InImportTranslation = FVector(0, 0, 0),
		bool bProcessImportRoationAndTranslation = false
	)
	{
		check(InPositions.Num() == InNormals.Num());
		ImportRotation = InImportRotation;
		ImportTranslation = InImportTranslation;
		NumTriangles = InNumTriangles;
		for (int32 i = 0; i < InPositions.Num(); ++i)
		{
			Vertices.Add(FTressFXMeshVertex(InPositions[i], InNormals[i]));
		}
		Indices = InIndices;
		SkinningData = InSkinningData;
		if (bProcessImportRoationAndTranslation)
		{
			ProcessImportRotationAndTranslation();
		}
		BoundingBox = FBox(InPositions.GetData(), InPositions.Num());
	}

	void ProcessImportRotationAndTranslation()
	{
		if (!ImportRotation.IsNearlyZero())
		{
			for (int32 PosIndex = 0; PosIndex < Vertices.Num(); PosIndex++)
			{
				Vertices[PosIndex].Position = ImportRotation.RotateVector(Vertices[PosIndex].Position);
				Vertices[PosIndex].Normal = ImportRotation.RotateVector(Vertices[PosIndex].Normal);
			}
		}

		if (!ImportTranslation.IsNearlyZero())
		{
			for (int32 PosIndex = 0; PosIndex < Vertices.Num(); PosIndex++)
			{
				Vertices[PosIndex].Position.X += ImportTranslation.X;
				Vertices[PosIndex].Position.Y += ImportTranslation.Y;
				Vertices[PosIndex].Position.Z += ImportTranslation.Z;
			}
		}
	}

	friend FArchive& operator << (FArchive& Ar, FTressFXMeshData & Data)
	{
		return
			Ar << Data.Vertices
			<< Data.Indices
			<< Data.SkinningData
			<< Data.BoundingBox
			<< Data.NumTriangles
			<< Data.ImportRotation
			<< Data.ImportTranslation;
	}

	void GetBoundingBox(FVector& Min, FVector& Max)
	{
		//JAKETODO: make this dynamic...
		auto Box = this->BoundingBox;
		FVector BoxMin = Box.Min;
		FVector BoxMax = Box.Max;
		FMemory::Memcpy(&Min.X, &BoxMin.X, sizeof(float) * 3);
		FMemory::Memcpy(&Max.X, &BoxMax.X, sizeof(float) * 3);
	}


public:

	TArray<FTressFXMeshVertex> Vertices;
	TArray<int32> Indices;
	TArray<FTressFXBoneSkinningData> SkinningData;
	FBox BoundingBox;
	int32 NumTriangles;
	FRotator ImportRotation;
	FVector ImportTranslation;

};

class ENGINE_API FTressFXMeshResources : public FRenderResource
{

public:

	FTressFXMeshResources(FTressFXMeshData& InData)
		: MeshData(InData)
		, m_CollisionMargin(1.5f) //JAKETODO: dont hardcode the collision margin!
		, m_NumCellsInXAxis(60)
		, m_GridAllocationMutliplier(1.4f)
		, m_NumTotalCells(TNumericLimits<int32>::Max())
	{

		// initialize SDF grid using the associated model's bounding box
		FVector BoxMin, BoxMax;
		MeshData.GetBoundingBox(BoxMin, BoxMax); //JAKETODO bounding box needs to be dynamic, right now is static with whatever the shape is on first load
		m_CellSize = (BoxMax.X - BoxMin.X) / m_NumCellsInXAxis;
		int32 numExtraPaddingCells = (int32)(0.8f * (float)m_NumCellsInXAxis);
		m_PaddingBoundary = FVector(numExtraPaddingCells * m_CellSize, numExtraPaddingCells * m_CellSize, numExtraPaddingCells * m_CellSize);

		UpdateSDFGrid(BoxMin, BoxMax);
		BoxMin -= m_PaddingBoundary;
		BoxMax += m_PaddingBoundary;
		m_NumCellsX = (int32)((BoxMax.X - BoxMin.X) / m_CellSize);
		m_NumCellsY = (int32)((BoxMax.Y - BoxMin.Y) / m_CellSize);
		m_NumCellsZ = (int32)((BoxMax.Z - BoxMin.Z) / m_CellSize);
		m_NumTotalCells = FMath::Min(m_NumTotalCells, (int32)(m_GridAllocationMutliplier * m_NumCellsX * m_NumCellsY * m_NumCellsZ));
	}

	FTressFXSDFUniformBuffer GetConstantBuffer(FTressFXHairObject* HairObject)
	{
		FVector Min, Max;
		MeshData.GetBoundingBox(Min, Max);
		UpdateSDFGrid(Min, Max);
		FTressFXSDFUniformBuffer Buffer;
		// Set the constant buffer parameters
		Buffer.g_Origin.X = m_Origin.X;
		Buffer.g_Origin.Y = m_Origin.Y;
		Buffer.g_Origin.Z = m_Origin.Z;
		Buffer.g_Origin.W = 0;
		Buffer.g_CellSize = m_CellSize;
		Buffer.g_NumCellsX = m_NumCellsX;
		Buffer.g_NumCellsY = m_NumCellsY;
		Buffer.g_NumCellsZ = m_NumCellsZ;
		Buffer.g_CollisionMargin = m_CollisionMargin * m_CellSize;
		Buffer.g_NumTotalHairVertices = HairObject->NumTotalVertice;
		Buffer.g_NumHairVerticesPerStrand = HairObject->NumVerticePerStrand;
		Buffer.g_MarchingCubesIsolevel = 0; // i dont think these are used: JAKETODO remove
		Buffer.g_MaxMarchingCubesVertices = 128 * 1024; // i dont think these are used: JAKETODO remove
		return Buffer;
	}

	virtual void InitDynamicRHI() override
	{
		uint32 NumOfVerts = MeshData.Vertices.Num();
		{
			SignedDistanceFieldBuffer.Initialize(sizeof(int32), m_NumTotalCells);
		}

		{
			MeshVertBuffer.Initialize(sizeof(FTressFXMeshVertex), NumOfVerts);
			const uint32 SizeInBytes = NumOfVerts * sizeof(FTressFXMeshVertex);
			void*LockedData = RHILockStructuredBuffer(MeshVertBuffer.Buffer, 0, SizeInBytes, RLM_WriteOnly);
			FMemory::Memcpy(LockedData, (void *)MeshData.Vertices.GetData(), SizeInBytes);
			RHIUnlockStructuredBuffer(MeshVertBuffer.Buffer);
		}

		{
			SkinningDataBuffer.Initialize(sizeof(FTressFXBoneSkinningData), NumOfVerts, PF_Unknown);
			InitialVertexPositionBuffer.Initialize(sizeof(FTressFXMeshVertex), NumOfVerts, PF_Unknown);
			IndexBuffer.Initialize(sizeof(uint32), MeshData.Indices.Num(), PF_Unknown);
		}

		{
			UploadGPUData(SkinningDataBuffer.Buffer, sizeof(FTressFXBoneSkinningData), NumOfVerts, MeshData.SkinningData.GetData());
			UploadGPUData(InitialVertexPositionBuffer.Buffer, sizeof(FTressFXMeshVertex), NumOfVerts, MeshData.Vertices.GetData());
			UploadGPUData(IndexBuffer.Buffer, sizeof(uint32), MeshData.Indices.Num(), MeshData.Indices.GetData());
		}
	}

	virtual void ReleaseDynamicRHI() override
	{
		SignedDistanceFieldBuffer.Release();
		MeshVertBuffer.Release();
		SkinningDataBuffer.Release();
		InitialVertexPositionBuffer.Release();
		IndexBuffer.Release();
	}

public:
	FRWBufferStructured SignedDistanceFieldBuffer;
	FRWBufferStructured MeshVertBuffer;
	FReadStructedBuffer SkinningDataBuffer;
	FReadStructedBuffer InitialVertexPositionBuffer;
	FReadStructedBuffer IndexBuffer;
	FTressFXMeshData MeshData;

	void SetSDFCollisionMargin(float collisionMargin) { m_CollisionMargin = collisionMargin; }
	float GetSDFCollisionMargin() const { return m_CollisionMargin; }

	// Grid
	void SetNumCellsInXAxis(int numCellsInX) { m_NumCellsInXAxis = numCellsInX; }
	float GetGridCellSize() const { return m_CellSize; }
	FVector GetGridOrigin() const { return m_Origin; }
	void GetGridNumCells(int32& x, int32& y, int32& z) const
	{
		x = m_NumCellsX;
		y = m_NumCellsY;
		z = m_NumCellsZ;
	}
	int GetGridNumTotalCells() const { return m_NumTotalCells; }

private:

	// SDF grid
	FVector m_Origin;
	float m_CellSize;
	int32 m_NumCellsX;
	int32 m_NumCellsY;
	int32 m_NumCellsZ;
	int32 m_NumTotalCells;
	FVector m_Min;
	FVector m_Max;
	FVector m_PaddingBoundary;
	float m_GridAllocationMutliplier;

	// number of cells in X axis
	int32 m_NumCellsInXAxis;

	// SDF collision margin.
	float m_CollisionMargin;

	void UpdateSDFGrid(const FVector& TightBoxMin, const FVector& TightBoxMax)
	{
		FVector BoxMin = TightBoxMin - m_PaddingBoundary;
		m_Origin = BoxMin;
	};
};
#pragma warning( pop ) 