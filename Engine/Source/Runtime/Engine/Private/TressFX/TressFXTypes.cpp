#include "TressFX/TressFXTypes.h"


void FTressFXImportData::Serialize(FArchive& Ar)
{
	Ar << NumGuideVertices << NumVerticesPerStrand << NumTotalStrands << NumGuideStrands << Positions << StrandUV << ImportedPositions;
	Ar << ImportRotation << ImportTranslation;
	Ar << ImportScale;
	// JAKETODO, i doubt this will deserialize correctly outside of the editor
#if WITH_EDITORONLY_DATA
	Ar << StrandUV_Original;
#endif
}

void FTressFXIndexBuffer::InitDynamicRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* Buffer = nullptr;
	IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), NumTotalIndices * sizeof(int32), BUF_Dynamic, CreateInfo, Buffer);

	// Write the indices to the index buffer.		
	FMemory::Memcpy(Buffer, Indices.GetData(), NumTotalIndices * sizeof(int32));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

FTressFXRuntimeData::~FTressFXRuntimeData()
{

}

void FTressFXRuntimeData::Serialize(FArchive& Ar)
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

FTressFXRuntimeData::FTressFXRuntimeData()
{

}

FReadStructedBuffer::FReadStructedBuffer() : NumBytes(0)
{

}

void FReadStructedBuffer::Initialize(uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, uint32 AdditionalUsage /*= 0*/)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4);
	NumBytes = BytesPerElement * NumElements;
	FRHIResourceCreateInfo CreateInfo;
	Buffer = RHICreateStructuredBuffer(BytesPerElement, NumBytes, BUF_ShaderResource | AdditionalUsage, CreateInfo);
	SRV = RHICreateShaderResourceView(Buffer);
}

void FReadStructedBuffer::Release()
{
	NumBytes = 0;
	Buffer.SafeRelease();
	SRV.SafeRelease();
}

FTressFXPosTanCollection::FTressFXPosTanCollection(FTressFXRuntimeData* Data)
{
	PositionsData = Data->Positions;
	TangentsData = Data->Tangents;
}

FTressFXPosTanCollection::FTressFXPosTanCollection()
{

}

FTressFXHairObject::FTressFXHairObject(FTressFXRuntimeData* InAssetData /*= nullptr*/)
{
	if (InAssetData)
	{
		UpdateTressFXData(InAssetData);
	}
}

void FTressFXHairObject::UpdateTressFXData(FTressFXRuntimeData* InAssetData)
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

void FTressFXHairObject::InitDynamicRHI()
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

void FTressFXHairObject::ReleaseDynamicRHI()
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

FTressFXMeshData & FTressFXMeshData::operator=(const FTressFXMeshData& Other)
{
	this->Vertices = Other.Vertices;
	this->Indices = Other.Indices;
	this->SkinningData = Other.SkinningData;
	this->BoundingBox = Other.BoundingBox;
	this->NumTriangles = Other.NumTriangles;
	return *this;
}

FTressFXMeshData::FTressFXMeshData(const FTressFXMeshData& Other)
{
	this->Vertices = Other.Vertices;
	this->Indices = Other.Indices;
	this->SkinningData = Other.SkinningData;
	this->BoundingBox = Other.BoundingBox;
	this->NumTriangles = Other.NumTriangles;
}

FTressFXMeshData::FTressFXMeshData()
{

}

void FTressFXMeshData::LoadData(const TArray<FVector>& InPositions, const TArray<FVector>& InNormals, const TArray<int32>& InIndices, const TArray<FTressFXBoneSkinningData> &InSkinningData, int32 InNumTriangles, FRotator InImportRotation /*= FRotator(0, 0, 0)*/, FVector InImportTranslation /*= FVector(0, 0, 0)*/, bool bProcessImportRoationAndTranslation /*= false */)
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

void FTressFXMeshData::ProcessImportRotationAndTranslation()
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

void FTressFXMeshData::GetBoundingBox(FVector& Min, FVector& Max)
{
	//JAKETODO: make this dynamic...
	auto Box = this->BoundingBox;
	FVector BoxMin = Box.Min;
	FVector BoxMax = Box.Max;
	FMemory::Memcpy(&Min.X, &BoxMin.X, sizeof(float) * 3);
	FMemory::Memcpy(&Max.X, &BoxMax.X, sizeof(float) * 3);
}

FTressFXMeshResources::FTressFXMeshResources(FTressFXMeshData& InData) : MeshData(InData)
, m_NumTotalCells(TNumericLimits<int32>::Max())
, m_GridAllocationMutliplier(1.4f)
, m_NumCellsInXAxis(60)
, m_CollisionMargin(1.5f) //JAKETODO: dont hardcode the collision margin!
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

FTressFXSDFUniformBuffer FTressFXMeshResources::GetConstantBuffer(FTressFXHairObject* HairObject)
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

void FTressFXMeshResources::InitDynamicRHI()
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

void FTressFXMeshResources::ReleaseDynamicRHI()
{
	SignedDistanceFieldBuffer.Release();
	MeshVertBuffer.Release();
	SkinningDataBuffer.Release();
	InitialVertexPositionBuffer.Release();
	IndexBuffer.Release();
}

void FTressFXMeshResources::SetSDFCollisionMargin(float collisionMargin)
{
	m_CollisionMargin = collisionMargin;
}

float FTressFXMeshResources::GetSDFCollisionMargin() const
{
	return m_CollisionMargin;
}

void FTressFXMeshResources::SetNumCellsInXAxis(int numCellsInX)
{
	m_NumCellsInXAxis = numCellsInX;
}

float FTressFXMeshResources::GetGridCellSize() const
{
	return m_CellSize;
}

FVector FTressFXMeshResources::GetGridOrigin() const
{
	return m_Origin;
}

void FTressFXMeshResources::GetGridNumCells(int32& x, int32& y, int32& z) const
{
	x = m_NumCellsX;
	y = m_NumCellsY;
	z = m_NumCellsZ;
}

int FTressFXMeshResources::GetGridNumTotalCells() const
{
	return m_NumTotalCells;
}

void FTressFXMeshResources::UpdateSDFGrid(const FVector& TightBoxMin, const FVector& TightBoxMax)
{
	FVector BoxMin = TightBoxMin - m_PaddingBoundary;
	m_Origin = BoxMin;
}
