#include "TressFX/TressFXSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "Engine/TressFXAsset.h"
#include "Engine/TressFXBoneSkinningAsset.h"
#include "RenderResource.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "ShaderParameterUtils.h"
#include "TressFX/TressFXSimulation.h"
#include "TressFX/TressFXVertexFactory.h"
#include "SkeletalRenderGPUSkin.h"
#include "MaterialShared.h"
#include "Materials/Material.h"

class FTressFXCopyMorphDeltasCs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXCopyMorphDeltasCs);

	FTressFXCopyMorphDeltasCs()
	{}

	FTressFXCopyMorphDeltasCs(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MorphVertexCount.Bind(Initializer.ParameterMap, TEXT("MorphVertexCount"));
		MorphIndexBuffer.Bind(Initializer.ParameterMap, TEXT("MorphIndexBuffer"));
		MorphVertexBuffer.Bind(Initializer.ParameterMap, TEXT("MorphVertexBuffer"));
		MorphPositionDeltaBuffer.Bind(Initializer.ParameterMap, TEXT("MorphPositionDeltaBuffer"));
		//MorphNormalDeltaBuffer.Bind(Initializer.ParameterMap, TEXT("MorphNormalDeltaBuffer"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MorphVertexCount << MorphIndexBuffer << MorphVertexBuffer << MorphPositionDeltaBuffer
			//<< MorphNormalDeltaBuffer
			;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == EShaderPlatform::SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FShaderParameter MorphVertexCount;
	FShaderResourceParameter MorphIndexBuffer;
	FShaderResourceParameter MorphVertexBuffer;
	FShaderResourceParameter MorphPositionDeltaBuffer;
	//FShaderResourceParameter MorphNormalDeltaBuffer;
};

IMPLEMENT_GLOBAL_SHADER(FTressFXCopyMorphDeltasCs, "/Engine/Private/TressFXSimulation.usf", "CopyMorphDeltas", SF_Compute);

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

FTressFXSceneProxy::FTressFXSceneProxy(UPrimitiveComponent * InComponent, FName ResourceName, FTressFXHairObject* InHairObject) :
	FPrimitiveSceneProxy(InComponent, ResourceName),
	VertexFactory(GetScene().GetFeatureLevel())
{

	TFXComponent = Cast<UTressFXComponent>(InComponent);
	TressFXHairObject = InHairObject;

	Material = TFXComponent->HairMaterial;


	if (!Material || !Material->CheckMaterialUsage_Concurrent(MATUSAGE_TressFX))
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	MaterialRelevance = Material->GetRelevance(InComponent->GetScene()->GetFeatureLevel());
	BeginInitResource(&VertexFactory);
}

FTressFXSceneProxy::~FTressFXSceneProxy()
{
	TressFXHairObject = nullptr;
	VertexFactory.ReleaseResource();

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
	ViewRel.bShadowRelevance = View->Family->EngineShowFlags.TressFX;;
	ViewRel.bDynamicRelevance = true;
	ViewRel.bRenderInMainPass = View->Family->EngineShowFlags.TressFX;
	ViewRel.bTressFX = View->Family->EngineShowFlags.TressFX;
	return ViewRel;
}

void UploadGPUData(FStructuredBufferRHIParamRef Buffer, int32 ElementSize, int32 ElementCount, void* InData)
{
	void* LockData = RHILockStructuredBuffer(Buffer, 0, ElementSize*ElementCount, RLM_WriteOnly);
	FMemory::Memcpy(LockData, InData, ElementSize*ElementCount);
	RHIUnlockStructuredBuffer(Buffer);
}

void FTressFXSceneProxy::CreateRenderThreadResources()
{
	SimulationFrame = 0;
}

void FTressFXSceneProxy::OnTransformChanged()
{
	//JAKETODO
}

SIZE_T FTressFXSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

static void SetWindCorner(
	FQuat rotFromXAxisToWindDir,
	FVector	rotAxis,
	float angleToWideWindCone,
	float wM,
	FVector4 &outVec
)
{
	static const FVector XAxis(1.0f, 0, 0);
	FQuat rot(rotAxis, angleToWideWindCone);
	FVector  newWindDir = rotFromXAxisToWindDir * rot * XAxis;
	outVec.X = newWindDir.X * wM;
	outVec.Y = newWindDir.Y * wM;
	outVec.Z = newWindDir.Z * wM;
	outVec.W = 0;  // unused
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
	this->SDFMeshResources = DynamicData.SDFMeshResources;
	this->bEnableMorphTargets = DynamicData.bEnableMorphTargets;

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

	//create new sim params, update with new settings
	FTressFXSimParameters SimulationParams(TressFXHairObject->NumVerticePerStrand, DynamicData.NumFollowStrandsPerGuide);
	SimulationParams.UpdateSimulationParameters(DynamicData.TressFXSimulationSettings, GFrameNumberRenderThread, DynamicData.HairObject);
	FTressFXSimParametersUniformBuffer SimParamsUniformBuffer = SimulationParams.GetBuffer();
	SimParamsUniformBuffer.g_centerAndRadius0 = DynamicData.CollisionCapsuleCenterAndRadius0;
	SimParamsUniformBuffer.g_centerAndRadius1 = DynamicData.CollisionCapsuleCenterAndRadius1;
	SimParamsUniformBuffer.g_numCollisionCapsules = DynamicData.NumCollisionCapsules;
	SimParamsUniformBuffer.g_BoneSkinningMatrix = DynamicData.BoneTransforms;

	//Shade Params
	FTressFXShadeParametersUniformBuffer ShadeParametersUniformBuffer;

	ShadeParametersUniformBuffer.g_FiberRadius = DynamicData.TressFXShadeSettings.FiberRadius;
	ShadeParametersUniformBuffer.g_FiberSpacing = DynamicData.TressFXShadeSettings.FiberSpacing;
	ShadeParametersUniformBuffer.g_NumVerticesPerStrand = TressFXHairObject->NumVerticePerStrand;
	ShadeParametersUniformBuffer.g_ratio = DynamicData.TressFXShadeSettings.HairThickness;

	TressFXHairObject->SimParametersUniformBuffer = TUniformBufferRef<FTressFXSimParametersUniformBuffer>::CreateUniformBufferImmediate(SimParamsUniformBuffer, UniformBuffer_SingleFrame);
	TressFXHairObject->ShadeParametersUniformBuffer = TUniformBufferRef<FTressFXShadeParametersUniformBuffer>::CreateUniformBufferImmediate(ShadeParametersUniformBuffer, UniformBuffer_SingleFrame);
	CollisionType = DynamicData.CollisionType;

	if (CVarTFXSDFDisable.GetValueOnAnyThread() == 0 && CollisionType == ETressFXCollisionType::TFXCollsion_SDF && this->SDFMeshResources)
	{
		FTressFXBoneSkinningUniformBuffer BoneSkinBuffer;
		BoneSkinBuffer.g_BoneSkinningMatrix = DynamicData.BoneTransforms; //kind of redundant with above
		BoneSkinBuffer.g_NumMeshVertices = DynamicData.SDFMeshResources->MeshData.Vertices.Num();
		TressFXHairObject->BoneSkinningUniformBuffer = TUniformBufferRef<FTressFXBoneSkinningUniformBuffer>::CreateUniformBufferImmediate(BoneSkinBuffer, UniformBuffer_SingleFrame);

		//SDF Uniform Buffer
		FTressFXSDFUniformBuffer SDFUniformBuffer = this->SDFMeshResources->GetConstantBuffer(TressFXHairObject);
		TressFXHairObject->SDFUniformBuffer = TUniformBufferRef<FTressFXSDFUniformBuffer>::CreateUniformBufferImmediate(SDFUniformBuffer, UniformBuffer_SingleFrame);
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
			//MorphNormalDeltaBuffer.Initialize(sizeof(FVector), VertexCount);
		}

		{
			SCOPED_DRAW_EVENTF(RHICmdList, TressFXCopyMorphDeltas, TEXT("TressFXCopyMorphDeltas %s"), *TFXComponent->Asset->GetName());
			// Copy position and normal delta
			TShaderMapRef<FTressFXCopyMorphDeltasCs> CopyMorphDeltasCs(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(CopyMorphDeltasCs->GetComputeShader());

			SetShaderValue(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphVertexCount, VertexCount);
			SetSRVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphIndexBuffer, MorphIndexBuffer.SRV);
			SetSRVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphVertexBuffer, MorphVertexBuffer);
			SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphPositionDeltaBuffer, MorphPositionDeltaBuffer.UAV);
			//SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphNormalDeltaBuffer, MorphNormalDeltaBuffer.UAV);
			//JAKETODO: will this work on large meshes, do i care?
			RHICmdList.DispatchComputeShader(VertexCount / 256 + 1, 1, 1);

			SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphPositionDeltaBuffer, nullptr);
			//SetUAVParameter(RHICmdList, CopyMorphDeltasCs->GetComputeShader(), CopyMorphDeltasCs->MorphNormalDeltaBuffer, nullptr);
			// In editor, it would be invalid sometimes. So set it to null and wait for update. 
			MorphVertexBuffer = nullptr;
		}
	}
	else
	{
		MorphPositionDeltaBuffer.Release();
		//MorphNormalDeltaBuffer.Release();
	}
}

bool FTressFXSceneProxy::WantsVelocityDraw()
{
	return Material && Material->GetMaterial() ? Material->GetMaterial()->bTressFXRenderVelocity : false;
}

void FTressFXSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView *>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const
{

	if (!TressFXHairObject || !TressFXHairObject->IndexBuffer.IsInitialized())
	{
		return;
	}

	//FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy(IsSelected());
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
				Mesh.bTressFX = true;
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &TressFXHairObject->IndexBuffer;
				//Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;

				//BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
				FTressFXVertexFactoryUserDataWrapper& UserDataWrapper = Collector.AllocateOneFrameResource<FTressFXVertexFactoryUserDataWrapper>();
				UserDataWrapper.Data.TressFXHairObject = TressFXHairObject;
				BatchElement.VertexFactoryUserData = &UserDataWrapper.Data;
				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, UseEditorDepthTest());
				BatchElement.PrimitiveUniformBuffer = DynamicPrimitiveUniformBuffer.UniformBuffer.GetUniformBufferRHI();

				BatchElement.FirstIndex = 0;
				BatchElement.NumIndices = TressFXHairObject->mtotalIndices;
				//BatchElement.bDrawIndexedInstanced = true;
				BatchElement.NumPrimitives = TressFXHairObject->mtotalIndices / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = TressFXHairObject->PosTanCollection.PositionsData.Num() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}

		}
	}
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

void FTressFXSimParameters::SetWind(const FVector& windDir, float windMag, int32 frame)
{

	float Magnitude = windMag * (FMath::Pow(FMath::Sin(frame * 0.01f), 2.0f) + 0.5f);

	FVector WindDirNomalized = FVector(windDir);
	WindDirNomalized.Normalize();

	FVector XAxis(1.0f, 0, 0);
	FVector xCrossW = XAxis ^ WindDirNomalized;

	FQuat RotFromXAxisToWindDir = FQuat::Identity;

	float Angle = FMath::Asin(xCrossW.Size());

	if (Angle > 0.001)
	{
		RotFromXAxisToWindDir = FQuat(xCrossW.GetSafeNormal(), Angle);
	}

	float AngleToWideWindCone = FMath::DegreesToRadians(40.f);

	SetWindCorner(RotFromXAxisToWindDir,
		FVector(0, -1.0, 0),
		AngleToWideWindCone,
		Magnitude,
		m_Wind);
	SetWindCorner(RotFromXAxisToWindDir,
		FVector(0, 1.0, 0),
		AngleToWideWindCone,
		Magnitude,
		m_Wind1);
	SetWindCorner(RotFromXAxisToWindDir,
		FVector(0, 0, -1.0),
		AngleToWideWindCone,
		Magnitude,
		m_Wind2);
	SetWindCorner(RotFromXAxisToWindDir,
		FVector(0, 0, 1.0),
		AngleToWideWindCone,
		Magnitude,
		m_Wind3);

	// fourth component unused. (used to store frame number, but no longer used).
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
	Result.g_Wind = m_Wind;
	Result.g_Wind1 = m_Wind1;
	Result.g_Wind2 = m_Wind2;
	Result.g_Wind3 = m_Wind3;
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

void FTressFXPosTanCollection::UAVBarrier(FRHICommandList& RHICmdList, FComputeFenceRHIParamRef Fence)
{
	FUnorderedAccessViewRHIParamRef UAVs[] =
	{
		Positions.UAV,
		PositionsPrev.UAV,
		PositionsPrevPrev.UAV,
		//Tangents.UAV
	};

	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, UAVs, ARRAY_COUNT(UAVs), Fence);
}

void FTressFXPosTanCollection::SetUAVs(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader)
{
	RHICmdList.SetUAVParameter(Shader, 0, Positions.UAV);
	RHICmdList.SetUAVParameter(Shader, 1, PositionsPrev.UAV);
	RHICmdList.SetUAVParameter(Shader, 2, PositionsPrevPrev.UAV);
	RHICmdList.SetUAVParameter(Shader, 3, Tangents.UAV);
}

void FTressFXPosTanCollection::UnsetUAVs(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader)
{
	RHICmdList.SetUAVParameter(Shader, 0, nullptr);
	RHICmdList.SetUAVParameter(Shader, 1, nullptr);
	RHICmdList.SetUAVParameter(Shader, 2, nullptr);
	RHICmdList.SetUAVParameter(Shader, 3, nullptr);
	RHICmdList.SetUAVParameter(Shader, 4, nullptr);
}
