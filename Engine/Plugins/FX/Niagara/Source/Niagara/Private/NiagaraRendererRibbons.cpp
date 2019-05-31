// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererRibbons.h"
#include "ParticleResources.h"
#include "NiagaraRibbonVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"

DECLARE_CYCLE_STAT(TEXT("Generate Ribbon Vertex Data [GT]"), STAT_NiagaraGenRibbonVertexData, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Ribbons [RT]"), STAT_NiagaraRenderRibbons, STATGROUP_Niagara);

DECLARE_CYCLE_STAT(TEXT("Genereate GPU Buffers"), STAT_NiagaraGenRibbonGpuBuffers, STATGROUP_Niagara);

int32 GNiagaraRibbonCullTwistedStrips = 1;
static FAutoConsoleVariableRef CVarNiagaraRibbonCullTwistedStrips(
	TEXT("Niagara.Ribbon.CullTwistedStrips"),
	GNiagaraRibbonCullTwistedStrips,
	TEXT("Apply backface culling to remove twisted strips. Double indices. (default=1)"),
	ECVF_Default
);

float GNiagaraRibbonTessellationAngle = 15.f * (2.f * PI) / 360.f; // Every 15 degrees
static FAutoConsoleVariableRef CVarNiagaraRibbonTessellationAngle(
	TEXT("Niagara.Ribbon.Tessellation.MinAngle"),
	GNiagaraRibbonTessellationAngle,
	TEXT("Ribbon segment angle to tesselate in radian. \n")
	TEXT("Set 0 to use fixed tesselation. (default=15 degrees)"),
	ECVF_Scalability
);

int32 GNiagaraRibbonMaxTessellation = 64;
static FAutoConsoleVariableRef CVarNiagaraRibbonMaxTessellation(
	TEXT("Niagara.Ribbon.Tessellation.MaxInterp"),
	GNiagaraRibbonMaxTessellation,
	TEXT("When TessellationAngle is > 0, this is the maximum tesselation factor. \n")
	TEXT("Higher values allow more evenly divided tesselation. \n")
	TEXT("When TessellationAngle is 0, this is the actually tesselation factor (default=60)."),
	ECVF_Scalability
);

float GNiagaraRibbonTessellationScreenPercentage = 0.002f; 
static FAutoConsoleVariableRef CVarNiagaraRibbonTessellationScreenPercentage(
	TEXT("Niagara.Ribbon.Tessellation.MaxErrorScreenPercentage"),
	GNiagaraRibbonTessellationScreenPercentage,
	TEXT("Screen percentage used to compute the tessellation factor. \n")
	TEXT("Smaller values will generate more tessellation, up to max tesselltion. (default=0.002)"),
	ECVF_Scalability
);

float GNiagaraRibbonTessellationMinDisplacementError = 0.5f; 
static FAutoConsoleVariableRef CVarNiagaraRibbonTessellationMinDisplacementError(
	TEXT("Niagara.Ribbon.Tessellation.MinAbsoluteError"),
	GNiagaraRibbonTessellationMinDisplacementError,
	TEXT("Minimum absolute world size error when tessellating. \n")
	TEXT("Prevent over tessellating when distance gets really small. (default=0.5)"),
	ECVF_Scalability
);

float GNiagaraRibbonMinSegmentLength = 1.f;
static FAutoConsoleVariableRef CVarNiagaraRibbonMinSegmentLength(
	TEXT("Niagara.Ribbon.MinSegmentLength"),
	GNiagaraRibbonMinSegmentLength,
	TEXT("Min length of niagara ribbon segments. (default=1)"),
	ECVF_Scalability
);

float GNiagaraRibbonMaxAveraging = .98f;
static FAutoConsoleVariableRef CVarNiagaraRibbonMaxAveraging(
	TEXT("Niagara.Ribbon.Tessellation.MaxAveraging"),
	GNiagaraRibbonMaxAveraging,
	TEXT("Averaging value between the max metrics and the current metrics. (default=95%)"),
	ECVF_Scalability
);



// max absolute error 9.0x10^-3
// Eberly's polynomial degree 1 - respect bounds
// input [-1, 1] and output [0, PI]
FORCEINLINE float AcosFast(float InX) 
{
    float X = FMath::Abs(InX);
    float Res = -0.156583f * X + (0.5 * PI);
    Res *= sqrt(1.0f - X);
    return (InX >= 0) ? Res : PI - Res;
}

struct FNiagaraDynamicDataRibbon : public FNiagaraDynamicDataBase
{
	// The list of all segments, each one connecting SortedIndices[SegmentId] to SortedIndices[SegmentId + 1].
	// We use this format because the final index buffer gets generated based on view sorting and InterpCount.
	TArray<int32> SegmentData;
	TArray<int32> SortedIndices;
	TArray<FVector4> TangentAndDistances;
	TArray<uint32> MultiRibbonIndices;
	TArray<float> PackedPerRibbonDataByIndex;

	//Direct ptr to the dataset. ONLY FOR USE BE GPU EMITTERS.
	//TODO: Even this needs to go soon.
	const FNiagaraDataSet *DataSet;

	// start and end world space position of the ribbon, to figure out draw direction
	FVector StartPos;
	FVector EndPos;

	void PackPerRibbonData(float U0Scale, float U0Offset, float U1Scale, float U1Offset, uint32 NumSegments, uint32 FirstParticleId)
	{
		const float OneOverNumSegments = 1 / (float)FMath::Max<uint32>(1, NumSegments);
		PackedPerRibbonDataByIndex.Add(U0Scale);
		PackedPerRibbonDataByIndex.Add(U0Offset);
		PackedPerRibbonDataByIndex.Add(U1Scale);
		PackedPerRibbonDataByIndex.Add(U1Offset);
		PackedPerRibbonDataByIndex.Add(OneOverNumSegments);
		PackedPerRibbonDataByIndex.Add(*(float*)&FirstParticleId);
	}
};

class FNiagaraMeshCollectorResourcesRibbon : public FOneFrameResource
{
public:
	FNiagaraRibbonVertexFactory VertexFactory;
	FNiagaraRibbonUniformBufferRef UniformBuffer;

	virtual ~FNiagaraMeshCollectorResourcesRibbon()
	{
		VertexFactory.ReleaseResource();
	}
};


NiagaraRendererRibbons::NiagaraRendererRibbons(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *InProps) :
	NiagaraRenderer()
	, PositionDataOffset(INDEX_NONE)
	, VelocityDataOffset(INDEX_NONE)
	, WidthDataOffset(INDEX_NONE)
	, TwistDataOffset(INDEX_NONE)
	, FacingDataOffset(INDEX_NONE)
	, ColorDataOffset(INDEX_NONE)
	, NormalizedAgeDataOffset(INDEX_NONE)
	, MaterialRandomDataOffset(INDEX_NONE)
	, LastSyncedId(INDEX_NONE)
{
	VertexFactory = new FNiagaraRibbonVertexFactory(NVFT_Ribbon, FeatureLevel);
	Properties = Cast<UNiagaraRibbonRendererProperties>(InProps);
}


void NiagaraRendererRibbons::ReleaseRenderThreadResources()
{
	VertexFactory->ReleaseResource();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

// FPrimitiveSceneProxy interface.
void NiagaraRendererRibbons::CreateRenderThreadResources()
{
	VertexFactory->InitResource();
}

void NiagaraRendererRibbons::GenerateIndexBuffer(uint16* OutIndices, const TArray<int32>& SegmentData,  int32 InterpCount, bool bInvertOrder, bool bCullTwistedStrips)
{
	auto AddTriangleIndices = [&OutIndices, InterpCount, bCullTwistedStrips](int32 SegmentIndex)
	{
		for (int32 SubSegmentIndex = 0; SubSegmentIndex < InterpCount; ++SubSegmentIndex)
		{
			const uint16 BaseVertexIndex = (int16)(SegmentIndex * InterpCount + SubSegmentIndex) * 2;
			OutIndices[0] = BaseVertexIndex + 0;
			OutIndices[1] = BaseVertexIndex + 1;
			OutIndices[2] = BaseVertexIndex + 2;
			OutIndices[3] = BaseVertexIndex + 1;
			OutIndices[4] = BaseVertexIndex + 3;
			OutIndices[5] = BaseVertexIndex + 2;

			if (bCullTwistedStrips)
			{
				// When the ribbon right vector abruptly changes direction, for example when a segment starts with a tangent going up, but ends with a tanging going down,
				// the triangles defined above will cross each other. One will be clockwise and the other counter-clockwise. We add two triangle here that replaces the invalid one
				// this prevents the crossing/twisting artefacts while putting a bit more pressure on triangle culling.

				OutIndices[6] = BaseVertexIndex + 0;
				OutIndices[7] = BaseVertexIndex + 2;
				OutIndices[8] = BaseVertexIndex + 3;
				OutIndices[9] = BaseVertexIndex + 0;
				OutIndices[10] = BaseVertexIndex + 3;
				OutIndices[11] = BaseVertexIndex + 1;
				OutIndices += 12;
			}
			else
			{
				OutIndices += 6;
			}
		}
	};

	// If per view sorting is required, generate sort keys and sort segment indices.
	if (!bInvertOrder)
	{
		for (int32 SegmentDataIndex = 0; SegmentDataIndex < SegmentData.Num(); ++SegmentDataIndex)
		{
			AddTriangleIndices(SegmentData[SegmentDataIndex]);
		}
	}
	else
	{
		for (int32 SegmentDataIndex = SegmentData.Num() - 1; SegmentDataIndex >= 0; --SegmentDataIndex)
		{
			AddTriangleIndices(SegmentData[SegmentDataIndex]);
		}
	}
}

void NiagaraRendererRibbons::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRender);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderRibbons);

	SimpleTimer MeshElementsTimer;
	FNiagaraDynamicDataRibbon *DynamicDataRibbon = static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender);
	if (!DynamicDataRibbon 
		|| DynamicDataRibbon->SegmentData.Num() == 0
		|| nullptr == Properties
		|| !GSupportsResourceView // Current shader requires SRV to draw properly in all cases.
		)
	{
		return;
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	const bool bCullTwistedStrips = Properties->FacingMode == ENiagaraRibbonFacingMode::Screen && GNiagaraRibbonCullTwistedStrips != 0;

	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy();

	FGlobalDynamicIndexBuffer& DynamicIndexBuffer = Collector.GetDynamicIndexBuffer();

	const int32 TotalFloatSize = DynamicDataRibbon->RTParticleData.GetFloatBuffer().Num() / sizeof(float);
	FGlobalDynamicReadBuffer::FAllocation ParticleData;

	if (DynamicDataRibbon->DataSet->GetSimTarget() == ENiagaraSimTarget::CPUSim)
	{
		FGlobalDynamicReadBuffer& DynamicReadBuffer = Collector.GetDynamicReadBuffer();
		ParticleData = DynamicReadBuffer.AllocateFloat(TotalFloatSize);
		FMemory::Memcpy(ParticleData.Buffer, DynamicDataRibbon->RTParticleData.GetFloatBuffer().GetData(), DynamicDataRibbon->RTParticleData.GetFloatBuffer().Num());
	}

	// Update the primitive uniform buffer if needed.
	if (!WorldSpacePrimitiveUniformBuffer.IsInitialized())
	{
		FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters = GetPrimitiveUniformShaderParameters(
			FMatrix::Identity,
			FMatrix::Identity,
			SceneProxy->GetActorPosition(),
			SceneProxy->GetBounds(),
			SceneProxy->GetLocalBounds(),
			SceneProxy->ReceivesDecals(),
			false,
			false,
			SceneProxy->UseSingleSampleShadowFromStationaryLights(),
			SceneProxy->GetScene().HasPrecomputedVolumetricLightmap_RenderThread(),
			SceneProxy->UseEditorDepthTest(),
			SceneProxy->GetLightingChannelMask(),
			0,
			INDEX_NONE,
			INDEX_NONE,
			SceneProxy->AlwaysHasVelocity()
			);
		WorldSpacePrimitiveUniformBuffer.SetContents(PrimitiveUniformShaderParameters);
		WorldSpacePrimitiveUniformBuffer.InitResource();
	}

	// Compute the per-view uniform buffers.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			check(View);

			const FVector ViewOriginForDistanceCulling = View->ViewMatrices.GetViewOrigin();

			int32 SegmentTessellation = 1;
			int32 NumSegments = DynamicDataRibbon->SegmentData.Num();
			if (GNiagaraRibbonMaxTessellation > 1 && TessellationCurvature > SMALL_NUMBER && ViewFamily.GetFeatureLevel() == ERHIFeatureLevel::SM5)
			{
				const float MinTesselation = FMath::Max<float>(1.f, FMath::Max(TessellationTwistAngle, TessellationAngle) / FMath::Max<float>(SMALL_NUMBER, GNiagaraRibbonTessellationAngle));

				const float ViewDistance = SceneProxy->GetBounds().ComputeSquaredDistanceFromBoxToPoint(ViewOriginForDistanceCulling);
				const float MaxDisplacementError = FMath::Max(GNiagaraRibbonTessellationMinDisplacementError, GNiagaraRibbonTessellationScreenPercentage * FMath::Sqrt(ViewDistance) / View->LODDistanceFactor);
				float Tess = TessellationAngle / FMath::Max(SMALL_NUMBER, AcosFast(TessellationCurvature / (TessellationCurvature + MaxDisplacementError)));
				// FMath::RoundUpToPowerOfTwo ? This could avoid vertices moving around as tesselation increases

				if (TessellationTwistAngle > 0 && TessellationTwistCurvature > 0)
				{
					const float TwistTess  = TessellationTwistAngle / FMath::Max(SMALL_NUMBER, AcosFast(TessellationTwistCurvature / (TessellationTwistCurvature + MaxDisplacementError)));
					Tess = FMath::Max(TwistTess, Tess);
				}
				SegmentTessellation = FMath::Clamp<int32>(FMath::RoundToInt(Tess), FMath::RoundToInt(MinTesselation), GNiagaraRibbonMaxTessellation);
				NumSegments *= SegmentTessellation;
			}

			const int32 NumberOfPrimitives = NumSegments * 2 * (bCullTwistedStrips ? 2 : 1);

			// Figure out whether start is closer to the view plane than end
			// TODO : This doesn't work with multi-ribbons.
			const float StartDist = FVector::DotProduct(View->GetViewDirection(), DynamicDataRibbon->StartPos - ViewOriginForDistanceCulling);
			const float EndDist = FVector::DotProduct(View->GetViewDirection(), DynamicDataRibbon->EndPos - ViewOriginForDistanceCulling);
			const bool bInvertOrder = ((StartDist > EndDist) && Properties->DrawDirection == ENiagaraRibbonDrawDirection::BackToFront)
					|| ((StartDist < EndDist) && Properties->DrawDirection == ENiagaraRibbonDrawDirection::FrontToBack);

			// Copy the vertex data over.
			FGlobalDynamicIndexBuffer::FAllocation DynamicIndexAllocation = DynamicIndexBuffer.Allocate(NumberOfPrimitives * 3, sizeof(int16));
			GenerateIndexBuffer((uint16*)DynamicIndexAllocation.Buffer, DynamicDataRibbon->SegmentData, SegmentTessellation, bInvertOrder, bCullTwistedStrips); 

			FNiagaraMeshCollectorResourcesRibbon& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesRibbon>();
			FNiagaraRibbonUniformParameters PerViewUniformParameters;// = UniformParameters;
			PerViewUniformParameters.LocalToWorld = bLocalSpace ? SceneProxy->GetLocalToWorld() : FMatrix::Identity;//For now just handle local space like this but maybe in future have a VF variant to avoid the transform entirely?
			PerViewUniformParameters.LocalToWorldInverseTransposed = bLocalSpace ? SceneProxy->GetLocalToWorld().Inverse().GetTransposed() : FMatrix::Identity;
			PerViewUniformParameters.DeltaSeconds = ViewFamily.DeltaWorldTime;
			PerViewUniformParameters.CameraUp = View->GetViewUp(); // FVector4(0.0f, 0.0f, 1.0f, 0.0f);
			PerViewUniformParameters.CameraRight = View->GetViewRight();//	FVector4(1.0f, 0.0f, 0.0f, 0.0f);
			PerViewUniformParameters.ScreenAlignment = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
			PerViewUniformParameters.TotalNumInstances = DynamicDataRibbon->RTParticleData.GetNumInstances();
			PerViewUniformParameters.InterpCount = SegmentTessellation;
			PerViewUniformParameters.OneOverInterpCount = 1.f / (float)SegmentTessellation;

			PerViewUniformParameters.PositionDataOffset = PositionDataOffset;
			PerViewUniformParameters.VelocityDataOffset = VelocityDataOffset;
			PerViewUniformParameters.ColorDataOffset = ColorDataOffset;
			PerViewUniformParameters.WidthDataOffset = WidthDataOffset;
			PerViewUniformParameters.TwistDataOffset = TwistDataOffset;
			PerViewUniformParameters.FacingDataOffset = Properties->FacingMode == ENiagaraRibbonFacingMode::Custom ? FacingDataOffset : -1;
			PerViewUniformParameters.NormalizedAgeDataOffset = NormalizedAgeDataOffset;
			PerViewUniformParameters.MaterialRandomDataOffset = MaterialRandomDataOffset;
			PerViewUniformParameters.MaterialParamDataOffset = MaterialParamOffset;
			PerViewUniformParameters.MaterialParam1DataOffset = MaterialParamOffset1;
			PerViewUniformParameters.MaterialParam2DataOffset = MaterialParamOffset2;
			PerViewUniformParameters.MaterialParam3DataOffset = MaterialParamOffset3;
			PerViewUniformParameters.OneOverUV0TilingDistance = Properties->UV0TilingDistance ? 1.f / (Properties->UV0TilingDistance) : 0.f;
			PerViewUniformParameters.OneOverUV1TilingDistance = Properties->UV1TilingDistance ? 1.f / (Properties->UV1TilingDistance) : 0.f;
			PerViewUniformParameters.PackedVData = FVector4(Properties->UV0Scale.Y, Properties->UV0Offset.Y, Properties->UV1Scale.Y, Properties->UV1Offset.Y);
			CollectorResources.VertexFactory.SetParticleData(ParticleData.ReadBuffer->SRV, ParticleData.FirstIndex / sizeof(float), DynamicDataRibbon->RTParticleData.GetFloatStride() / sizeof(float));

			// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory
			CollectorResources.VertexFactory.SetParticleFactoryType(NVFT_Ribbon);

			CollectorResources.UniformBuffer = FNiagaraRibbonUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

			CollectorResources.VertexFactory.InitResource();
			CollectorResources.VertexFactory.SetRibbonUniformBuffer(CollectorResources.UniformBuffer);

			if (!DynamicDataRibbon->SortedIndices.Num())
			{
				return;
			}

			// TODO: need to make these two a global alloc buffer as well, not recreate
			// pass in the sorted indices so the VS can fetch the particle data in order
			FReadBuffer SortedIndicesBuffer;
			SortedIndicesBuffer.Initialize(sizeof(int32), DynamicDataRibbon->SortedIndices.Num(), EPixelFormat::PF_R32_SINT, BUF_Volatile);
			void *IndexPtr = RHILockVertexBuffer(SortedIndicesBuffer.Buffer, 0, DynamicDataRibbon->SortedIndices.Num() * sizeof(int32), RLM_WriteOnly);
			FMemory::Memcpy(IndexPtr, DynamicDataRibbon->SortedIndices.GetData(), DynamicDataRibbon->SortedIndices.Num() * sizeof(int32));
			RHIUnlockVertexBuffer(SortedIndicesBuffer.Buffer);
			CollectorResources.VertexFactory.SetSortedIndices(SortedIndicesBuffer.SRV, 0);

			// pass in the CPU generated total segment distance (for tiling distance modes); needs to be a buffer so we can fetch them in the correct order based on Draw Direction (front->back or back->front)
			//	otherwise UVs will pop when draw direction changes based on camera view point
			FReadBuffer TangentsAndDistancesBuffer;
			TangentsAndDistancesBuffer.Initialize(sizeof(FVector4), DynamicDataRibbon->TangentAndDistances.Num(), EPixelFormat::PF_A32B32G32R32F, BUF_Volatile);
			void *TangentsAndDistancesPtr = RHILockVertexBuffer(TangentsAndDistancesBuffer.Buffer, 0, DynamicDataRibbon->TangentAndDistances.Num() * sizeof(FVector4), RLM_WriteOnly);
			FMemory::Memcpy(TangentsAndDistancesPtr, DynamicDataRibbon->TangentAndDistances.GetData(), DynamicDataRibbon->TangentAndDistances.Num() * sizeof(FVector4));
			RHIUnlockVertexBuffer(TangentsAndDistancesBuffer.Buffer);
			CollectorResources.VertexFactory.SetTangentAndDistances(TangentsAndDistancesBuffer.SRV);

			// Copy a buffer which has the per particle multi ribbon index.
			FReadBuffer MultiRibbonIndicesBuffer;
			MultiRibbonIndicesBuffer.Initialize(sizeof(uint32), DynamicDataRibbon->MultiRibbonIndices.Num(), EPixelFormat::PF_R32_UINT, BUF_Volatile);
			void* MultiRibbonIndexPtr = RHILockVertexBuffer(MultiRibbonIndicesBuffer.Buffer, 0, DynamicDataRibbon->MultiRibbonIndices.Num() * sizeof(uint32), RLM_WriteOnly);
			FMemory::Memcpy(MultiRibbonIndexPtr, DynamicDataRibbon->MultiRibbonIndices.GetData(), DynamicDataRibbon->MultiRibbonIndices.Num() * sizeof(uint32));
			RHIUnlockVertexBuffer(MultiRibbonIndicesBuffer.Buffer);
			CollectorResources.VertexFactory.SetMultiRibbonIndicesSRV(MultiRibbonIndicesBuffer.SRV);

			// Copy the packed u data for stable age based uv generation.
			FReadBuffer PackedPerRibbonDataByIndexBuffer;
			PackedPerRibbonDataByIndexBuffer.Initialize(sizeof(float), DynamicDataRibbon->PackedPerRibbonDataByIndex.Num(), EPixelFormat::PF_R32_FLOAT, BUF_Volatile);
			void *PackedPerRibbonDataByIndexPtr = RHILockVertexBuffer(PackedPerRibbonDataByIndexBuffer.Buffer, 0, DynamicDataRibbon->PackedPerRibbonDataByIndex.Num() * sizeof(float), RLM_WriteOnly);
			FMemory::Memcpy(PackedPerRibbonDataByIndexPtr, DynamicDataRibbon->PackedPerRibbonDataByIndex.GetData(), DynamicDataRibbon->PackedPerRibbonDataByIndex.Num() * sizeof(float));
			RHIUnlockVertexBuffer(PackedPerRibbonDataByIndexBuffer.Buffer);
			CollectorResources.VertexFactory.SetPackedPerRibbonDataByIndexSRV(PackedPerRibbonDataByIndexBuffer.SRV);

			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			MeshBatch.VertexFactory = &CollectorResources.VertexFactory;
			MeshBatch.CastShadow = SceneProxy->CastsDynamicShadow();
			MeshBatch.bUseAsOccluder = false;
			MeshBatch.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
			MeshBatch.bDisableBackfaceCulling = !bCullTwistedStrips;
			MeshBatch.Type = PT_TriangleList;
			MeshBatch.DepthPriorityGroup = SceneProxy->GetDepthPriorityGroup(View);
			MeshBatch.bCanApplyViewModeOverrides = true;
			MeshBatch.bUseWireframeSelectionColoring = SceneProxy->IsSelected();

			if (bIsWireframe)
			{
				MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			}
			else
			{
				MeshBatch.MaterialRenderProxy = MaterialRenderProxy;
			}

			FMeshBatchElement& MeshElement = MeshBatch.Elements[0];
			MeshElement.IndexBuffer = DynamicIndexAllocation.IndexBuffer;
			MeshElement.FirstIndex = DynamicIndexAllocation.FirstIndex;
			MeshElement.NumPrimitives = NumberOfPrimitives;
			check(MeshElement.NumPrimitives > 0);
			MeshElement.NumInstances = 1;
			MeshElement.MinVertexIndex = 0;
			MeshElement.MaxVertexIndex = 0;
			MeshElement.PrimitiveUniformBuffer = WorldSpacePrimitiveUniformBuffer.GetUniformBufferRHI();

			Collector.AddMesh(ViewIndex, MeshBatch);
		}
	}

	CPUTimeMS += MeshElementsTimer.GetElapsedMilliseconds();
}

void NiagaraRendererRibbons::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	check(IsInRenderingThread());

	if (DynamicDataRender)
	{
		delete static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender);
		DynamicDataRender = NULL;
	}
	DynamicDataRender = NewDynamicData;
}

int NiagaraRendererRibbons::GetDynamicDataSize()
{
	uint32 Size = sizeof(FNiagaraDynamicDataRibbon);
	if (DynamicDataRender)
	{
		FNiagaraDynamicDataRibbon* RibbonDynamicData = static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender);
		Size += RibbonDynamicData->SegmentData.GetAllocatedSize();
		Size += RibbonDynamicData->SortedIndices.GetAllocatedSize();
		Size += RibbonDynamicData->TangentAndDistances.GetAllocatedSize();
		Size += RibbonDynamicData->MultiRibbonIndices.GetAllocatedSize();
		Size += RibbonDynamicData->PackedPerRibbonDataByIndex.GetAllocatedSize();
	}

	return Size;
}

bool NiagaraRendererRibbons::HasDynamicData()
{
	return DynamicDataRender && (static_cast<FNiagaraDynamicDataRibbon*>(DynamicDataRender))->SegmentData.Num() > 0;
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& NiagaraRendererRibbons::GetRequiredAttributes()
{
	return Properties->GetRequiredAttributes();
}

const TArray<FNiagaraVariable>& NiagaraRendererRibbons::GetOptionalAttributes()
{
	return Properties->GetOptionalAttributes();
}

#endif


bool NiagaraRendererRibbons::SetMaterialUsage()
{
	return Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraRibbons);
}

void NiagaraRendererRibbons::TransformChanged()
{
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void CalculateUVScaleAndOffsets(
	const FNiagaraDataSetAccessor<float>& SortKeyData, const TArray<int32>& RibbonIndices,
	bool bSortKeyIsAge, int32 StartIndex, int32 EndIndex, int32 NumSegments,
	float InUTilingDistance, float InUScale, float InUOffset, ENiagaraRibbonAgeOffsetMode InAgeOffsetMode,
	float& OutUScale, float& OutUOffset)
{
	if (EndIndex - StartIndex > 0 && bSortKeyIsAge && InUTilingDistance == 0)
	{
		float AgeUScale;
		float AgeUOffset;
		if (InAgeOffsetMode == ENiagaraRibbonAgeOffsetMode::Scale)
		{
			// In scale mode we scale and offset the UVs so that no part of the texture is clipped. In order to prevent
			// clipping at the ends we'll have to move the UVs in up to the size of a single segment of the ribbon since
			// that's the distance we'll need to to smoothly interpolate when a new segment is added, or when an old segment
			// is removed.  We calculate the end offset when the end of the ribbon is within a single time step of 0 or 1
			// which is then normalized to the range of a single segment.  We can then calculate how many segments we actually
			// have to draw the scaled ribbon, and can offset the start by the correctly scaled offset.
			float FirstAge = SortKeyData[RibbonIndices[StartIndex]];
			float SecondAge = SortKeyData[RibbonIndices[StartIndex + 1]];
			float SecondToLastAge = SortKeyData[RibbonIndices[EndIndex - 1]];
			float LastAge = SortKeyData[RibbonIndices[EndIndex]];

			float StartTimeStep = SecondAge - FirstAge;
			float StartTimeOffset = FirstAge < StartTimeStep ? StartTimeStep - FirstAge : 0;
			float StartSegmentOffset = StartTimeOffset / StartTimeStep;

			float EndTimeStep = LastAge - SecondToLastAge;
			float EndTimeOffset = 1 - LastAge < EndTimeStep ? EndTimeStep - (1 - LastAge) : 0;
			float EndSegmentOffset = EndTimeOffset / EndTimeStep;

			float AvailableSegments = NumSegments - (StartSegmentOffset + EndSegmentOffset);
			AgeUScale = NumSegments / AvailableSegments;
			AgeUOffset = -((StartSegmentOffset / NumSegments) * AgeUScale);
		}
		else
		{
			float FirstAge = SortKeyData[RibbonIndices[StartIndex]];
			float LastAge = SortKeyData[RibbonIndices[EndIndex]];

			AgeUScale = LastAge - FirstAge;
			AgeUOffset = FirstAge;
		}

		OutUScale = AgeUScale * InUScale;
		OutUOffset = (AgeUOffset * InUScale) + InUOffset;
	}
	else
	{
		OutUScale = InUScale;
		OutUOffset = InUOffset;
	}
}

FNiagaraDynamicDataBase* NiagaraRendererRibbons::GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenRibbonVertexData);

	SimpleTimer VertexDataTimer;
	if (!bEnabled)
	{
		return nullptr;
	}
	FNiagaraDynamicDataRibbon* DynamicData = new FNiagaraDynamicDataRibbon;
	TArray<int32>& SegmentData = DynamicData->SegmentData;
	float TotalSegmentLength = 0;
	float TotalSegmentAngle = 0;
	float TotalTwistAngle = 0;
	float TotalWidth = 0;
	int NumTotalSamples = 0;

	FNiagaraDataSetAccessor<FVector> PosData(Data, Properties->PositionBinding.DataSetVariable);

	bool bSortKeyIsAge = false;
	FNiagaraDataSetAccessor<float> SortKeyData(Data, Properties->RibbonLinkOrderBinding.DataSetVariable);
	if (SortKeyData.IsValid() == false)
	{
		SortKeyData = FNiagaraDataSetAccessor<float>(Data, Properties->NormalizedAgeBinding.DataSetVariable);
		bSortKeyIsAge = true;
	}

	//Bail if we don't have the required attributes to render this emitter.
	if (Data.GetNumInstances() < 2 || !PosData.IsValid() || !SortKeyData.IsValid())
	{
		return DynamicData;
	}

	if (PositionDataOffset == INDEX_NONE || LastSyncedId != Properties->SyncId)
	{
		// required attributes
		int32 IntDummy;
		Data.GetVariableComponentOffsets(Properties->PositionBinding.DataSetVariable, PositionDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->VelocityBinding.DataSetVariable, VelocityDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->ColorBinding.DataSetVariable, ColorDataOffset, IntDummy);

		// optional attributes
		Data.GetVariableComponentOffsets(Properties->RibbonWidthBinding.DataSetVariable, WidthDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->RibbonTwistBinding.DataSetVariable, TwistDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->RibbonFacingBinding.DataSetVariable, FacingDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->NormalizedAgeBinding.DataSetVariable, NormalizedAgeDataOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->MaterialRandomBinding.DataSetVariable, MaterialRandomDataOffset, IntDummy);

		Data.GetVariableComponentOffsets(Properties->DynamicMaterialBinding.DataSetVariable, MaterialParamOffset, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial1Binding.DataSetVariable, MaterialParamOffset1, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial2Binding.DataSetVariable, MaterialParamOffset2, IntDummy);
		Data.GetVariableComponentOffsets(Properties->DynamicMaterial3Binding.DataSetVariable, MaterialParamOffset3, IntDummy);

		LastSyncedId = Properties->SyncId;
	}

	DynamicData->DataSet = &Data;

	////////
	FNiagaraDataSetAccessor<float> SizeData(Data, Properties->RibbonWidthBinding.DataSetVariable);
	FNiagaraDataSetAccessor<float> TwistData(Data, Properties->RibbonTwistBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector> AlignData(Data, Properties->RibbonFacingBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParamData(Data, Properties->DynamicMaterialBinding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParam1Data(Data, Properties->DynamicMaterial1Binding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParam2Data(Data, Properties->DynamicMaterial2Binding.DataSetVariable);
	FNiagaraDataSetAccessor<FVector4> MaterialParam3Data(Data, Properties->DynamicMaterial3Binding.DataSetVariable);

	FNiagaraDataSetAccessor<int32> RibbonIdData;
	FNiagaraDataSetAccessor<FNiagaraID> RibbonFullIDData;
	if (Properties->RibbonIdBinding.DataSetVariable.GetType() == FNiagaraTypeDefinition::GetIDDef())
	{
		RibbonFullIDData.Create(&Data, Properties->RibbonIdBinding.DataSetVariable);
		RibbonFullIDData.InitForAccess(true);
	}
	else
	{
		RibbonIdData.Create(&Data, Properties->RibbonIdBinding.DataSetVariable);
		RibbonIdData.InitForAccess(true);
	}

	bool bFullIDs = RibbonFullIDData.IsValid();
	bool bSimpleIDs = !bFullIDs && RibbonIdData.IsValid();
	bool bMultiRibbons = bFullIDs || bSimpleIDs;
	bool bHasTwist = TwistData.IsValid() && SizeData.IsValid();

	auto AddRibbonVerts = [&](TArray<int32>& RibbonIndices, uint32 RibbonIndex)
	{
		int32 StartIndex = DynamicData->SortedIndices.Num();
		int32 NumIndices = RibbonIndices.Num();

		float TotalDistance = 0.0f;

		const FVector FirstPos = PosData[RibbonIndices[0]];
		FVector CurrPos = FirstPos;
		FVector LastToCurrVec = FVector::ZeroVector;
		float LastToCurrSize = 0;
		float LastTwist = 0;
		float LastWidth = 0;

		// Find the first position with enough distance.
		int32 CurrentIndex = 1;
		while (CurrentIndex < RibbonIndices.Num())
		{
			const int32 CurrentDataIndex = RibbonIndices[CurrentIndex];
			CurrPos = PosData[CurrentDataIndex];
			LastToCurrVec = CurrPos - FirstPos;
			LastToCurrSize = LastToCurrVec.Size();
			if (bHasTwist)
			{
				LastTwist = TwistData[CurrentDataIndex];
				LastWidth = SizeData[CurrentDataIndex];
			}

			// Find the first segment, or unique segment
			if (LastToCurrSize > GNiagaraRibbonMinSegmentLength)
			{
				// Normalize LastToCurrVec
				LastToCurrVec *= 1.f / LastToCurrSize;

				// Add the first point. Tangent follows first segment.
				DynamicData->SortedIndices.Add(RibbonIndices[0]);
				DynamicData->TangentAndDistances.Add(FVector4(LastToCurrVec.X, LastToCurrVec.Y, LastToCurrVec.Z, 0));
				DynamicData->MultiRibbonIndices.Add(RibbonIndex);

				break;
			}
			else
			{
				LastToCurrSize = 0; // Ensure that the segment gets ignored if too small
				++CurrentIndex;
			}
		}

		// Now iterate on all other points, to proceed each particle connected to 2 segments.
		int32 NextIndex = CurrentIndex + 1;
		while (NextIndex < RibbonIndices.Num())
		{
			const int32 NextDataIndex = RibbonIndices[NextIndex];
			const FVector NextPos = PosData[NextDataIndex];
			FVector CurrToNextVec = NextPos - CurrPos;
			const float CurrToNextSize = CurrToNextVec.Size();
			
			float NextTwist = 0;
			float NextWidth = 0;
			if (bHasTwist)
			{
				NextTwist = TwistData[NextDataIndex];
				NextWidth = SizeData[NextDataIndex];
			}

			// It the next is far enough, or the last element
			if (CurrToNextSize > GNiagaraRibbonMinSegmentLength || NextIndex == RibbonIndices.Num() - 1)
			{
				// Normalize CurrToNextVec
				CurrToNextVec *= 1.f / FMath::Max(GNiagaraRibbonMinSegmentLength, CurrToNextSize);
				const FVector Tangent = (LastToCurrVec + CurrToNextVec).GetSafeNormal();

				// Update the distance for CurrentIndex.
				TotalDistance += LastToCurrSize;

				// Add the current point, which tangent is computed from neighbors
				DynamicData->SortedIndices.Add(RibbonIndices[CurrentIndex]);
				DynamicData->TangentAndDistances.Add(FVector4(Tangent.X, Tangent.Y, Tangent.Z, TotalDistance));
				DynamicData->MultiRibbonIndices.Add(RibbonIndex);

				// Assumed equal to dot(Tangent, CurrToNextVec)
				TotalSegmentLength += CurrToNextSize;
				TotalSegmentAngle += AcosFast(FVector::DotProduct(LastToCurrVec, CurrToNextVec));
				TotalTwistAngle += FMath::Abs(NextTwist - LastTwist);
				TotalWidth += LastWidth;
				++NumTotalSamples;

				// Move to next segment.
				CurrentIndex = NextIndex;
				CurrPos = NextPos;
				LastToCurrVec = CurrToNextVec;
				LastToCurrSize = CurrToNextSize;
				LastTwist = NextTwist;
				LastWidth = NextWidth;
			}

			// Try next if there is one.
			++NextIndex;
		}

		// Close the last point and segment if there was at least 2.
		if (LastToCurrSize > 0)
		{
			// Update the distance for CurrentIndex.
			TotalDistance += LastToCurrSize;

			// Add the last point, which tangent follows the last segment.
			DynamicData->SortedIndices.Add(RibbonIndices[CurrentIndex]);
			DynamicData->TangentAndDistances.Add(FVector4(LastToCurrVec.X, LastToCurrVec.Y, LastToCurrVec.Z, TotalDistance));
			DynamicData->MultiRibbonIndices.Add(RibbonIndex);
		}

		const int32 EndIndex = DynamicData->SortedIndices.Num() - 1;
		const int32 NumSegments = EndIndex - StartIndex;

		if (NumSegments > 0)
		{
			// Update the tangents for the first and last vertex, apply a reflect vector logic so that the initial and final curvature is continuous.
			if (NumSegments > 1)
			{
				FVector& FirstTangent = reinterpret_cast<FVector&>(DynamicData->TangentAndDistances[StartIndex]);
				FVector& NextToFirstTangent = reinterpret_cast<FVector&>(DynamicData->TangentAndDistances[StartIndex + 1]);
				FirstTangent = (2.f * FVector::DotProduct(FirstTangent, NextToFirstTangent)) * FirstTangent - NextToFirstTangent;

				FVector& LastTangent = reinterpret_cast<FVector&>(DynamicData->TangentAndDistances[EndIndex]);
				FVector& PrevToLastTangent = reinterpret_cast<FVector&>(DynamicData->TangentAndDistances[EndIndex - 1]);
				LastTangent = (2.f * FVector::DotProduct(LastTangent, PrevToLastTangent)) * LastTangent - PrevToLastTangent;
			}

			// Add segment data
			for (int32 SegmentIndex = StartIndex; SegmentIndex < EndIndex; ++SegmentIndex)
			{
				SegmentData.Add(SegmentIndex);
			}

			float U0Offset, U0Scale, U1Offset, U1Scale;

			CalculateUVScaleAndOffsets(SortKeyData, DynamicData->SortedIndices, bSortKeyIsAge, StartIndex, DynamicData->SortedIndices.Num() - 1, NumSegments,
				Properties->UV0TilingDistance,	Properties->UV0Scale.X, Properties->UV0Offset.X, Properties->UV0AgeOffsetMode, U0Scale, U0Offset);
			CalculateUVScaleAndOffsets(SortKeyData, DynamicData->SortedIndices, bSortKeyIsAge, StartIndex, DynamicData->SortedIndices.Num() - 1, NumSegments,
				Properties->UV1TilingDistance, Properties->UV1Scale.X, Properties->UV1Offset.X, Properties->UV1AgeOffsetMode, U1Scale, U1Offset);

			DynamicData->PackPerRibbonData(U0Scale, U0Offset, U1Scale, U1Offset, NumSegments, StartIndex);
		}
	};

	// store the start and end positions for the ribbon for draw distance flipping 
	DynamicData->StartPos = PosData[0];
	DynamicData->EndPos = PosData[Data.GetNumInstances() - 1];
	
	//TODO: Move sorting to share code with sprite and mesh sorting and support the custom sorting key.
	int32 TotalIndices = Data.GetNumInstances();

	if (!bMultiRibbons)
	{
		TArray<int32> SortedIndices;
		for (int32 i = 0; i < TotalIndices; ++i)
		{
			SortedIndices.Add(i);
		}

		SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });

		AddRibbonVerts(SortedIndices, 0);
	}
	else
	{
		if (bFullIDs)
		{
			TMap<FNiagaraID, TArray<int32>> MultiRibbonSortedIndices;
			
			for (int32 i = 0; i < TotalIndices; ++i)
			{
				TArray<int32>& Indices = MultiRibbonSortedIndices.FindOrAdd(RibbonFullIDData[i]);
				Indices.Add(i);
			}

			uint32 RibbonIndex = 0;
			for (TPair<FNiagaraID, TArray<int32>>& Pair : MultiRibbonSortedIndices)
			{
				TArray<int32>& SortedIndices = Pair.Value;
				SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });
				AddRibbonVerts(SortedIndices, RibbonIndex);
				
				RibbonIndex++;
			};
		}
		else
		{
			//TODO: Remove simple ID path
			check(bSimpleIDs);

			TMap<int32, TArray<int32>> MultiRibbonSortedIndices;

			for (int32 i = 0; i < TotalIndices; ++i)
			{
				TArray<int32>& Indices = MultiRibbonSortedIndices.FindOrAdd(RibbonIdData[i]);
				Indices.Add(i);
			}

			uint32 RibbonIndex = 0;
			for (TPair<int32, TArray<int32>>& Pair : MultiRibbonSortedIndices)
			{
				TArray<int32>& SortedIndices = Pair.Value;
				SortedIndices.Sort([&SortKeyData](const int32& A, const int32& B) {	return (SortKeyData[A] < SortKeyData[B]); });
				AddRibbonVerts(SortedIndices, RibbonIndex);
				RibbonIndex++;
			};
		}
	}

	if (NumTotalSamples > 0)
	{
		const float OneOverSampleCount = 1.f / (float)NumTotalSamples;
		const float AverageSegmentAngle = TotalSegmentAngle * OneOverSampleCount;
		const float AverageSegmentCurvature = TotalSegmentLength * OneOverSampleCount * .5 / (FMath::Max(SMALL_NUMBER, FMath::Abs(FMath::Sin(AverageSegmentAngle))));

		// Accumulate the max so that tessellation is ever increasing and stabilizes over time.
		// This is to avoid the spawning and unspawning of ribbons to constantly generate different tessellation factors.
		
		TessellationAngle = FMath::Lerp<float>(AverageSegmentAngle, FMath::Max(TessellationAngle, AverageSegmentAngle), GNiagaraRibbonMaxAveraging);
		TessellationCurvature = FMath::Lerp<float>(AverageSegmentCurvature, FMath::Max(TessellationCurvature, AverageSegmentCurvature), GNiagaraRibbonMaxAveraging);

		if (bHasTwist)
		{
			const float AverageTwistAngle = TotalTwistAngle * OneOverSampleCount;
			const float AverageTwistCurvature = TotalWidth * OneOverSampleCount;
			
			TessellationTwistAngle = FMath::Lerp<float>(AverageTwistAngle, FMath::Max(TessellationTwistAngle, AverageTwistAngle), GNiagaraRibbonMaxAveraging);
			TessellationTwistCurvature = FMath::Lerp<float>(AverageTwistCurvature, FMath::Max(TessellationTwistCurvature, AverageTwistCurvature), GNiagaraRibbonMaxAveraging);
		}
	}
	else // Reset the metrics when the ribbons are reset.
	{
		TessellationAngle = 0;
		TessellationCurvature = 0;
		TessellationTwistAngle = 0;
		TessellationTwistCurvature = 0;
	}
	 
	if (Data.CurrData().GetNumInstances() > 0)
	{
		//TODO: This buffer is far fatter than needed. Just pull out the data needed for rendering.
		Data.CurrData().CopyTo(DynamicData->RTParticleData);

		DynamicData->DataSet = &Data;
	}

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();

	return DynamicData;
}
