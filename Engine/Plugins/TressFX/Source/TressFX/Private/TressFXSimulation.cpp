#include "TressFXSimulation.h"
#include "TressFXSceneProxy.h"
#include "TressFXTypes.h"
#include "TressFXCollision.h"
#include "TressFXComponent.h"
#include "TressFXAsset.h"
#include "TressFXBoneSkinning.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"


struct FTressFXSimFeatures
{
	enum Type
	{
		None = 0,
		Morphs = 1,
		Velocity = 2,
		MorphsAndVelocity = Morphs | Velocity,
		FullSimulation = 4,
		AllFeatures = MorphsAndVelocity | FullSimulation
	};
};

template <FTressFXSimFeatures::Type TSimFeatures>
class FIntegrationAndGlobalShapeConstraintsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FIntegrationAndGlobalShapeConstraintsCS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static const TCHAR* GetSourceFilename();;

	static const TCHAR*GetFunctionName();;

	FIntegrationAndGlobalShapeConstraintsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);;


	FIntegrationAndGlobalShapeConstraintsCS();;

	virtual bool Serialize(FArchive& Ar) override;;


public:

	FRWShaderParameter HairVertexPositions;
	FRWShaderParameter HairVertexPositionsPrev;
	FRWShaderParameter HairVertexPositionsPrevPrev;
	FRWShaderParameter HairVertexTangents;

	FShaderResourceParameter BoneSkinningData;
	FShaderResourceParameter BoneIndexData;
	FShaderResourceParameter InitialHairPositions;
	FShaderResourceParameter MorphDeltas;
	FShaderResourceParameter FollowHairRootOffset;

	FShaderUniformBufferParameter TressfxSimParametersUniformBuffer;

};

template <FTressFXSimFeatures::Type TSimFeatures>
bool FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

template <FTressFXSimFeatures::Type TSimFeatures>
void FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
	OutEnvironment.SetDefine(TEXT("TRESSFX_MORPHS"), TSimFeatures & FTressFXSimFeatures::Morphs ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("UPDATE_PREV_FOLLOW_HAIRS"), TSimFeatures & FTressFXSimFeatures::Velocity ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("SIM_QUALITY"), TSimFeatures & FTressFXSimFeatures::FullSimulation ? TEXT("1") : TEXT("0"));
}

template <FTressFXSimFeatures::Type TSimFeatures>
const TCHAR* FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>::GetSourceFilename()
{
	return TEXT("TressFXSimulation");
}

template <FTressFXSimFeatures::Type TSimFeatures>
const TCHAR* FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>::GetFunctionName()
{
	return TEXT("IntegrationAndGlobalShapeConstraints");
}

template <FTressFXSimFeatures::Type TSimFeatures>
FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>::FIntegrationAndGlobalShapeConstraintsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("HairVertexPositions"));
	HairVertexPositionsPrev.Bind(Initializer.ParameterMap, TEXT("HairVertexPositionsPrev"));
	HairVertexPositionsPrevPrev.Bind(Initializer.ParameterMap, TEXT("HairVertexPositionsPrevPrev"));
	HairVertexTangents.Bind(Initializer.ParameterMap, TEXT("HairVertexTangents"));
	BoneSkinningData.Bind(Initializer.ParameterMap, TEXT("BoneSkinningData"));
	BoneIndexData.Bind(Initializer.ParameterMap, TEXT("BoneIndexData"));
	InitialHairPositions.Bind(Initializer.ParameterMap, TEXT("InitialHairPositions"));

	if (TSimFeatures & FTressFXSimFeatures::Morphs)
	{
		MorphDeltas.Bind(Initializer.ParameterMap, TEXT("MorphDeltas"));
	}

	if (TSimFeatures & FTressFXSimFeatures::Velocity)
	{
		FollowHairRootOffset.Bind(Initializer.ParameterMap, TEXT("FollowHairRootOffset"));
	}

	TressfxSimParametersUniformBuffer.Bind(Initializer.ParameterMap, TEXT("TressfxSimParameters"));
}

template <FTressFXSimFeatures::Type TSimFeatures>
FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>::FIntegrationAndGlobalShapeConstraintsCS()
{

}

template <FTressFXSimFeatures::Type TSimFeatures>
bool FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << HairVertexPositions << HairVertexPositionsPrev << HairVertexPositionsPrevPrev
		<< HairVertexTangents << BoneSkinningData << BoneIndexData << InitialHairPositions;

	if (TSimFeatures & FTressFXSimFeatures::Morphs)
	{
		Ar << MorphDeltas;
	}
	if (TSimFeatures & FTressFXSimFeatures::Velocity)
	{
		Ar << FollowHairRootOffset;
	}

	return bShaderHasOutdatedParameters;
}


IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::None>, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::Morphs>, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::MorphsAndVelocity>, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::Velocity>, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::AllFeatures>, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);

bool FVelocityShockPropagationCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FVelocityShockPropagationCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
}

const TCHAR* FVelocityShockPropagationCS::GetSourceFilename()
{
	return TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf");
}

const TCHAR* FVelocityShockPropagationCS::GetFunctionName()
{
	return TEXT("VelocityShockPropagation");
}

FVelocityShockPropagationCS::FVelocityShockPropagationCS()
{

}

FVelocityShockPropagationCS::FVelocityShockPropagationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("HairVertexPositions"));
	HairVertexPositionsPrev.Bind(Initializer.ParameterMap, TEXT("HairVertexPositionsPrev"));
	HairVertexPositionsPrevPrev.Bind(Initializer.ParameterMap, TEXT("HairVertexPositionsPrevPrev"));
}

bool FVelocityShockPropagationCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << HairVertexPositions << HairVertexPositionsPrev << HairVertexPositionsPrevPrev;

	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FVelocityShockPropagationCS, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("VelocityShockPropagation"), SF_Compute);

bool FLocalShapeConstraintsWithIterationCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}


void FLocalShapeConstraintsWithIterationCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
}

const TCHAR* FLocalShapeConstraintsWithIterationCS::GetSourceFilename()
{
	return TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf");
}

const TCHAR* FLocalShapeConstraintsWithIterationCS::GetFunctionName()
{
	return TEXT("LocalShapeConstraintsWithIteration");
}

FLocalShapeConstraintsWithIterationCS::FLocalShapeConstraintsWithIterationCS()
{

}

FLocalShapeConstraintsWithIterationCS::FLocalShapeConstraintsWithIterationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	GlobalRotations.Bind(Initializer.ParameterMap, TEXT("GlobalRotations"));
	HairRefVecsInLocalFrame.Bind(Initializer.ParameterMap, TEXT("HairRefVecsInLocalFrame"));
	HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("HairVertexPositions"));
	HairVertexTangents.Bind(Initializer.ParameterMap, TEXT("HairVertexTangents"));
}

bool FLocalShapeConstraintsWithIterationCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << GlobalRotations << HairRefVecsInLocalFrame << HairVertexPositions << HairVertexTangents;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FLocalShapeConstraintsWithIterationCS, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("LocalShapeConstraintsWithIteration"), SF_Compute);

bool FLengthConstriantsWindAndCollisionCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}


void FLengthConstriantsWindAndCollisionCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
}

const TCHAR* FLengthConstriantsWindAndCollisionCS::GetSourceFilename()
{
	return TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf");
}

const TCHAR* FLengthConstriantsWindAndCollisionCS::GetFunctionName()
{
	return TEXT("LengthConstriantsWindAndCollision");
}

FLengthConstriantsWindAndCollisionCS::FLengthConstriantsWindAndCollisionCS()
{

}

FLengthConstriantsWindAndCollisionCS::FLengthConstriantsWindAndCollisionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("HairVertexPositions"));
	HairVertexTangents.Bind(Initializer.ParameterMap, TEXT("HairVertexTangents"));
	HairRestLengthSRV.Bind(Initializer.ParameterMap, TEXT("HairRestLengthSRV"));
	HairVertexPositionsPrev.Bind(Initializer.ParameterMap, TEXT("HairVertexPositionsPrev"));
	HairVertexPositionsPrevPrev.Bind(Initializer.ParameterMap, TEXT("HairVertexPositionsPrevPrev"));
}

bool FLengthConstriantsWindAndCollisionCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << HairVertexPositions << HairVertexTangents << HairRestLengthSRV << HairVertexPositionsPrev << HairVertexPositionsPrevPrev;

	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FLengthConstriantsWindAndCollisionCS, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("LengthConstriantsWindAndCollision"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FUpdateFollowHairVerticesCS, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("UpdateFollowHairVertices"), SF_Compute);

bool FPrepareFollowHairBeforeTurningIntoGuideCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FPrepareFollowHairBeforeTurningIntoGuideCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
}

const TCHAR* FPrepareFollowHairBeforeTurningIntoGuideCS::GetSourceFilename()
{
	return TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf");
}

const TCHAR* FPrepareFollowHairBeforeTurningIntoGuideCS::GetFunctionName()
{
	return TEXT("PrepareFollowHairBeforeTurningIntoGuide");
}

FPrepareFollowHairBeforeTurningIntoGuideCS::FPrepareFollowHairBeforeTurningIntoGuideCS()
{

}

FPrepareFollowHairBeforeTurningIntoGuideCS::FPrepareFollowHairBeforeTurningIntoGuideCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("HairVertexPositions"));
	HairVertexPositionsPrev.Bind(Initializer.ParameterMap, TEXT("HairVertexPositionsPrev"));
	TressfxSimParametersUniformBuffer.Bind(Initializer.ParameterMap, TEXT("TressfxSimParameters"));
}

bool FPrepareFollowHairBeforeTurningIntoGuideCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << HairVertexPositions << HairVertexPositionsPrev << TressfxSimParametersUniformBuffer;

	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FPrepareFollowHairBeforeTurningIntoGuideCS, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("PrepareFollowHairBeforeTurningIntoGuide"), SF_Compute);

bool FLocalShapeConstraintsCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FLocalShapeConstraintsCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
}

const TCHAR* FLocalShapeConstraintsCS::GetSourceFilename()
{
	return TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf");
}

const TCHAR* FLocalShapeConstraintsCS::GetFunctionName()
{
	return TEXT("LocalShapeConstraints");
}

FLocalShapeConstraintsCS::FLocalShapeConstraintsCS()
{

}

FLocalShapeConstraintsCS::FLocalShapeConstraintsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	GlobalRotations.Bind(Initializer.ParameterMap, TEXT("GlobalRotations"));
	HairRefVecsInLocalFrame.Bind(Initializer.ParameterMap, TEXT("HairRefVecsInLocalFrame"));
	HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("HairVertexPositions"));
	HairVertexTangents.Bind(Initializer.ParameterMap, TEXT("HairVertexTangents"));
}

bool FLocalShapeConstraintsCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << GlobalRotations << HairRefVecsInLocalFrame << HairVertexPositions << HairVertexTangents;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FLocalShapeConstraintsCS, TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf"), TEXT("LocalShapeConstraints"), SF_Compute);

static TAutoConsoleVariable<int32> CVarTFXMorphTargets(TEXT("tfx.MorphTargets"), 1, TEXT("Enable/Disable Morph Targets"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass1(TEXT("tfx.SimPass1"), 1, TEXT("Enable/Disable IntegrationAndGlobalShapeConstraints Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass2(TEXT("tfx.SimPass2"), 1, TEXT("Enable/Disable VelocityShockPropagation Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass3(TEXT("tfx.SimPass3"), 1, TEXT("Enable/Disable LocalShapeConstraints"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass4(TEXT("tfx.SimPass4"), 1, TEXT("Enable/Disable LengthConstriantsWindAndCollision Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass5(TEXT("tfx.SimPass5"), 1, TEXT("Enable/Disable UpdateFollowHairVertices Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimEnable(TEXT("tfx.EnableSimulation"), 1, TEXT("Globally Enable or disable TressFX Simulation"), ECVF_RenderThreadSafe);

template <FTressFXSimFeatures::Type TSimFeatures>
void SimulateTressFX_impl(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations);

void SimulateTressFX(FRHICommandList& RHICmdList, FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations)
{
	if (!Proxy->bInitialized || CVarTFXSimEnable.GetValueOnRenderThread() < 1)
	{
		return;
	}

	const bool bUseMorphTargets =
		(
			Proxy->GetMorphTargetsEnabled() &&
			CVarTFXMorphTargets.GetValueOnAnyThread() > 0 &&
			Proxy->MorphPositionDeltaBuffer.NumBytes > 0 &&
			Proxy->MorphPositionDeltaBuffer.Buffer.IsValid()
		);
	const bool bNeedsVelocity = Proxy->WantsVelocityDraw();

	if (Proxy->TFXComponent->TressFXSimulationSettings.SimulationQuality != ETressFXSimulationQuality::TFXSim_Disable)
	{
		SimulateTressFX_impl<FTressFXSimFeatures::AllFeatures >(RHICmdList, Proxy, CPULocalShapeIterations);
	}
	else
	{
		if (bUseMorphTargets)
		{
			if (bNeedsVelocity)
			{
				SimulateTressFX_impl<FTressFXSimFeatures::MorphsAndVelocity>(RHICmdList, Proxy, CPULocalShapeIterations);
			}
			else
			{
				SimulateTressFX_impl<FTressFXSimFeatures::Morphs>(RHICmdList, Proxy, CPULocalShapeIterations);
			}
		}
		else
		{
			if (bNeedsVelocity)
			{
				SimulateTressFX_impl<FTressFXSimFeatures::Velocity>(RHICmdList, Proxy, CPULocalShapeIterations);
			}
			else
			{
				SimulateTressFX_impl<FTressFXSimFeatures::None>(RHICmdList, Proxy, CPULocalShapeIterations);
			}
		}
	}
}

template <FTressFXSimFeatures::Type TSimFeatures>
void SimulateTressFX_impl(FRHICommandList& RHICmdList, FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations)
{

	const bool bUseSDFCollision = CVarTFXSDFDisable.GetValueOnRenderThread() == 0 && Proxy->CollisionType == ETressFXCollisionType::TFXCollsion_SDF && Proxy->SDFMeshResources;
	
	ETressFXSimulationQuality SimQuality = Proxy->TFXComponent->TressFXSimulationSettings.SimulationQuality;

	// For dispatching one thread per one vertex
	int32 NumGroupsForCS_VertexLevel = (int32)((float)Proxy->InstanceRenderData->PosTanCollection.NumOfVerts / (float)TRESSFX_SIM_THREAD_GROUP_SIZE);
	// For dispatching one thread per one strand
	int32 NumGroupsForCS_StrandLevel = (int32)(((float)(Proxy->TressFXHairObject->NumTotalStrands) / (float)TRESSFX_SIM_THREAD_GROUP_SIZE));

	if (bUseSDFCollision)
	{
		UpdateSDF(RHICmdList, Proxy);
	}

	// Simulation Start
	{
		SCOPED_DRAW_EVENTF(RHICmdList, SimulateTressFXAsset, TEXT("SimulateTressFXAsset %s"), *Proxy->TFXComponent->Asset->GetName());

		FRHIUnorderedAccessView* SimResources[] =
		{
			Proxy->InstanceRenderData->PosTanCollection.Positions.UAV,
			Proxy->InstanceRenderData->PosTanCollection.PositionsPrev.UAV,
			Proxy->InstanceRenderData->PosTanCollection.PositionsPrevPrev.UAV,
			Proxy->InstanceRenderData->PosTanCollection.Tangents.UAV
		};

		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SimResources, ARRAY_COUNT(SimResources));

		const bool bUseMorphs = (bool)(TSimFeatures & FTressFXSimFeatures::Morphs);
		const bool bNeedsVelocity = (bool)(TSimFeatures & FTressFXSimFeatures::Velocity);

		if (bUseMorphs)
		{
			FRHIUnorderedAccessView* MorphResources[] =
			{
				Proxy->MorphPositionDeltaBuffer.UAV
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, MorphResources, ARRAY_COUNT(MorphResources));
		}

		if (CVarTFXSimPass1.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, IntegrationAndGlobalShapeConstraints);
			TRefCountPtr<FRHIComputeFence> Fence = RHICreateComputeFence(TEXT("IntegrationAndGlobalShapeConstraintsCSFence"));

			TShaderMapRef<FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
			RHICmdList.SetComputeShader(Shader->GetComputeShader());
			Proxy->InstanceRenderData->PosTanCollection.SetUAVs(RHICmdList, Shader->GetComputeShader());
			SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSimParametersUniformBuffer>(), Proxy->InstanceRenderData->SimParametersUniformBuffer);

			SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->InitialHairPositions, Proxy->TressFXHairObject->InitialHairPositionsBuffer.SRV);
			SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->BoneSkinningData, Proxy->TressFXHairObject->BoneSkinningDataBuffer.SRV);
			SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->BoneIndexData, Proxy->TressFXHairObject->BoneIndexDataBuffer.SRV);
			if (bUseMorphs)
			{
				SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->MorphDeltas, Proxy->MorphPositionDeltaBuffer.SRV);
			}
			if (bNeedsVelocity)
			{
				SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->FollowHairRootOffset, Proxy->TressFXHairObject->FollowHairRootOffsetBuffer.SRV);
			}
			
			DispatchComputeShader(RHICmdList, *Shader, NumGroupsForCS_VertexLevel, 1, 1);
			Proxy->InstanceRenderData->PosTanCollection.UAVBarrier(RHICmdList, Fence);

		}

		if (SimQuality == ETressFXSimulationQuality::TFXSim_Full && CVarTFXSimPass2.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, VelocityShockPropagation);
			TRefCountPtr<FRHIComputeFence> Fence = RHICreateComputeFence(TEXT("VelocityShockPropagationCSFence"));

			TShaderMapRef<FVelocityShockPropagationCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(Shader->GetComputeShader());

			DispatchComputeShader(RHICmdList, *Shader, NumGroupsForCS_StrandLevel, 1, 1);
			Proxy->InstanceRenderData->PosTanCollection.UAVBarrier(RHICmdList, Fence);

		}

		if (SimQuality == ETressFXSimulationQuality::TFXSim_Full && CVarTFXSimPass3.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, LocalShapeConstraints);
			TShaderMapRef<FLocalShapeConstraintsCS> LocalShapeConstraintsCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(LocalShapeConstraintsCS->GetComputeShader());

			SetSRVParameter(RHICmdList, LocalShapeConstraintsCS->GetComputeShader(), LocalShapeConstraintsCS->GlobalRotations, Proxy->TressFXHairObject->GlobalRotationsBuffer.SRV);
			SetSRVParameter(RHICmdList, LocalShapeConstraintsCS->GetComputeShader(), LocalShapeConstraintsCS->HairRefVecsInLocalFrame, Proxy->TressFXHairObject->HairRefVecsInLocalFrameBuffer.SRV);
			for (int32 i = 0; i < CPULocalShapeIterations; ++i)
			{
				TRefCountPtr<FRHIComputeFence> Fence = RHICreateComputeFence(TEXT("LocalShapeConstraintsCSFence"));
				DispatchComputeShader(RHICmdList, *LocalShapeConstraintsCS, NumGroupsForCS_StrandLevel, 1, 1);
				Proxy->InstanceRenderData->PosTanCollection.UAVBarrier(RHICmdList, Fence);
			}
		}
		if (SimQuality == ETressFXSimulationQuality::TFXSim_Full && CVarTFXSimPass4.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, LengthConstriantsWindAndCollision);
			TRefCountPtr<FRHIComputeFence> Fence = RHICreateComputeFence(TEXT("LengthConstriantsWindAndCollisionCSFence"));
			TShaderMapRef<FLengthConstriantsWindAndCollisionCS> LengthConstriantsWindAndCollisionCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(LengthConstriantsWindAndCollisionCS->GetComputeShader());
			SetSRVParameter(RHICmdList, LengthConstriantsWindAndCollisionCS->GetComputeShader(), LengthConstriantsWindAndCollisionCS->HairRestLengthSRV, Proxy->TressFXHairObject->HairRestLengthSRVBuffer.SRV);

			DispatchComputeShader(RHICmdList, *LengthConstriantsWindAndCollisionCS, NumGroupsForCS_VertexLevel, 1, 1);
			Proxy->InstanceRenderData->PosTanCollection.UAVBarrier(RHICmdList, Fence);
		}

		TRefCountPtr<FRHIComputeFence> UpdateFollowHairsFence = nullptr;

		if (CVarTFXSimPass5.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, UpdateFollowHairVertices);
			UpdateFollowHairsFence = RHICreateComputeFence(TEXT("UpdateFollowHairVerticesCSFence"));
			TShaderMapRef<FUpdateFollowHairVerticesCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
			RHICmdList.SetComputeShader(Shader->GetComputeShader());
			Proxy->InstanceRenderData->PosTanCollection.SetUAVs(RHICmdList, Shader->GetComputeShader());
			SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->FollowHairRootOffset, Proxy->TressFXHairObject->FollowHairRootOffsetBuffer.SRV);
			SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSimParametersUniformBuffer>(), Proxy->InstanceRenderData->SimParametersUniformBuffer);
			DispatchComputeShader(RHICmdList, *Shader, NumGroupsForCS_VertexLevel, 1, 1);
			Shader->HairVertexTangents.UnsetUAV(RHICmdList, Shader->GetComputeShader());
			Proxy->InstanceRenderData->PosTanCollection.UnsetUAVs(RHICmdList, Shader->GetComputeShader());
		}

		if (bUseSDFCollision && CVarTFXSDFApply.GetValueOnRenderThread() == 1)
		{
			TRefCountPtr<FRHIComputeFence> ApplyFence = ApplySDF(RHICmdList, Proxy, UpdateFollowHairsFence, SimResources);
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SimResources, ARRAY_COUNT(SimResources), ApplyFence);
		}
		else
		{
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SimResources, ARRAY_COUNT(SimResources), UpdateFollowHairsFence);
		}
		Proxy->SimulationFrame++;
	}
	
}
