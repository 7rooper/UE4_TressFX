
#pragma once
#include "TressFXRendering.h"
#include "TressFXShaders.h"
#include "SceneUtils.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "DepthRendering.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "IHeadMountedDisplay.h"
#include "ClearQuad.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "TressFX/TressFXSceneProxy.h"
#include "TressFX/TressFXShortCut.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"

DEFINE_LOG_CATEGORY(TressFXRenderingLog);

extern TAutoConsoleVariable<int32> CVarTressFXKBufferSize;
extern TAutoConsoleVariable<int32> CVarTressFXType;

#pragma optimize("", off)

void TressFXCopySceneDepth(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext, TRefCountPtr<IPooledRenderTarget> Destination)
{

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Destination->GetRenderTargetItem().TargetableTexture);

	FRHIRenderPassInfo RPInfo(Destination->GetRenderTargetItem().TargetableTexture, EDepthStencilTargetActions::ClearDepthStencil_DontStoreDepthStencil);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXCopySceneDepth"));
	
	TShaderMapRef<FScreenVS> VertexShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	TShaderMapRef<FTressFXCopyDepthPS> PixelShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
	PixelShader->SetParameters(RHICmdList, View);

	const FIntPoint BufferSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
	RHICmdList.SetViewport(0, 0, 0, BufferSize.X, BufferSize.Y, 1);

	DrawRectangle(
		RHICmdList,
		0, 0,
		BufferSize.X, BufferSize.Y,
		0, 0,
		BufferSize.X, BufferSize.Y,
		BufferSize,
		BufferSize,
		*VertexShader,
		EDrawRectangleFlags::EDRF_Default,
		1
	);
	RHICmdList.EndRenderPass();
}

//////////////////////////////////////////////////////////////////////////
//FTressFXDepthsVelocityPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

template<bool bCalcVelocity>
void FTressFXDepthsVelocityPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	bool bNoDepth
)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	FShaderPipeline* ShaderPipeline = nullptr;

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	if (bNoDepth)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	}
	else
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	}

	//SetDepthPassDitheredLODTransitionState(ViewIfDynamicMeshCommand, MeshBatch, StaticMeshId, DrawRenderState);

	FTressFXShaderElementData ShaderElementData(ETressFXPass::DepthsVelocity, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	TMeshProcessorShaders<
		TTressFX_ShortCutVS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFX_VelocityDepthPS<bCalcVelocity>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFX_VelocityDepthPS<bCalcVelocity>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<TTressFX_ShortCutVS<bCalcVelocity>>(VertexFactory->GetType());

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(TFXShaders.VertexShader, TFXShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		TFXShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData
	);
}

void FTressFXDepthsVelocityPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{

	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
	const FMaterial& MeshBatchMaterial = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

	const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

	const EBlendMode BlendMode = MeshBatchMaterial.GetBlendMode();
	const bool bWantsVelocity = MeshBatchMaterial.TressFXShouldRenderVelocity();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	const bool bNoDepth = bIsTranslucent;

	if (bWantsVelocity == false && bNoDepth == true)
	{
		//nothing to do
		return;
	}
	
	bool bDraw = MeshBatchMaterial.IsUsedWithTressFX() && MeshBatch.bTressFX;

	if (bDraw)
	{
		//guess i dont really need the proxy for now, but i might add some more configurable render settings on it later
		const FTressFXSceneProxy* TFXProxy = ((const FTressFXSceneProxy*)(PrimitiveSceneProxy));
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, MeshBatchMaterial);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, MeshBatchMaterial);

		if(bWantsVelocity)
		{
			Process<true>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode, bNoDepth);
		}
		else 
		{
			Process<false>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode, bNoDepth);
		}
	}
}

FTressFXDepthsVelocityPassMeshProcessor::FTressFXDepthsVelocityPassMeshProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState = InPassDrawRenderState;
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.DepthPassUniformBuffer);
}

void SetupDepthsVelocityPassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
}

FMeshPassProcessor* CreateTRessFXDepthsVelocityPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState TRessFXDepthsVelocityPassState;
	SetupDepthsVelocityPassState(TRessFXDepthsVelocityPassState);
	return new(FMemStack::Get()) FTressFXDepthsVelocityPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, TRessFXDepthsVelocityPassState, InDrawListContext);
}

//////////////////////////////////////////////////////////////////////////
//TressFXDepthsAlphaPassMeshProcessor
/////////////////////////////////////////////////////////////////////////


template<bool bCalcVelocity>
void TressFXDepthsAlphaPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode
)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	FShaderPipeline* ShaderPipeline = nullptr;

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	//JAKETODO?	
	//SetDepthPassDitheredLODTransitionState(ViewIfDynamicMeshCommand, MeshBatch, StaticMeshId, DrawRenderState);

	FTressFXShaderElementData ShaderElementData(ETressFXPass::DepthsAlpha, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	TMeshProcessorShaders<
		TTressFX_ShortCutVS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFX_DepthsAlphaPS<bCalcVelocity>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFX_DepthsAlphaPS<bCalcVelocity>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<TTressFX_ShortCutVS<bCalcVelocity>>(VertexFactory->GetType());

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(TFXShaders.VertexShader, TFXShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		TFXShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData
	);
}

void TressFXDepthsAlphaPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
	const FMaterial& MeshBatchMaterial = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

	const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

	const EBlendMode BlendMode = MeshBatchMaterial.GetBlendMode();
	const bool bWantsVelocity = MeshBatchMaterial.TressFXShouldRenderVelocity();

	const bool bDraw = MeshBatchMaterial.IsUsedWithTressFX() && MeshBatch.bTressFX;

	if (bDraw)
	{
		//guess i dont really need the proxy for now, but i might add some more configurable render settings on it later
		const FTressFXSceneProxy* TFXProxy = ((const FTressFXSceneProxy*)(PrimitiveSceneProxy));
		const ERasterizerFillMode MeshFillMode = FM_Solid;// ComputeMeshFillMode(MeshBatch, MeshBatchMaterial);
		const ERasterizerCullMode MeshCullMode = CM_CW; // ComputeMeshCullMode(MeshBatch, MeshBatchMaterial);
		
		//JAKETODO do i need to force Front ccW?
		if (bWantsVelocity)
		{
			Process<true>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
		}
		else
		{
			Process<false>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
		}
	}
}

TressFXDepthsAlphaPassMeshProcessor::TressFXDepthsAlphaPassMeshProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState = InPassDrawRenderState;
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
}

void SetupDepthsAlphaPassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	DrawRenderState.SetBlendState(TStaticBlendState<
		//accum inv allpha
		CW_RED
		, BO_Add
		, BF_Zero
		, BF_SourceColor
		, BO_Add
		, BF_Zero
		, BF_SourceAlpha
		//velocity
		, CW_RGBA
		, BO_Add
		, BF_One
		, BF_Zero
		, BO_Add
		, BF_One
		, BF_Zero
	>::GetRHI());
	DrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		0xff, 0xff, DWM_Zero //JAKETODO, DWM_zero only currently implemented in dx11/12
	>::GetRHI());

}

FMeshPassProcessor* CreateTRessFXDepthsAlphaPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	SetupDepthsAlphaPassState(PassDrawRenderState);
	return new(FMemStack::Get()) TressFXDepthsAlphaPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext);
}

//FTressFXPPLLDrawingPolicy::FTressFXPPLLDrawingPolicy(const FVertexFactory* InVertexFactory, const FMaterialRenderProxy* InMaterialRenderProxy, const FMaterial& InMaterialResource, const FMeshDrawingPolicyOverrideSettings& InOverrideSettings, ERHIFeatureLevel::Type InFeatureLevel) :
//	FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, DVSM_None)
//{
//	VertexShader = InMaterialResource.GetShader<FTressFX_PPLLBuildVS>(VertexFactory->GetType());
//	PixelShader = InMaterialResource.GetShader<FTressFX_PPLLBuildPS>(InVertexFactory->GetType());
//}
//
//void FTressFXPPLLDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FTressFXPPLLDrawingPolicy::ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState) const
//{
//	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, View->ViewUniformBuffer, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated, DrawRenderState);
//	PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, View->ViewUniformBuffer, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated, DrawRenderState);
//
//}
//
//FBoundShaderStateInput FTressFXPPLLDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
//{
//	return FBoundShaderStateInput(
//		FMeshDrawingPolicy::GetVertexDeclaration(),
//		VertexShader->GetVertexShader(),
//		nullptr,
//		nullptr,
//		PixelShader->GetPixelShader(),
//		NULL);
//}
//
//void FTressFXPPLLDrawingPolicy::SetMeshRenderState(FRHICommandList& RHICmdList, const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMeshBatch& Mesh, int32 BatchElementIndex, const FDrawingPolicyRenderState& DrawRenderState) const
//{
//	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
//	VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
//	PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
//}
//
//void FTressFXPPLLDrawingPolicy::SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View)
//{
//
//	DrawRenderState.SetBlendState(TStaticBlendState<
//		CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
//		CW_ALPHA, BO_Add, BF_Zero, BF_One, BO_Add, BF_One, BF_Zero,
//		CW_RGB, BO_Add, BF_One, BF_Zero, BO_Add, BF_Zero, BF_One,
//		CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
//		CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
//		CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
//		CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero,
//		CW_NONE, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
//
//	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
//		true,
//		CF_GreaterEqual,
//		true,
//		CF_Always,
//		SO_Keep,
//		SO_Keep,
//		SO_SaturatedIncrement,
//		true,
//		CF_Always,
//		SO_Keep,
//		SO_Keep,
//		SO_SaturatedIncrement,
//		0xFF,
//		0xFF,
//		DWM_Zero>::GetRHI());
//}

//bool FTressFXPPLLDrawingPolicyFactory::DrawDynamicMesh(FRHICommandList& RHICmdList, const FViewInfo& View, ContextType DrawingContext, const FMeshBatch& Mesh, bool bPreFog, const FDrawingPolicyRenderState& DrawRenderState, const FPrimitiveSceneProxy* PrimitiveSceneProxy, FHitProxyId HitProxyId)
//{
//	if (PrimitiveSceneProxy)
//	{
//		SCOPED_DRAW_EVENT(RHICmdList, TressFXBuildPPLL);
//		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
//		const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
//		FTressFXPPLLDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), ComputeMeshOverrideSettings(Mesh), View.GetFeatureLevel());
//
//		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
//		DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
//		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
//		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
//		DrawingPolicy.SetSharedState(RHICmdList, &View, FTressFXPPLLDrawingPolicy::ContextDataType(), DrawRenderStateLocal);
//
//		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
//		{
//			SCOPED_DRAW_EVENT(RHICmdList, TressFXBuildPPLLDrawMesh);
//			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal);
//			DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex);
//		}
//		return true;
//	}
//	return false;
//}


//////////////////////////////////////////////////////////////////////////
// FillColor
//////////////////////////////////////////////////////////////////////////

//FTressFXFillColorDrawingPolicy::FTressFXFillColorDrawingPolicy(const FVertexFactory* InVertexFactory, const FMaterialRenderProxy* InMaterialRenderProxy, const FMaterial& InMaterialResource, const FMeshDrawingPolicyOverrideSettings& InOverrideSettings, ERHIFeatureLevel::Type InFeatureLevel)
//	:FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, DVSM_None)
//{
//	VertexShader = InMaterialResource.GetShader<TTressFX_ShortCutVS<false>>(VertexFactory->GetType());
//	PixelShader = InMaterialResource.GetShader<FTressFX_FillColorPS>(InVertexFactory->GetType());
//}
//
//void FTressFXFillColorDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FViewInfo* View, const ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState) const
//{
//	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, View->ViewUniformBuffer, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated, DrawRenderState);
//	PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, View->ViewUniformBuffer, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated, DrawRenderState);
//}
//
//FBoundShaderStateInput FTressFXFillColorDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
//{
//	return FBoundShaderStateInput(
//		FMeshDrawingPolicy::GetVertexDeclaration(),
//		VertexShader->GetVertexShader(),
//		nullptr,
//		nullptr,
//		PixelShader->GetPixelShader(),
//		NULL);
//}
//
//void FTressFXFillColorDrawingPolicy::SetMeshRenderState(FRHICommandList& RHICmdList, const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMeshBatch& Mesh, int32 BatchElementIndex, const FDrawingPolicyRenderState& DrawRenderState) const
//{
//	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
//	VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh, BatchElement, DrawRenderState);
//	PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
//}
//
//void FTressFXFillColorDrawingPolicy::SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View)
//{
//
//	DrawRenderState.SetBlendState(
//		TStaticBlendState<
//		CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI(), FLinearColor::Transparent, 0x000000ff);
//
//	DrawRenderState.SetDepthStencilState(
//		TStaticDepthStencilState<
//		true, CF_GreaterEqual,
//		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
//		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
//		0xff, 0xff, DWM_Zero
//		>::GetRHI(), 0x00);
//}

//FRasterizerStateRHIParamRef FTressFXFillColorDrawingPolicy::ComputeRasterizerState(EDrawingPolicyOverrideFlags InOverrideFlags) const
//{
//	return TStaticRasterizerState<FM_Solid, CM_CW, false, true, true>::GetRHI();
//}
//
//bool FTressFXFillColorDrawingPolicyFactory::DrawDynamicMesh(FRHICommandList& RHICmdList, const FViewInfo& View, ContextType DrawingContext, const FMeshBatch& Mesh, bool bPreFog, const FDrawingPolicyRenderState& DrawRenderState, const FPrimitiveSceneProxy* PrimitiveSceneProxy, FHitProxyId HitProxyId)
//{
//	if (PrimitiveSceneProxy)
//	{
//		SCOPED_DRAW_EVENT(RHICmdList, TressFXShortCutFillColor);
//		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
//		const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
//		FTressFXFillColorDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), ComputeMeshOverrideSettings(Mesh), View.GetFeatureLevel());
//
//		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
//		DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
//		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
//		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
//		DrawingPolicy.SetSharedState(RHICmdList, &View, FTressFXFillColorDrawingPolicy::ContextDataType(), DrawRenderStateLocal);
//
//		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
//		{
//			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal);
//			SCOPED_DRAW_EVENTF(RHICmdList, TressFXDrawMeshFillColor, TEXT("Asset  %s"), PrimitiveSceneProxy->GetOwnerName());
//			DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex);
//		}
//		return true;
//	}
//	return false;
//}
extern void DrawRectangle(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	FShader* VertexShader,
	EDrawRectangleFlags Flags,
	uint32 InstanceCount
);
//void GatherPPLL(FRHICommandList& RHICmdList, const FViewInfo& View)
//{
//	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
//
//	TShaderMapRef<FScreenVS>							ScreenVertexShader(View.ShaderMap);
//	TShaderMapRef<FTressFXPPLLGather2_PS>				PixelShader(View.ShaderMap);
//	static FGlobalBoundShaderState BoundState;
//
//	FRHIRenderTargetView RTV(SceneContext.GetSceneColorSurface(), ERenderTargetLoadAction::ELoad);
//
//	SetRenderTargets(RHICmdList, 1, &RTV.Texture, SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, 0, nullptr);
//
//	FGraphicsPipelineStateInitializer PSOInitializer;
//
//	RHICmdList.ApplyCachedRenderTargets(PSOInitializer);
//
//	PSOInitializer.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, false, false>::GetRHI();
//
//	FBlendStateInitializerRHI::FRenderTarget ResolveColor_BS;
//
//	PSOInitializer.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_Zero,
//		CW_ALPHA>::GetRHI();
//	PSOInitializer.BlendFactor = FLinearColor::Transparent;
//	PSOInitializer.SampleMask = 0xffffffff;
//
//
//
//
//
//	PSOInitializer.DepthStencilState =
//		TStaticDepthStencilState<
//		false, CF_Greater,
//		true, CF_Less, SO_Keep, SO_Keep, SO_Keep,
//		true, CF_Less, SO_Keep, SO_Keep, SO_Keep,
//		0xff, 0x00, DWM_All, true>::GetRHI();
//
//	PSOInitializer.PrimitiveType = PT_TriangleList;
//
//
//	PSOInitializer.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
//	PSOInitializer.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
//	PSOInitializer.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
//
//
//	SetGraphicsPipelineState(RHICmdList, PSOInitializer);
//
//	ScreenVertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
//	PixelShader->SetShaderParams(RHICmdList, View, SceneContext.PPLLNodes.SRV, SceneContext.PPLLHeads->GetRenderTargetItem().ShaderResourceTexture);
//
//
//
//	const auto Size = View.ViewRect.Size();
//
//	DrawRectangle(
//		RHICmdList,
//		0, 0,
//		Size.X, Size.Y,
//		0, 0,
//		Size.X, Size.Y,
//		Size,
//		Size,
//		*ScreenVertexShader,
//		EDRF_Default,
//		1
//	);
//
//}

bool FSceneRenderer::ShouldRenderTressFX(int32 TressFXPass)
{
	if (TressFXPass < 0 || TressFXPass > ETressFXPass::Max)
	{
		return false;
	}

	if (!Scene->World || (Scene->World->WorldType != EWorldType::EditorPreview && Scene->World->WorldType != EWorldType::Inactive))
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (View.TressFXMeshBatches.Num() > 0)
			{
				return true;
			}
		}
	}
	return false;
}

void RenderShortcutBasePass(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);


	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1)
		{
			continue;
		}

		SCOPED_DRAW_EVENT(RHICmdList, TressFXShortcut_BasePass);

		{
			// Make a copy of scene depth for us to use
			SCOPED_DRAW_EVENT(RHICmdList, CopySceneDepth);
			TressFXCopySceneDepth(RHICmdList, View, SceneContext, SceneContext.TressFXSceneDepth);
		}

		
		// shortcut pass 1, accumulate alpha, depths, and optionally velocity if the material wants it
		{
			SCOPED_DRAW_EVENT(RHICmdList, DepthsAlpha);

			static const uint32 ShortcutClearValue[4] = { SHORTCUT_INITIAL_DEPTH ,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH };
			ClearUAV(RHICmdList, SceneContext.FragmentDepthsTexture->GetRenderTargetItem(), ShortcutClearValue);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			FRHIRenderTargetView ColorTargetsRTV[2];

			ColorTargetsRTV[0] = FRHIRenderTargetView(SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad);
			ColorTargetsRTV[1] = FRHIRenderTargetView(SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad);
			FRHIDepthRenderTargetView DepthRTV(SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
			FUnorderedAccessViewRHIParamRef UAVs[8] = { SceneContext.FragmentDepthsTexture->GetRenderTargetItem().UAV,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr };
			RHICmdList.SetRenderTargets(2, ColorTargetsRTV, &DepthRTV, 1, UAVs);
			RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToGfx, UAVs, ARRAY_COUNT(UAVs));
			FLinearColor ClearColors[] = { FLinearColor::White, FLinearColor::Transparent };
			DrawClearQuadMRT( RHICmdList, true, 2, ClearColors, false, 0, false, 0);
			FMeshPassProcessorRenderState DrawRenderState(View);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_DepthsAlpha].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UAVs, 1);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture);
		}
		// shortcut pass 2, depths resolve
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveDepths);

			SetRenderTarget(RHICmdList, NULL, SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture);

			TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
			TShaderMapRef<FTressFXShortCut_ResolveDepthPS>		PixelShader(View.ShaderMap);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
			PixelShader->SetParameters(RHICmdList, View, SceneContext);

			const FIntPoint Size = View.ViewRect.Size();
			RHICmdList.SetViewport(0, 0, 0, Size.X, Size.Y, 1);
			DrawRectangle(
				RHICmdList,
				0, 0,
				Size.X, Size.Y,
				0, 0,
				Size.X, Size.Y,
				Size,
				Size,
				*VertexShader,
				EDRF_Default,
				1
			);
		}

		const bool CopyHairDepthToSceneDepth = true;
		if(CopyHairDepthToSceneDepth)
		{
			SCOPED_DRAW_EVENT(RHICmdList, TressFXSCopyHairDepthToSceneDepth);

			FTextureRHIParamRef Resources[1] = {
				SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources, 1);

			TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
			TShaderMapRef<FTressFXCopyOpaqueDepthPS>			PixelShader(View.ShaderMap);

			SetRenderTarget(RHICmdList, NULL, SceneContext.GetSceneDepthSurface());

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
			PixelShader->SetParameters(RHICmdList, View, SceneContext.AccumInvAlpha, SceneContext.TressFXSceneDepth);

			const FIntPoint Size = View.ViewRect.Size();
			RHICmdList.SetViewport(0, 0, 0, Size.X, Size.Y, 1);
			DrawRectangle(
				RHICmdList,
				0, 0,
				Size.X, Size.Y,
				0, 0,
				Size.X, Size.Y,
				Size,
				Size,
				*VertexShader,
				EDRF_Default,
				1
			);
		}
	}
}

void FSceneRenderer::RenderTressFXROVPass(FRHICommandListImmediate& RHICmdList)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1)
		{
			continue;
		}

		TUniformBufferRef<FOpaqueBasePassUniformParameters> BasePassUniformBuffer;
		// this should include forward lighting buffer i think, if not will need to bind manually
		CreateOpaqueBasePassUniformBuffer(RHICmdList, View, nullptr, BasePassUniformBuffer);
		FMeshPassProcessorRenderState DrawRenderState(View, BasePassUniformBuffer);
		
		if (View.ShouldRenderView())
		{
			Scene->UniformBuffers.UpdateViewUniformBuffer(View);

			//do this type of thing.
			//FMeshPassProcessorRenderState DrawRenderState(InDrawRenderState);
			//SetupBasePassView(RHICmdList, View, this);
			//View.ParallelMeshDrawCommandPasses[EMeshPass::BasePass].DispatchDraw(nullptr, RHICmdList);

		}
	}
}

void RenderShortcutResolvePass(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SCOPED_DRAW_EVENT(RHICmdList, TressFXShortcut_Resolve);

	//for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	//{
	//	FViewInfo& View = Views[ViewIndex];

	//	if (View.TressFXMeshBatches.Num() < 1)
	//	{
	//		continue;
	//	}


	//	// shortcut pass 3, fill colors
	//	{
	//		SCOPED_DRAW_EVENT(RHICmdList, FillColors);

	//		TUniformBufferRef<FOpaqueBasePassUniformParameters> BasePassUniformBuffer;
	//		CreateOpaqueBasePassUniformBuffer(RHICmdList, View, nullptr, BasePassUniformBuffer);
	//		FMeshPassProcessorRenderState DrawRenderState(View, BasePassUniformBuffer);

	//		FUnorderedAccessViewRHIParamRef UAVs[7] = { nullptr,nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	//		//FUAVCountInitializerRHI UAVCounts(0, 0, 0, 0, 0, 0, 0, 0);

	//		{
	//			FTextureRHIParamRef RenderTargets[] = {
	//				SceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture
	//			};
	//			SetRenderTargets(RHICmdList, 1, RenderTargets, SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, 6, UAVs, false);
	//		}

	//		DrawClearQuad(RHICmdList, true, FLinearColor::Transparent, false, 0, false, 0);

	//		{
	//			FTextureRHIParamRef RenderTargets[] = {
	//				SceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture,
	//				SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture,
	//				SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture,
	//				SceneContext.GBufferE->GetRenderTargetItem().TargetableTexture,
	//				SceneContext.GBufferD->GetRenderTargetItem().TargetableTexture,
	//				SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture,
	//				SceneContext.GetSceneColorSurface()
	//			};

	//			SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	//		}
	//		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	//		View.TressFXSet.DrawPrims(RHICmdList, View, DrawRenderState, false, ETressFXPass::TFXRU_FillColor);
	//	}

	//	{
	//		FTextureRHIParamRef RTVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	//		FUnorderedAccessViewRHIParamRef UAVs[2] = { nullptr,nullptr };
	//		SetRenderTargets(RHICmdList, ARRAY_COUNT(RTVs), RTVs, nullptr, 2, UAVs);
	//	}
	//	// shortcut pass 4: resolve color
	//	{
	//		SCOPED_DRAW_EVENT(RHICmdList, ResolveColors);

	//		FTextureRHIParamRef Resources[2] = {
	//			SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture,
	//			SceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture
	//		};
	//		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources,2);

	//		FRHIDepthRenderTargetView DepthRTV(
	//			SceneContext.GetSceneDepthSurface(),
	//			ERenderTargetLoadAction::ENoAction,
	//			ERenderTargetStoreAction::ENoAction
	//		);
	//		FRHIRenderTargetView ColorRTV(
	//			SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture,
	//			ERenderTargetLoadAction::ELoad
	//		);

	//		RHICmdList.SetRenderTargets(1, &ColorRTV, &DepthRTV, 0, nullptr);

	//		TShaderMapRef<FPostProcessVS>						VertexShader(View.ShaderMap);
	//		TShaderMapRef<FTressFXShortCut_ResolveColorPS>		PixelShader(View.ShaderMap);

	//		FRenderingCompositePassContext Context(RHICmdList, View);

	//		Context.SetViewportAndCallRHI(View.ViewRect);

	//		FGraphicsPipelineStateInitializer GraphicsPSOInit;
	//		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	//		GraphicsPSOInit.BlendState = TStaticBlendState<
	//			CW_RGBA
	//			, BO_Add // color blend op
	//			, BF_One // color source blend
	//			, BF_SourceAlpha //color dest blend
	//			, BO_Add //alpha blend op
	//			, BF_Zero // alpha src blend
	//			, BF_Zero //alpha dest blend
	//		>::GetRHI();
	//		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	//		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
	//			false, CF_Always
	//		>::GetRHI();

	//		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	//		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	//		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	//		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	//		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	//		VertexShader->SetParameters(Context);
	//		PixelShader->SetParameters(Context.RHICmdList, View, SceneContext);

	//		DrawRectangle(
	//			RHICmdList,
	//			0, 0,
	//			View.ViewRect.Width(), View.ViewRect.Height(),
	//			View.ViewRect.Min.X, View.ViewRect.Min.Y,
	//			View.ViewRect.Width(), View.ViewRect.Height(),
	//			View.ViewRect.Size(),
	//			SceneContext.GetBufferSizeXY(),
	//			*VertexShader,
	//			EDRF_UseTriangleOptimization);
	//	}
	//}
}

void FSceneRenderer::RenderTressFXVelocitiesDepth(FRHICommandListImmediate& RHICmdList)
{
	if (!ShouldRenderTressFX((int32)ETressFXPass::DepthsVelocity))
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1)
		{
			continue;
		}
		// shouldnt be needed since we are piggy-backing off of the depth pass
		//Scene->UniformBuffers.UpdateViewUniformBuffer(View);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		{
			SCOPED_DRAW_EVENT(RHICmdList, TressFXDepthsVelocity);

			//Clear_Store to clear the TFX Velocity Target
			FRHIRenderPassInfo RPInfo(SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Clear_Store);
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_Store, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthSurface();
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXDepthsVelocity"));
			RHICmdList.BindClearMRTValues(true, false, false);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_DepthsVelocity].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.EndRenderPass();
		}
	}
}

void FSceneRenderer::RenderTressFXBasePass(FRHICommandListImmediate& RHICmdList)
{

	int32 TressFXType = CVarTressFXType.GetValueOnAnyThread();
	TressFXType = FMath::Clamp(TressFXType, 0, (int32)ETressFXRenderType::Max);

	SCOPED_DRAW_EVENT(RHICmdList, TressFXBasePass);

	switch (TressFXType)
	{
		case ETressFXRenderType::Opaque:
		{
			checkf(0, TEXT("RenderTressFXBasePass was callled, but render type is opaque! fix that"));
			break;
		}
		case ETressFXRenderType::ShortCut:
		{
			if (ShouldRenderTressFX(ETressFXPass::DepthsAlpha))
			{
				RenderShortcutBasePass(RHICmdList, Views);
			}
			break;
		}
		default:
		{
			check(0);
		}
	}
}

void FSceneRenderer::RenderTressfXResolvePass(FRHICommandListImmediate& RHICmdList)
{

	int32 TressFXType = CVarTressFXType.GetValueOnAnyThread();
	TressFXType = FMath::Clamp(TressFXType, 0, (int32)ETressFXRenderType::Max);

	SCOPED_DRAW_EVENT(RHICmdList, TressFXResolvePass);

	switch (TressFXType)
	{
		case ETressFXRenderType::Opaque:
		{
			checkf(0, TEXT("RenderTressfXResolvePass was callled, but render type is opaque! fix that"));
			break;
		}
		case ETressFXRenderType::ShortCut:
		{
			RenderShortcutResolvePass(RHICmdList, Views);
			break;
		}
		default:
		{
			check(0);
		}

	}
}

void FSceneRenderer::RenderTressFXResolveVelocity(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	if (!ShouldRenderTressFX(ETressFXPass::ResolveVelocity))
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	SCOPED_DRAW_EVENT(RHICmdList, TressFXVelocity);

	FTextureRHIParamRef Resources[2] = {
		SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture,
		SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture
	};
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources, 2);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, CM_None);
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

	TShaderMapRef<FScreenVS> VertexShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	TShaderMapRef<FTressFXResolveVelocityPs> PixelShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));

	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, EApplyRendertargetOption::ForceApply);

	PixelShader->SetParameters(RHICmdList, SceneContext.TressFXVelocity, SceneContext.TressFXSceneDepth);

	const FIntPoint Size = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
	RHICmdList.SetViewport(0, 0, 0, Size.X, Size.Y, 1);

	DrawRectangle(
		RHICmdList,
		0, 0,
		Size.X, Size.Y,
		0, 0,
		Size.X, Size.Y,
		Size,
		Size,
		*VertexShader
	);
}

FRegisterPassProcessorCreateFunction RegisterTRessFXDepthsVelocityPass(&CreateTRessFXDepthsVelocityPassProcessor, EShadingPath::Deferred, EMeshPass::TressFX_DepthsVelocity, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterTRessFXDepthsAlphaPass(&CreateTRessFXDepthsAlphaPassProcessor, EShadingPath::Deferred, EMeshPass::TressFX_DepthsAlpha, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
#pragma optimize("", on)