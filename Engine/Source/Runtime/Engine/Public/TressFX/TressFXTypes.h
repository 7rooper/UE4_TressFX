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
	SHADER_PARAMETER(float, g_FiberRadius)
	SHADER_PARAMETER(float, g_FiberSpacing)
	SHADER_PARAMETER(int32, g_NumVerticesPerStrand)
	SHADER_PARAMETER(float, g_ratio)
	SHADER_PARAMETER(FVector4, TressFXSettings1)
	SHADER_PARAMETER(FVector4, TressFXSettings2)
	SHADER_PARAMETER(FVector4, TressFXSpecularColor)
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
	TFXCollsion_PhysicsAsset UMETA(DisplayName = "Physics Asset"),
	TFXCollsion_SDF UMETA(DisplayName = "Signed Distance Field Collision")
};


struct ETressFXRenderType
{
	enum Type
	{
		Opaque,
		ShortCut,
		///////
		Num,
		Max = (Num - 1)
	};

};

static TArray<int32>  GTressFXAVSMNodeCounts(TArray<int32>({ 4, 8, 12, 16 }));
static TArray<int32>  GTressFXAVSMTextureSizes(TArray<int32>({ 128, 256, 512, 1024 }));

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

	void Serialize(FArchive& Ar);
};

/** Index Buffer */
class ENGINE_API FTressFXIndexBuffer : public FIndexBuffer
{
public:
	TArray<int32> Indices;
	int32 NumTotalIndices;

	virtual void InitDynamicRHI() override;
};


class ENGINE_API FTressFXRuntimeData : public FTressFXImportData
{
public:

	virtual ~FTressFXRuntimeData();

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

	void Serialize(FArchive& Ar);

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

	TArray<FVector> GetRootPositions();
	FTressFXRuntimeData();

};

class ENGINE_API FReadStructedBuffer
{

public:

	FStructuredBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;

	uint32 NumBytes;

	FReadStructedBuffer();

	void Initialize(uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, uint32 AdditionalUsage = 0);

	void Release();

};

class ENGINE_API FTressFXPosTanCollection
{

public:

	FTressFXPosTanCollection();

	FTressFXPosTanCollection(FTressFXRuntimeData* Data);

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
	FTressFXHairObject(FTressFXRuntimeData* InAssetData = nullptr);

	void UpdateTressFXData(FTressFXRuntimeData* InAssetData);
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

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;
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

	FTressFXMeshData();

	FTressFXMeshData(const FTressFXMeshData& Other);

	FTressFXMeshData & operator = (const FTressFXMeshData& Other);


	void LoadData(
		const TArray<FVector>& InPositions,
		const TArray<FVector>& InNormals,
		const TArray<int32>& InIndices,
		const TArray<FTressFXBoneSkinningData> &InSkinningData,
		int32 InNumTriangles,
		FRotator InImportRotation = FRotator(0, 0, 0),
		FVector InImportTranslation = FVector(0, 0, 0),
		bool bProcessImportRoationAndTranslation = false
	);

	void ProcessImportRotationAndTranslation();

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

	void GetBoundingBox(FVector& Min, FVector& Max);


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

	FTressFXMeshResources(FTressFXMeshData& InData);

	FTressFXSDFUniformBuffer GetConstantBuffer(FTressFXHairObject* HairObject);

	virtual void InitDynamicRHI() override;

	virtual void ReleaseDynamicRHI() override;

public:
	FRWBufferStructured SignedDistanceFieldBuffer;
	FRWBufferStructured MeshVertBuffer;
	FReadStructedBuffer SkinningDataBuffer;
	FReadStructedBuffer InitialVertexPositionBuffer;
	FReadStructedBuffer IndexBuffer;
	FTressFXMeshData MeshData;

	void SetSDFCollisionMargin(float collisionMargin);
	float GetSDFCollisionMargin() const;

	// Grid
	void SetNumCellsInXAxis(int numCellsInX);
	float GetGridCellSize() const;
	FVector GetGridOrigin() const;
	void GetGridNumCells(int32& x, int32& y, int32& z) const;
	int GetGridNumTotalCells() const;

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

	void UpdateSDFGrid(const FVector& TightBoxMin, const FVector& TightBoxMax);
};
#pragma warning( pop ) 