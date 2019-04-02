
#pragma once
#include "TressFX/TressFXSimulation.h"
#include "TressFX/TressFXSceneProxy.h"
#include "TressFX/TressFXTypes.h"
#include "TressFX/TressFXCollision.h"
#include "Components/TressFXComponent.h"
#include "Engine/TressFXAsset.h"
#include "TressFX/TressFXBoneSkinning.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"

#define TOTEXT(a) TEXT(#a)
#define BIND_PARAM(p) p.Bind(Initializer.ParameterMap, TOTEXT(p))

IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::None>, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::Morphs>, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::MorphsAndVelocity>, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FIntegrationAndGlobalShapeConstraintsCS<FTressFXSimFeatures::Velocity>, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);

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
	return TEXT("/Engine/Private/TressFXSimulation.usf");
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
	BIND_PARAM(g_HairVertexPositions);
	BIND_PARAM(g_HairVertexPositionsPrev);
	BIND_PARAM(g_HairVertexPositionsPrevPrev);
}

bool FVelocityShockPropagationCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << g_HairVertexPositions << g_HairVertexPositionsPrev << g_HairVertexPositionsPrevPrev;

	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FVelocityShockPropagationCS, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("VelocityShockPropagation"), SF_Compute);

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
	return TEXT("/Engine/Private/TressFXSimulation.usf");
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
	BIND_PARAM(g_GlobalRotations);
	BIND_PARAM(g_HairRefVecsInLocalFrame);
	BIND_PARAM(g_HairVertexPositions);
	BIND_PARAM(g_HairVertexTangents);

}

bool FLocalShapeConstraintsWithIterationCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << g_GlobalRotations << g_HairRefVecsInLocalFrame << g_HairVertexPositions << g_HairVertexTangents;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FLocalShapeConstraintsWithIterationCS, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("LocalShapeConstraintsWithIteration"), SF_Compute);

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
	return TEXT("/Engine/Private/TressFXSimulation.usf");
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
	BIND_PARAM(g_HairVertexPositions);
	BIND_PARAM(g_HairVertexTangents);
	BIND_PARAM(g_HairRestLengthSRV);
	BIND_PARAM(g_HairVertexPositionsPrev);
}

bool FLengthConstriantsWindAndCollisionCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << g_HairVertexPositions << g_HairVertexTangents << g_HairRestLengthSRV << g_HairVertexPositionsPrev;

	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FLengthConstriantsWindAndCollisionCS, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("LengthConstriantsWindAndCollision"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FUpdateFollowHairVerticesCS, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("UpdateFollowHairVertices"), SF_Compute);

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
	return TEXT("/Engine/Private/TressFXSimulation.usf");
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
	BIND_PARAM(g_HairVertexPositions);
	BIND_PARAM(g_HairVertexPositionsPrev);
	TressfxSimParametersUniformBuffer.Bind(Initializer.ParameterMap, TEXT("tressfxSimParameters"));
}

bool FPrepareFollowHairBeforeTurningIntoGuideCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << g_HairVertexPositions << g_HairVertexPositionsPrev << TressfxSimParametersUniformBuffer;

	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FPrepareFollowHairBeforeTurningIntoGuideCS, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("PrepareFollowHairBeforeTurningIntoGuide"), SF_Compute);

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
	return TEXT("/Engine/Private/TressFXSimulation.usf");
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
	BIND_PARAM(g_GlobalRotations);
	BIND_PARAM(g_HairRefVecsInLocalFrame);
	BIND_PARAM(g_HairVertexPositions);
	BIND_PARAM(g_HairVertexTangents);
}

bool FLocalShapeConstraintsCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

	Ar << g_GlobalRotations << g_HairRefVecsInLocalFrame << g_HairVertexPositions << g_HairVertexTangents;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FLocalShapeConstraintsCS, TEXT("/Engine/Private/TressFXSimulation.usf"), TEXT("LocalShapeConstraints"), SF_Compute);

static TAutoConsoleVariable<int32> CVarTFXMorphTargets(TEXT("tfx.MorphTargets"), 1, TEXT("Enable/Disable Morph Targets"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass1(TEXT("tfx.SimPass1"), 1, TEXT("Enable/Disable IntegrationAndGlobalShapeConstraints Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass2(TEXT("tfx.SimPass2"), 1, TEXT("Enable/Disable VelocityShockPropagation Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass3(TEXT("tfx.SimPass3"), 1, TEXT("Enable/Disable LocalShapeConstraints"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass4(TEXT("tfx.SimPass4"), 1, TEXT("Enable/Disable LengthConstriantsWindAndCollision Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimPass5(TEXT("tfx.SimPass5"), 1, TEXT("Enable/Disable UpdateFollowHairVertices Pass"), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSimEnable(TEXT("tfx.EnableSimulation"), 1, TEXT("Globally Enable or disable TressFX Simulation"), ECVF_RenderThreadSafe);

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

template <FTressFXSimFeatures::Type TSimFeatures>
void SimulateTressFX_impl(FRHICommandList& RHICmdList, FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations)
{

	const bool bUseSDFCollision = CVarTFXSDFDisable.GetValueOnRenderThread() == 0 && Proxy->CollisionType == ETressFXCollisionType::TFXCollsion_SDF && Proxy->SDFMeshResources;
	
	// For dispatching one thread per one vertex
	int32 NumGroupsForCS_VertexLevel = (int32)((float)Proxy->TressFXHairObject->PosTanCollection.NumOfVerts / (float)TRESSFX_SIM_THREAD_GROUP_SIZE);
	// For dispatching one thread per one strand
	int32 NumGroupsForCS_StrandLevel = (int32)(((float)(Proxy->TressFXHairObject->NumTotalStrands) / (float)TRESSFX_SIM_THREAD_GROUP_SIZE));

	if (bUseSDFCollision)
	{
		UpdateSDF(RHICmdList, Proxy);
	}

	// Simulation Start
	{
		SCOPED_DRAW_EVENTF(RHICmdList, SimulateTressFXAsset, TEXT("SimulateTressFXAsset %s"), *Proxy->TFXComponent->Asset->GetName());

		FUnorderedAccessViewRHIParamRef SimResources[] =
		{
			Proxy->TressFXHairObject->PosTanCollection.Positions.UAV,
			Proxy->TressFXHairObject->PosTanCollection.PositionsPrev.UAV,
			Proxy->TressFXHairObject->PosTanCollection.PositionsPrevPrev.UAV,
			Proxy->TressFXHairObject->PosTanCollection.Tangents.UAV
		};

		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SimResources, ARRAY_COUNT(SimResources));

		const bool bUseMorphs = TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity || TSimFeatures == FTressFXSimFeatures::Morphs;
		const bool bNeedsVelocity = TSimFeatures == FTressFXSimFeatures::Velocity || TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity;

		if (bUseMorphs)
		{
			FUnorderedAccessViewRHIParamRef MorphResources[] =
			{
				Proxy->MorphPositionDeltaBuffer.UAV
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, MorphResources, ARRAY_COUNT(MorphResources));
		}

		if (CVarTFXSimPass1.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, IntegrationAndGlobalShapeConstraints);
			auto Fence = RHICreateComputeFence(TEXT("IntegrationAndGlobalShapeConstraintsCSFence"));

			TShaderMapRef<FIntegrationAndGlobalShapeConstraintsCS<TSimFeatures>> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
			RHICmdList.SetComputeShader(Shader->GetComputeShader());
			Proxy->TressFXHairObject->PosTanCollection.SetUAVs(RHICmdList, Shader->GetComputeShader());
			SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSimParametersUniformBuffer>(), Proxy->TressFXHairObject->SimParametersUniformBuffer);

			SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->g_InitialHairPositions, Proxy->TressFXHairObject->InitialHairPositionsBuffer.SRV);
			SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->g_BoneSkinningData, Proxy->TressFXHairObject->BoneSkinningDataBuffer.SRV);

			if (bUseMorphs)
			{
				SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->g_MorphDeltas, Proxy->MorphPositionDeltaBuffer.SRV);
			}
			if (bNeedsVelocity)
			{
				SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->g_FollowHairRootOffset, Proxy->TressFXHairObject->FollowHairRootOffsetBuffer.SRV);
			}
			
			DispatchComputeShader(RHICmdList, *Shader, NumGroupsForCS_VertexLevel, 1, 1);
			Proxy->TressFXHairObject->PosTanCollection.UAVBarrier(RHICmdList, Fence);

		}

		if (CVarTFXSimPass2.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, VelocityShockPropagation);
			auto Fence = RHICreateComputeFence(TEXT("VelocityShockPropagationCSFence"));

			TShaderMapRef<FVelocityShockPropagationCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(Shader->GetComputeShader());

			DispatchComputeShader(RHICmdList, *Shader, NumGroupsForCS_StrandLevel, 1, 1);
			Proxy->TressFXHairObject->PosTanCollection.UAVBarrier(RHICmdList, Fence);

		}

		if (CVarTFXSimPass3.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, LocalShapeConstraints);
			TShaderMapRef<FLocalShapeConstraintsCS> LocalShapeConstraintsCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(LocalShapeConstraintsCS->GetComputeShader());

			SetSRVParameter(RHICmdList, LocalShapeConstraintsCS->GetComputeShader(), LocalShapeConstraintsCS->g_GlobalRotations, Proxy->TressFXHairObject->GlobalRotationsBuffer.SRV);
			SetSRVParameter(RHICmdList, LocalShapeConstraintsCS->GetComputeShader(), LocalShapeConstraintsCS->g_HairRefVecsInLocalFrame, Proxy->TressFXHairObject->HairRefVecsInLocalFrameBuffer.SRV);
			for (int32 i = 0; i < CPULocalShapeIterations; ++i)
			{
				auto Fence = RHICreateComputeFence(TEXT("LocalShapeConstraintsCSFence"));
				DispatchComputeShader(RHICmdList, *LocalShapeConstraintsCS, NumGroupsForCS_StrandLevel, 1, 1);
				Proxy->TressFXHairObject->PosTanCollection.UAVBarrier(RHICmdList, Fence);
			}
		}
		if (CVarTFXSimPass4.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, LengthConstriantsWindAndCollision);
			auto Fence = RHICreateComputeFence(TEXT("LengthConstriantsWindAndCollisionCSFence"));
			TShaderMapRef<FLengthConstriantsWindAndCollisionCS> LengthConstriantsWindAndCollisionCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

			RHICmdList.SetComputeShader(LengthConstriantsWindAndCollisionCS->GetComputeShader());
			SetSRVParameter(RHICmdList, LengthConstriantsWindAndCollisionCS->GetComputeShader(), LengthConstriantsWindAndCollisionCS->g_HairRestLengthSRV, Proxy->TressFXHairObject->HairRestLengthSRVBuffer.SRV);

			DispatchComputeShader(RHICmdList, *LengthConstriantsWindAndCollisionCS, NumGroupsForCS_VertexLevel, 1, 1);
			Proxy->TressFXHairObject->PosTanCollection.UAVBarrier(RHICmdList, Fence);
		}

		FComputeFenceRHIRef UpdateFollowHairsFence = nullptr;

		if (CVarTFXSimPass5.GetValueOnRenderThread() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, UpdateFollowHairVertices);
			UpdateFollowHairsFence = RHICreateComputeFence(TEXT("UpdateFollowHairVerticesCSFence"));
			TShaderMapRef<FUpdateFollowHairVerticesCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
			RHICmdList.SetComputeShader(Shader->GetComputeShader());
			Proxy->TressFXHairObject->PosTanCollection.SetUAVs(RHICmdList, Shader->GetComputeShader());
			SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->g_FollowHairRootOffset, Proxy->TressFXHairObject->FollowHairRootOffsetBuffer.SRV);
			SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSimParametersUniformBuffer>(), Proxy->TressFXHairObject->SimParametersUniformBuffer);
			DispatchComputeShader(RHICmdList, *Shader, NumGroupsForCS_VertexLevel, 1, 1);
			Shader->g_HairVertexTangents.UnsetUAV(RHICmdList, Shader->GetComputeShader());
			Proxy->TressFXHairObject->PosTanCollection.UnsetUAVs(RHICmdList, Shader->GetComputeShader());
		}

		if (bUseSDFCollision && CVarTFXSDFApply.GetValueOnRenderThread() == 1)
		{
			FComputeFenceRHIRef ApplyFence = ApplySDF(RHICmdList, Proxy, UpdateFollowHairsFence, SimResources);
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SimResources, ARRAY_COUNT(SimResources), ApplyFence);
		}
		else
		{
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SimResources, ARRAY_COUNT(SimResources), UpdateFollowHairsFence);
		}
		Proxy->SimulationFrame++;
	}
	
}
