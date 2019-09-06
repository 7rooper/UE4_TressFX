#include "TressFXSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "TressFXAsset.h"
#include "TressFXBoneSkinningAsset.h"
#include "RenderResource.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "ShaderParameterUtils.h"
#include "TressFXSimulation.h"
#include "TressFXVertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "RayTracingInstance.h"

#ifdef TRESSFX_STANDALONE_PLUGIN
	#define TRESSFX_SHOW_FLAG StaticMeshes
#else
	#include "SkeletalRenderPublic.h"
	#define TRESSFX_SHOW_FLAG TressFX
#endif

class FTressFXCopyMorphDeltasCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXCopyMorphDeltasCS);

	FTressFXCopyMorphDeltasCS()
	{}

	FTressFXCopyMorphDeltasCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MorphVertexCount.Bind(Initializer.ParameterMap, TEXT("MorphVertexCount"));
		MorphIndexBuffer.Bind(Initializer.ParameterMap, TEXT("MorphIndexBuffer"));
		MorphVertexBuffer.Bind(Initializer.ParameterMap, TEXT("MorphVertexBuffer"));
		MorphPositionDeltaBuffer.Bind(Initializer.ParameterMap, TEXT("MorphPositionDeltaBuffer"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MorphVertexCount << MorphIndexBuffer << MorphVertexBuffer << MorphPositionDeltaBuffer;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FShaderParameter MorphVertexCount;
	FShaderResourceParameter MorphIndexBuffer;
	FShaderResourceParameter MorphVertexBuffer;
	FShaderResourceParameter MorphPositionDeltaBuffer;
};

IMPLEMENT_GLOBAL_SHADER(FTressFXCopyMorphDeltasCS, "/Plugin/TressFX/Private/TressFXSimulation.usf", "CopyMorphDeltas", SF_Compute);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTressFXShadeParametersUniformBuffer, "TressFXShadeParameters");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTressFXSimParametersUniformBuffer, "TressFXSimParameters");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTressFXBoneSkinningUniformBuffer, "TressFXBoneSkinningParameters");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTressFXSDFUniformBuffer, "ConstBuffer_SDF");

FTressFXShadeParametersUniformBuffer::FTressFXShadeParametersUniformBuffer()
{
	FMemory::Memzero(*this);
	g_NumVerticesPerStrand = 4;
}

FTressFXSimParametersUniformBuffer::FTressFXSimParametersUniformBuffer()
{
	FMemory::Memzero(*this);
}

FTressFXBoneSkinningUniformBuffer::FTressFXBoneSkinningUniformBuffer()
{
	FMemory::Memzero(*this);
}

FTressFXSDFUniformBuffer::FTressFXSDFUniformBuffer()
{
	FMemory::Memzero(*this);
}

FTressFXSceneProxy::FTressFXSceneProxy(UPrimitiveComponent * InComponent, FName ResourceName, FTressFXHairObject* InHairObject) 
	: ITressFXSceneProxy(InComponent, ResourceName)
	, VertexFactory(GetScene().GetFeatureLevel())
{
	bIsTressFX = true;
	LODCullingFactor = 1.0f;
	LODScreenSizeThreshold = 2.5f;
	MinLODRate = 0.1f;
	TFXComponent = Cast<UTressFXComponent>(InComponent);
	TressFXHairObject = InHairObject;
	bCastShadowAsTwoSided = true;
	Material = TFXComponent->HairMaterial;


#ifdef TRESSFX_STANDALONE_PLUGIN
	if (!Material || !Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh))
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
#else
	if (!Material || !Material->CheckMaterialUsage_Concurrent(MATUSAGE_TressFX))
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
#endif

	MaterialRelevance = Material->GetRelevance(InComponent->GetScene()->GetFeatureLevel());
	BeginInitResource(&VertexFactory);
	InstanceRenderData = nullptr;
	TressFXHairObject = nullptr;

}

FTressFXSceneProxy::~FTressFXSceneProxy()
{
	TressFXHairObject = nullptr;
	VertexFactory.ReleaseResource();
#if TRESSFX_RAYTRACING && RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		RayTracingGeometry.ReleaseResource();
		RayTracingDynamicVertexBuffer.Release();
	}
#endif

}

uint32 FTressFXSceneProxy::GetMemoryFootprint(void) const
{
	return 0; //JAKETODO
}

FPrimitiveViewRelevance FTressFXSceneProxy::GetViewRelevance(const FSceneView * View) const
{
	FPrimitiveViewRelevance ViewRel;
	MaterialRelevance.SetPrimitiveViewRelevance(ViewRel);
	ViewRel.bDrawRelevance = IsShown(View);
	ViewRel.bShadowRelevance = View->Family->EngineShowFlags.TRESSFX_SHOW_FLAG;
	ViewRel.bDynamicRelevance = true;
	ViewRel.bRenderInMainPass = View->Family->EngineShowFlags.TRESSFX_SHOW_FLAG;
#ifndef TRESSFX_STANDALONE_PLUGIN
	if (Material &&Material->GetMaterial())
	{
		const ETressFXRenderMode RenderMode = Material->GetMaterial()->TressFXRenderMode;
		ViewRel.bTressFXTranslucent = View->Family->EngineShowFlags.TressFX && RenderMode == ETressFXRenderMode::TressFXRender_Translucent;
		ViewRel.bTressFXOpaque = ViewRel.bTressFXTranslucent ? false : true;
	}
	else 
	{
		//assume opaque
		ViewRel.bTressFXOpaque = View->Family->EngineShowFlags.TressFX;
		ViewRel.bTressFXTranslucent = false;
	}
	check(!(ViewRel.bTressFXOpaque && ViewRel.bTressFXTranslucent))
#endif

	return ViewRel;
}

void UploadGPUData(FRHIStructuredBuffer* Buffer, int32 ElementSize, int32 ElementCount, void* InData)
{
	void* LockData = RHILockStructuredBuffer(Buffer, 0, ElementSize*ElementCount, RLM_WriteOnly);
	FMemory::Memcpy(LockData, InData, ElementSize*ElementCount);
	RHIUnlockStructuredBuffer(Buffer);
}

void FTressFXSceneProxy::CreateRenderThreadResources()
{
	SimulationFrame = 0;

#if TRESSFX_RAYTRACING && RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		ENQUEUE_RENDER_COMMAND(InitTressFXRayTracingGeometry)(
			[this](FRHICommandListImmediate& RHICmdList)
		{
			const int32 NumPosVerts = InstanceRenderData->PosTanCollection.PositionsData.Num();
			RayTracingDynamicVertexBuffer.Initialize( sizeof(FVector4), NumPosVerts, PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource, TEXT("TressFXRayTracingDynamicVertexBuffer"));
			FRayTracingGeometryInitializer Initializer;
			Initializer.PositionVertexBuffer = nullptr;
			Initializer.IndexBuffer = nullptr;
			Initializer.BaseVertexIndex = 0;
			Initializer.VertexBufferStride = sizeof(FVector4);
			Initializer.VertexBufferByteOffset = 0;
			Initializer.TotalPrimitiveCount = 0;
			Initializer.VertexBufferElementType = VET_Float4;
			Initializer.GeometryType = RTGT_Triangles;
			Initializer.bFastBuild = true;
			Initializer.bAllowUpdate = false;
			RayTracingGeometry.SetInitializer(Initializer);
			RayTracingGeometry.InitResource();
		});
	}
#endif
}

void FTressFXSceneProxy::OnTransformChanged()
{
	//JAKETODO ?
}

SIZE_T FTressFXSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FTressFXSceneProxy::UpdateMorphIndices_RenderThread(const TArray<int32>& MorphIndices)
{
	if (MorphIndexBuffer.NumBytes != MorphIndices.Num() * sizeof(int32))
	{
		MorphIndexBuffer.Initialize(sizeof(int32), MorphIndices.Num(), EPixelFormat::PF_R32_SINT);
	}

	if (MorphIndexBuffer.NumBytes <= 0)
	{
		return;
	}

	void* LockedMorphIndices = RHILockVertexBuffer(MorphIndexBuffer.Buffer, 0, MorphIndexBuffer.NumBytes, EResourceLockMode::RLM_WriteOnly);

	FMemory::BigBlockMemcpy(LockedMorphIndices, MorphIndices.GetData(), MorphIndexBuffer.NumBytes);

	RHIUnlockVertexBuffer(MorphIndexBuffer.Buffer);
}

void FTressFXSceneProxy::UpdateDynamicData_RenderThread(const FDynamicRenderData& DynamicData)
{
	this->TressFXHairObject = DynamicData.HairObject;
	this->InstanceRenderData = DynamicData.InstanceRenderData;
	this->SDFMeshResources = DynamicData.SDFMeshResources;
	this->bEnableMorphTargets = DynamicData.bEnableMorphTargets;
	LODCullingFactor = DynamicData.LODCullingFactor;
	LODScreenSizeThreshold = DynamicData.LODScreenSizeThreshold;
	MinLODRate = DynamicData.MinLODRate;

#ifdef TRESSFX_STANDALONE_PLUGIN
	this->MorphVertexBuffer = nullptr;
	this->MorphVertexUpdateFrameNumber = 0;
#else
	// Morph data. It's too early to update morph data here. It's not ready yet. Delay it just before simulation.
	if (this->bEnableMorphTargets && DynamicData.ParentSkin != nullptr && DynamicData.ParentSkin->GetMorphVertexBuffer().bHasBeenUpdated)
	{
		DynamicData.ParentSkin->GetMorphVertexBuffer().RequireSRV();
		this->MorphVertexBuffer = DynamicData.ParentSkin->GetMorphVertexBuffer().GetSRV();
	}
	else
	{
		this->MorphVertexBuffer = nullptr;
	}
	this->MorphVertexUpdateFrameNumber = GFrameNumberRenderThread;
#endif

	this->MorphVertexUpdateFrameNumber = GFrameNumberRenderThread;

	//create new sim params, update with new settings
	FTressFXSimParameters SimulationParams(TressFXHairObject->NumVerticePerStrand, DynamicData.NumFollowStrandsPerGuide);
	SimulationParams.UpdateSimulationParameters(DynamicData.TressFXSimulationSettings, GFrameNumberRenderThread, DynamicData.HairObject);
	FTressFXSimParametersUniformBuffer SimParamsUniformBuffer = SimulationParams.GetBuffer();
	SimParamsUniformBuffer.g_centerAndRadius0 = DynamicData.CollisionCapsuleCenterAndRadius0;
	SimParamsUniformBuffer.g_centerAndRadius1 = DynamicData.CollisionCapsuleCenterAndRadius1;
	SimParamsUniformBuffer.g_numCollisionCapsules = DynamicData.NumCollisionCapsules;
	SimParamsUniformBuffer.BoneSkinningMatrix = DynamicData.BoneTransforms;

	//Shade Params
	FTressFXShadeParametersUniformBuffer ShadeParametersUniformBuffer;

	ShadeParametersUniformBuffer.g_FiberRadius = DynamicData.TressFXShadeSettings.FiberRadius;
	ShadeParametersUniformBuffer.g_FiberSpacing = DynamicData.TressFXShadeSettings.FiberSpacing;
	ShadeParametersUniformBuffer.g_NumVerticesPerStrand = TressFXHairObject->NumVerticePerStrand;
	ShadeParametersUniformBuffer.g_ratio = DynamicData.TressFXShadeSettings.HairThickness;
	ShadeParametersUniformBuffer.g_RootTangentBlending = DynamicData.TressFXShadeSettings.RootTangentBlending;

	const FTressFXSpecularSettings SpecularSettings = DynamicData.TressFXShadeSettings.Specular;
	ShadeParametersUniformBuffer.TressFXSettings1 = FVector4
	(
		SpecularSettings.SpecularPrimaryScale,
		SpecularSettings.SpecularPrimaryExponent,
		SpecularSettings.SpecularSecondaryScale,
		SpecularSettings.SpecularSecondaryExponent
	);
	ShadeParametersUniformBuffer.TressFXSettings2 = FVector4
	(
		DynamicData.TressFXShadeSettings.DiffuseBlend,
		DynamicData.TressFXShadeSettings.SelfShadowStrength,
		0,
		0
	);

	ShadeParametersUniformBuffer.TressFXSettings3 = FVector4
	(
		SpecularSettings.GlintStrength,
		SpecularSettings.GlintSize,
		SpecularSettings.GlintPowerExponent,
		0
	);

	ShadeParametersUniformBuffer.TressFXSpecularColor = FVector4(SpecularSettings.SpecularColor);

	InstanceRenderData->SimParametersUniformBuffer = TUniformBufferRef<FTressFXSimParametersUniformBuffer>::CreateUniformBufferImmediate(SimParamsUniformBuffer, UniformBuffer_SingleFrame);
	InstanceRenderData->ShadeParametersUniformBuffer = TUniformBufferRef<FTressFXShadeParametersUniformBuffer>::CreateUniformBufferImmediate(ShadeParametersUniformBuffer, UniformBuffer_SingleFrame);
	CollisionType = DynamicData.CollisionType;

	if (CVarTFXSDFDisable.GetValueOnAnyThread() == 0 && CollisionType == ETressFXCollisionType::TFXCollsion_SDF && this->SDFMeshResources)
	{
		FTressFXBoneSkinningUniformBuffer BoneSkinBuffer;
		BoneSkinBuffer.BoneSkinningMatrix = DynamicData.BoneTransforms; 
		BoneSkinBuffer.NumMeshVertices = DynamicData.SDFMeshResources->MeshData.Vertices.Num();
		 InstanceRenderData->BoneSkinningUniformBuffer = TUniformBufferRef<FTressFXBoneSkinningUniformBuffer>::CreateUniformBufferImmediate(BoneSkinBuffer, UniformBuffer_SingleFrame);

		//SDF Uniform Buffer
		FTressFXSDFUniformBuffer SDFUniformBuffer = this->SDFMeshResources->GetConstantBuffer(TressFXHairObject);
		InstanceRenderData->SDFUniformBuffer = TUniformBufferRef<FTressFXSDFUniformBuffer>::CreateUniformBufferImmediate(SDFUniformBuffer, UniformBuffer_SingleFrame);
	}

	Material = DynamicData.HairMaterial;

	if (!bInitialized)
	{
		bInitialized = true;
	}
}

void FTressFXSceneProxy::CopyMorphs(FRHICommandList& RHICmdList)
{
	// Pass morph delta
	if (GFrameNumberRenderThread > MorphVertexUpdateFrameNumber)
	{
		return;
	}

	//Morph vertex buffer is from the parent mesh, and retrieved during UpdateDynamicData_RenderThread
	if (bEnableMorphTargets && MorphVertexBuffer && MorphIndexBuffer.NumBytes > 0)
	{
		// Create buffers
		const int32 VertexCount = MorphIndexBuffer.NumBytes / sizeof(int32);

		if (MorphPositionDeltaBuffer.NumBytes != VertexCount * sizeof(FVector))
		{
			MorphPositionDeltaBuffer.Initialize(sizeof(FVector), VertexCount);
		}

		{
			SCOPED_DRAW_EVENTF(RHICmdList, TressFXCopyMorphDeltas, TEXT("TressFXCopyMorphDeltas %s"), *TFXComponent->Asset->GetName());
			// Copy position delta
			TShaderMapRef<FTressFXCopyMorphDeltasCS> CopyMorphDeltasCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(CopyMorphDeltasCS->GetComputeShader());

			SetShaderValue(RHICmdList, CopyMorphDeltasCS->GetComputeShader(), CopyMorphDeltasCS->MorphVertexCount, VertexCount);
			SetSRVParameter(RHICmdList, CopyMorphDeltasCS->GetComputeShader(), CopyMorphDeltasCS->MorphIndexBuffer, MorphIndexBuffer.SRV);
			SetSRVParameter(RHICmdList, CopyMorphDeltasCS->GetComputeShader(), CopyMorphDeltasCS->MorphVertexBuffer, MorphVertexBuffer);
			SetUAVParameter(RHICmdList, CopyMorphDeltasCS->GetComputeShader(), CopyMorphDeltasCS->MorphPositionDeltaBuffer, MorphPositionDeltaBuffer.UAV);
			//JAKETODO: will this work on large meshes?
			RHICmdList.DispatchComputeShader(VertexCount / 256 + 1, 1, 1);

			SetUAVParameter(RHICmdList, CopyMorphDeltasCS->GetComputeShader(), CopyMorphDeltasCS->MorphPositionDeltaBuffer, nullptr);
			// In editor, it would be invalid sometimes. So set it to null and wait for update. 
			MorphVertexBuffer = nullptr;
		}
	}
	else
	{
		MorphPositionDeltaBuffer.Release();
	}
}

bool FTressFXSceneProxy::WantsVelocityDraw()
{
#ifdef TRESSFX_STANDALONE_PLUGIN
	return Material && Material->GetMaterial();
#else
	return Material && Material->GetMaterial() ? Material->GetMaterial()->bTressFXRenderVelocity : false;
#endif
}

float GetLodRate(float MinLODRate, float LODCullingFactor, float LODScreenSizeThreshold, const FBoxSphereBounds ProxyBounds, const FSceneView * View)
{
	float LodRate = 1.0f;
	//the transitions still arent as smooth as i would like
	if (!FMath::IsNearlyZero(LODCullingFactor) && !FMath::IsNearlyEqual(MinLODRate, 1.0f))
	{
		const float ScreenRadiusSquared = ComputeBoundsScreenRadiusSquared(ProxyBounds.Origin, ProxyBounds.SphereRadius, *View);
		if (ScreenRadiusSquared < LODScreenSizeThreshold)
		{
			if (ScreenRadiusSquared > 1.0f)
			{
				LodRate = FMath::Clamp(ScreenRadiusSquared / FMath::Max(LODCullingFactor, 1.0f), MinLODRate, 1.0f);
			}
			else
			{
				LodRate = MinLODRate;
			}
		}
	}
	return LodRate;
}


void FTressFXSceneProxy::CreateMeshBatchForView(
	const FSceneView* View,	
	const FSceneViewFamily& ViewFamily,	
	FTressFXVertexFactoryUserDataWrapper& UserDataWrapper,
	FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer,
	FMeshBatch& OutMeshBatch) const
{
	FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();
	float LodRate = GetLodRate(MinLODRate, LODCullingFactor, LODScreenSizeThreshold, GetBounds(), View);

	#ifndef TRESSFX_STANDALONE_PLUGIN
		OutMeshBatch.bTressFX = true;
	#endif
	#if TRESSFX_RAYTRACING && RHI_RAYTRACING
		OutMeshBatch.CastRayTracedShadow = CastsDynamicShadow();
	#endif
		FMeshBatchElement& BatchElement = OutMeshBatch.Elements[0];
		BatchElement.IndexBuffer = &TressFXHairObject->IndexBuffer;
		//Mesh.bWireframe = bWireframe;
		OutMeshBatch.VertexFactory = &VertexFactory;
		OutMeshBatch.MaterialRenderProxy = MaterialProxy;

		UserDataWrapper.Data.TressFXHairObject = TressFXHairObject;
		UserDataWrapper.Data.InstanceRenderData = InstanceRenderData;
		BatchElement.VertexFactoryUserData = &UserDataWrapper.Data;
		DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, false, false);
		BatchElement.PrimitiveUniformBuffer = DynamicPrimitiveUniformBuffer.UniformBuffer.GetUniformBufferRHI();

		BatchElement.FirstIndex = 0;
	#ifndef TRESSFX_STANDALONE_PLUGIN
		BatchElement.NumIndices = TressFXHairObject->mtotalIndices;
	#endif
		BatchElement.NumPrimitives = (uint32)((TressFXHairObject->mtotalIndices / 3) * LodRate);
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = InstanceRenderData->PosTanCollection.PositionsData.Num() - 1;
		OutMeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		OutMeshBatch.Type = PT_TriangleList;
		OutMeshBatch.DepthPriorityGroup = SDPG_World;
		OutMeshBatch.bCanApplyViewModeOverrides = false;
}

void FTressFXSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView *>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{

	if (!TressFXHairObject || !TressFXHairObject->IndexBuffer.IsInitialized())
	{
		return;
	}

	FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];
		FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FTressFXVertexFactoryUserDataWrapper& UserDataWrapper = Collector.AllocateOneFrameResource<FTressFXVertexFactoryUserDataWrapper>();
				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				CreateMeshBatchForView(View, ViewFamily, UserDataWrapper, DynamicPrimitiveUniformBuffer, Mesh);
				Collector.AddMesh(ViewIndex, Mesh);
			}

		}
	}
}

FRHIUniformBuffer* FTressFXSceneProxy::GetHairObjectShaderUniformBufferParam()
{
	return InstanceRenderData->ShadeParametersUniformBuffer;
}


void FTressFXSimParameters::UpdateSimulationParameters(const FTressFXSimulationSettings& InSettings, uint32 Frame, FTressFXHairObject* HairObject)
{
	SetVelocityShockPropogation(InSettings.VSPCoefficient);
	SetVSPAccelThreshold(InSettings.VSPAccelerationThreshold);
	SetDamping(InSettings.Damping);
	SetLocalStiffness(InSettings.LocalConstraintStiffness);
	SetGlobalStiffness(InSettings.GlobalConstraintStiffness);
	SetGlobalRange(InSettings.GlobalShapeRange);
	SetGravity(InSettings.GravityMagnitude);
	SetVerticesPerStrand(NumVertsPerStrand);
	SetFollowHairsPerGuideHair(NumFollowHairsPerGuideHair);
	SetTipSeperation(InSettings.TipSeparation);
	SetTotalHairs(HairObject->NumTotalStrands);

	SetTimeStep(1.f / 60.f);

	//Note: this sets the z component of g_SimInts/m_SimInts but that flag seems to not be used anymore
	// eventually probably remove this
	SetCollision(false);

	// Right now, we do all local contraint iterations on the CPU.
	// (loop and dispatch compute shader for num CPULocalShapeIterations) see CVarTFXSimPass3 in TressFXSimulation.cpp
	// It's actually a bit faster to
	if (NumVertsPerStrand >= TRESSFX_MIN_VERTS_PER_STRAND_FOR_GPU_ITERATION)
	{
		SetLocalIterations(InSettings.LocalContraintsIterations);
		CPULocalShapeIterations = 1;
	}
	else
	{
		SetLocalIterations(1);
		CPULocalShapeIterations = InSettings.LocalContraintsIterations;
	}

	SetLengthIterations(InSettings.LengthConstraintsIterations);

	// Set wind parameters
	SetWind(InSettings.WindDirection, InSettings.WindMagnitude, Frame);
	//Note: collision capsules are set in UpdateDynamicData_RenderThread
}

void FTressFXSimParameters::SetWind(const FVector& WindDir, const float WindMagnitude, const int32 FrameNumber)
{
	const float Magnitude = WindMagnitude * (FMath::Pow(FMath::Sin(FrameNumber * 0.01f), 2.0f) + 0.5f);

	FVector WindDirNomalized = FVector(WindDir);
	WindDirNomalized.Normalize();
	this->Wind = WindDirNomalized;
	this->Wind *= Magnitude;

	static const FRotator WindRotator = FRotator(0, -180, 0);
	FVector FinalWind = FVector(Wind.X, Wind.Y, Wind.Z);
	FinalWind = WindRotator.RotateVector(FinalWind);
	FinalWind.Z *= -1.f;
	this->Wind = FVector4(FinalWind, this->Wind.W);
}

// NOTE: collision and bones etc need to be set by whatever calls this function!
FTressFXSimParametersUniformBuffer FTressFXSimParameters::GetBuffer()
{
	FTressFXSimParametersUniformBuffer Result;
	Result.g_Counts = m_Counts;
	Result.g_GravTimeTip = m_GravTimeTip;
	Result.g_Shape = m_Shape;
	Result.g_SimInts = m_SimInts;
	Result.g_VSP = m_VSP;
	Result.Wind = Wind;
	return Result;
}

void FTressFXPosTanCollection::InitResources(uint32 InNumOfVerts)
{

	if (InNumOfVerts > 0)
	{
		Positions.Initialize(sizeof(FVector4), InNumOfVerts);
		Tangents.Initialize(sizeof(FVector4), InNumOfVerts);
		PositionsPrev.Initialize(sizeof(FVector4), InNumOfVerts);
		PositionsPrevPrev.Initialize(sizeof(FVector4), InNumOfVerts);
		TempTangents.Initialize(sizeof(FVector4), InNumOfVerts);
	}
	NumOfVerts = InNumOfVerts;
}

void FTressFXPosTanCollection::ReleaseResources()
{
	Positions.Release();
	Tangents.Release();
	PositionsPrev.Release();
	PositionsPrevPrev.Release();
	TempTangents.Release();
}

void FTressFXPosTanCollection::UAVBarrier(FRHICommandList& RHICmdList, FRHIComputeFence* Fence)
{
	FRHIUnorderedAccessView* UAVs[] =
	{
		Positions.UAV,
		PositionsPrev.UAV,
		PositionsPrevPrev.UAV,
		//Tangents.UAV
	};

	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, UAVs, ARRAY_COUNT(UAVs), Fence);
}

void FTressFXPosTanCollection::SetUAVs(FRHICommandList& RHICmdList, FRHIComputeShader* Shader)
{
	RHICmdList.SetUAVParameter(Shader, 0, Positions.UAV);
	RHICmdList.SetUAVParameter(Shader, 1, PositionsPrev.UAV);
	RHICmdList.SetUAVParameter(Shader, 2, PositionsPrevPrev.UAV);
	RHICmdList.SetUAVParameter(Shader, 3, Tangents.UAV);
}

void FTressFXPosTanCollection::UnsetUAVs(FRHICommandList& RHICmdList, FRHIComputeShader* Shader)
{
	RHICmdList.SetUAVParameter(Shader, 0, nullptr);
	RHICmdList.SetUAVParameter(Shader, 1, nullptr);
	RHICmdList.SetUAVParameter(Shader, 2, nullptr);
	RHICmdList.SetUAVParameter(Shader, 3, nullptr);
	RHICmdList.SetUAVParameter(Shader, 4, nullptr);
}


#if TRESSFX_RAYTRACING && RHI_RAYTRACING

void FTressFXSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	//copied and adapted from proceduralmeshcomponent, also see niagararenderersprites
	FMaterialRenderProxy* MaterialProxy = this->Material->GetRenderProxy();

	if (MaterialProxy && RayTracingGeometry.RayTracingGeometryRHI.IsValid())
	{
		FRayTracingInstance RayTracingInstance;
		RayTracingInstance.Geometry = &RayTracingGeometry;
		RayTracingInstance.InstanceTransforms.Add(FMatrix::Identity);

		//see GetDynamicRayTracingInstances in NiagaraRendersprites.cpp	
		FMeshBatch MeshBatch;
		//CreateMeshBatchForView(Context.ReferenceView, Context.ReferenceViewFamily, SceneProxy, DynamicDataSprites, IndirectArgsOffset, MeshBatch, CollectorResources);

	}
	
}

#endif