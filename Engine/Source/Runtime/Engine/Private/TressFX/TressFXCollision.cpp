
#include "TressFX/TressFXCollision.h"
#include "TressFX/TressFXSceneProxy.h"
#include "TressFX/TressFXTypes.h"
#include "Engine/TressFXAsset.h"
#include "TressFX/TressFXBoneSkinning.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"

IMPLEMENT_SHADER_TYPE(, FInitializeSignedDistanceFieldCS, TEXT("/Engine/Private/TressFXSDFCollision.usf"), TEXT("InitializeSignedDistanceField"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FConstructSignedDistanceFieldCS, TEXT("/Engine/Private/TressFXSDFCollision.usf"), TEXT("ConstructSignedDistanceField"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FFinalizeSignedDistanceFieldCS, TEXT("/Engine/Private/TressFXSDFCollision.usf"), TEXT("FinalizeSignedDistanceField"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FCollideHairVerticesWithSdf_forwardCS, TEXT("/Engine/Private/TressFXSDFCollision.usf"), TEXT("CollideHairVerticesWithSdf_forward"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FCollideHairVerticesWithSdfCS, TEXT("/Engine/Private/TressFXSDFCollision.usf"), TEXT("CollideHairVerticesWithSdf"), SF_Compute)



static TAutoConsoleVariable<int32> CVarTFXSDFFinalize(TEXT("tfx.SDFCollisionFinalize"), 0, TEXT("SDFCollisionFinalize toggle."), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSDFConstruct(TEXT("tfx.SDFCollisionConstruct"), 0, TEXT("SDFCollisionConstruct toggle."), ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> CVarTFXSDFInitialize(TEXT("tfx.SDFCollisionInitialize"), 0, TEXT("SDFCollisionInitialize toggle."), ECVF_RenderThreadSafe);


void UpdateSDF(FRHICommandList& RHICmdList, FTressFXSceneProxy* Proxy)
{
	SCOPED_DRAW_EVENTF(RHICmdList, TressFXSDFCollision, TEXT("SimulateTressFXSDFCollision: %s"), *Proxy->TFXComponent->Asset->GetName());

	//BoneSkinning
	{
		SCOPED_DRAW_EVENT(RHICmdList, BoneSkinning);
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, Proxy->SDFMeshResources->MeshVertBuffer.UAV);

		auto BoneSkinningFence = RHICreateComputeFence(TEXT("BoneSkinningFence"));
		TShaderMapRef<FTressBoneSkinningCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
		RHICmdList.SetComputeShader(Shader->GetComputeShader());
		RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 0, Proxy->SDFMeshResources->MeshVertBuffer.UAV);

		SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXBoneSkinningUniformBuffer>(), Proxy->TressFXHairObject->BoneSkinningUniformBuffer);
		SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->BoneSkinningData, Proxy->SDFMeshResources->SkinningDataBuffer.SRV);
		SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->InitialVertexPositions, Proxy->SDFMeshResources->InitialVertexPositionBuffer.SRV);

		// Run BoneSkinning
		int32 DispatchSize = FMath::CeilToInt((float)Proxy->SDFMeshResources->MeshData.Vertices.Num() / (float)TRESSFX_SIM_THREAD_GROUP_SIZE);
		DispatchComputeShader(RHICmdList, *Shader, DispatchSize, 1, 1);

		Shader->ColMeshVertexPositions.UnsetUAV(RHICmdList, Shader->GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Proxy->SDFMeshResources->MeshVertBuffer.UAV, BoneSkinningFence);
	}

	if (CVarTFXSDFInitialize.GetValueOnRenderThread() == 1)
	{
		SCOPED_DRAW_EVENT(RHICmdList, InitializeSignedDistanceField);
		//InitializeSignedDistanceField - could probably just use clearUAV here with the default value.
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV);
		auto Fence = RHICreateComputeFence(TEXT("InitializeSignedDistanceFieldFence"));
		TShaderMapRef<FInitializeSignedDistanceFieldCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
		RHICmdList.SetComputeShader(Shader->GetComputeShader());
		RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 0, Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV);
		SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSDFUniformBuffer>(), Proxy->TressFXHairObject->SDFUniformBuffer);
		int32 DispatchSize = FMath::CeilToInt((float)(Proxy->SDFMeshResources->GetGridNumTotalCells()) / (float)TRESSFX_SIM_THREAD_GROUP_SIZE);
		DispatchComputeShader(RHICmdList, *Shader, DispatchSize, 1, 1);
		Shader->g_SignedDistanceField.UnsetUAV(RHICmdList, Shader->GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV, Fence);
	}

	if (CVarTFXSDFConstruct.GetValueOnRenderThread() == 1)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ConstructSignedDistanceField);
		auto Fence = RHICreateComputeFence(TEXT("ConstructSignedDistanceFieldFence"));
		TShaderMapRef<FConstructSignedDistanceFieldCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
		RHICmdList.SetComputeShader(Shader->GetComputeShader());
		RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 0, Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV);
		RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 1, Proxy->SDFMeshResources->MeshVertBuffer.UAV);
		SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSDFUniformBuffer>(), Proxy->TressFXHairObject->SDFUniformBuffer);
		SetSRVParameter(RHICmdList, Shader->GetComputeShader(), Shader->g_TrimeshVertexIndices, Proxy->SDFMeshResources->IndexBuffer.SRV);
		int32 DispatchSize = FMath::CeilToInt((float)Proxy->SDFMeshResources->MeshData.NumTriangles / (float)TRESSFX_SIM_THREAD_GROUP_SIZE);
		DispatchComputeShader(RHICmdList, *Shader, DispatchSize, 1, 1);
		Shader->g_SignedDistanceField.UnsetUAV(RHICmdList, Shader->GetComputeShader());
		Shader->ColMeshVertexPositions.UnsetUAV(RHICmdList, Shader->GetComputeShader());
		FUnorderedAccessViewRHIParamRef Resources[] = {
			Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV,
			Proxy->SDFMeshResources->MeshVertBuffer.UAV
		};
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, Resources, ARRAY_COUNT(Resources), Fence);
	}

	//FinalizeSignedDistanceField
	if (CVarTFXSDFFinalize.GetValueOnRenderThread() == 1)
	{
		SCOPED_DRAW_EVENT(RHICmdList, FinalizeSignedDistanceField);
		auto Fence = RHICreateComputeFence(TEXT("FinalizeSignedDistanceFieldFence"));
		TShaderMapRef<FFinalizeSignedDistanceFieldCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
		RHICmdList.SetComputeShader(Shader->GetComputeShader());
		RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 0, Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV);
		SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSDFUniformBuffer>(), Proxy->TressFXHairObject->SDFUniformBuffer);
		int32 DispatchSize = FMath::CeilToInt((float)(Proxy->SDFMeshResources->GetGridNumTotalCells()) / (float)TRESSFX_SIM_THREAD_GROUP_SIZE);
		DispatchComputeShader(RHICmdList, *Shader, DispatchSize, 1, 1);
		Shader->g_SignedDistanceField.UnsetUAV(RHICmdList, Shader->GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV, Fence);
	}
}

FComputeFenceRHIRef ApplySDF(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, FComputeFenceRHIRef InFence, FUnorderedAccessViewRHIParamRef SimResources[])
{
	Proxy->TressFXHairObject->PosTanCollection.UAVBarrier(RHICmdList, InFence);

	SCOPED_DRAW_EVENT(RHICmdList, FCollideHairVerticesWithSdf_forward);

	FComputeFenceRHIRef Fence = RHICreateComputeFence(TEXT("FCollideHairVerticesWithSdf_forwardFence"));
	TShaderMapRef<FCollideHairVerticesWithSdf_forwardCS> Shader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	RHICmdList.SetComputeShader(Shader->GetComputeShader());
	RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 0, Proxy->SDFMeshResources->SignedDistanceFieldBuffer.UAV);
	RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 1, Proxy->TressFXHairObject->PosTanCollection.Positions.UAV);
	RHICmdList.SetUAVParameter(Shader->GetComputeShader(), 2, Proxy->TressFXHairObject->PosTanCollection.PositionsPrev.UAV);
	SetUniformBufferParameter(RHICmdList, Shader->GetComputeShader(), Shader->GetUniformBufferParameter<FTressFXSDFUniformBuffer>(), Proxy->TressFXHairObject->SDFUniformBuffer);
	int32 DispatchSize = FMath::CeilToInt((float)Proxy->TressFXHairObject->NumTotalVertice / (float)TRESSFX_SIM_THREAD_GROUP_SIZE);
	DispatchComputeShader(RHICmdList, *Shader, DispatchSize, 1, 1);
	Proxy->TressFXHairObject->PosTanCollection.UnsetUAVs(RHICmdList, Shader->GetComputeShader());
	Shader->g_HairVertices.UnsetUAV(RHICmdList, Shader->GetComputeShader());
	return Fence;
}