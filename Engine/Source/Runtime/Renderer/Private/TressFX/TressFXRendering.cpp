
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
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"

DEFINE_LOG_CATEGORY(TressFXRenderingLog);

extern int32 GTressFXRenderType;

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
		FTressFX_VS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFX_VelocityDepthPS<bCalcVelocity>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFX_VelocityDepthPS<bCalcVelocity>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFX_VS<bCalcVelocity>>(VertexFactory->GetType());

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
	int32 TFXRenderType = static_cast<uint32>(GTressFXRenderType);
	TFXRenderType = FMath::Clamp(TFXRenderType, 0, (int32)ETressFXRenderType::Max);
	const bool bNoDepth = bIsTranslucent || TFXRenderType == ETressFXRenderType::KBuffer;

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

	FTressFXShaderElementData ShaderElementData(ETressFXPass::DepthsAlpha, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	TMeshProcessorShaders<
		FTressFX_VS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFX_DepthsAlphaPS<bCalcVelocity>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFX_DepthsAlphaPS<bCalcVelocity>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFX_VS<bCalcVelocity>>(VertexFactory->GetType());

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

//////////////////////////////////////////////////////////////////////////
//FTressFXFillColorPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

template<bool bIsShortcut>
void FTressFXFillColorPassMeshProcessor::Process(
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

	FTressFXShaderElementData ShaderElementData(ETressFXPass::FillColor, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	TMeshProcessorShaders<
		FTressFX_VS<false>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFX_FillColorPS<bIsShortcut>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFX_FillColorPS>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFX_VS<false>>(VertexFactory->GetType());

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

void FTressFXFillColorPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{

	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
	const FMaterial& MeshBatchMaterial = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

	const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

	const EBlendMode BlendMode = MeshBatchMaterial.GetBlendMode();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

	//todo
	bool bDraw = MeshBatchMaterial.IsUsedWithTressFX() && MeshBatch.bTressFX;

	if (bDraw)
	{
		//guess i dont really need the proxy for now, but i might add some more configurable render settings on it later
		const FTressFXSceneProxy* TFXProxy = ((const FTressFXSceneProxy*)(PrimitiveSceneProxy));
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, MeshBatchMaterial);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, MeshBatchMaterial);
		int32 TFXRenderType = static_cast<uint32>(GTressFXRenderType);
		TFXRenderType = FMath::Clamp(TFXRenderType, 0, (int32)ETressFXRenderType::Max);
		if (TFXRenderType == ETressFXRenderType::KBuffer) 
		{
			Process<false>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
		}
		else 
		{
			Process<true>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
		}


	}
}

FTressFXFillColorPassMeshProcessor::FTressFXFillColorPassMeshProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState = InPassDrawRenderState;
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.OpaqueBasePassUniformBuffer);
}

void SetupFillColorPassState(FMeshPassProcessorRenderState& DrawRenderState)
{

	DrawRenderState.SetBlendState(
		TStaticBlendState<
		CW_RGBA, 
		BO_Add,
		BF_One, 
		BF_One, 
		BO_Add, 
		BF_One, 
		BF_One
		>::GetRHI());
	DrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		0xff, 0xff, DWM_Zero
		>::GetRHI());
}

FMeshPassProcessor* CreateTRessFXFillColorPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	SetupFillColorPassState(PassDrawRenderState);
	return new(FMemStack::Get()) FTressFXFillColorPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext);
}

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

bool FSceneRenderer::GetAnyViewHasTressFX()
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		if (View.TressFXMeshBatches.Num() > 0)
		{
			return true;
		}
	}
	return false;
}

bool FSceneRenderer::ShouldRenderTressFX(int32 TressFXPass)
{
	if (TressFXPass < 0 || TressFXPass > ETressFXPass::Max)
	{
		return false;
	}

	if (!Scene->World || (Scene->World->WorldType != EWorldType::EditorPreview && Scene->World->WorldType != EWorldType::Inactive))
	{
		return true;
	}
	return false;
}

void RenderShortcutBasePass(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);


	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView())
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


			FUnorderedAccessViewRHIParamRef UAVs[] = { SceneContext.FragmentDepthsTexture->GetRenderTargetItem().UAV};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToGfx, UAVs, ARRAY_COUNT(UAVs));
			
			FRHITexture* ColorTargets[] = {
				SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture,
				SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture
			};

			FRHIRenderPassInfo RPInfo(ARRAY_COUNT(ColorTargets), ColorTargets, ERenderTargetActions::Load_Store);
			RPInfo.UAVIndex = 0;
			RPInfo.UAVs[RPInfo.NumUAVs++] = SceneContext.FragmentDepthsTexture->GetRenderTargetItem().UAV;			
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_Store, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture;
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXDepthsAlpha"));

			FLinearColor ClearColors[] = { FLinearColor::White, FLinearColor::Transparent };
			DrawClearQuadMRT( RHICmdList, true, 2, ClearColors, false, 0, false, 0);
			FMeshPassProcessorRenderState DrawRenderState(View);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_DepthsAlpha].DispatchDraw(nullptr, RHICmdList);

			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UAVs, ARRAY_COUNT(UAVs));
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture);
			RHICmdList.EndRenderPass();
		}
		// shortcut pass 2, depths resolve
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveDepths);

			FRHIRenderPassInfo RPInfo(
				SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, 
				EDepthStencilTargetActions::ClearDepthStencil_DontStoreDepthStencil
			);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXResolveDepths"));

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
			RHICmdList.EndRenderPass();
		}

		const bool CopyHairDepthToSceneDepth = true;
		
		if(CopyHairDepthToSceneDepth)
		{
			SCOPED_DRAW_EVENT(RHICmdList, TressFXCopyHairDepthToSceneDepth);

			FRHIRenderPassInfo RPInfo(
				SceneContext.GetSceneDepthSurface(),
				EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
			);

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXCopyHairDepthToSceneDepth"));
			
			FTextureRHIParamRef Resources[1] = {
				SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources, 1);

			TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
			TShaderMapRef<FTressFXCopyOpaqueDepthPS>			PixelShader(View.ShaderMap);

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
			RHICmdList.EndRenderPass();
		}
	}
}

void RenderShortcutResolvePass(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views, FScene* Scene, TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SCOPED_DRAW_EVENT(RHICmdList, TressFXShortcut_Resolve);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView())
		{
			continue;
		}


		// shortcut pass 3, fill colors
		{
			SCOPED_DRAW_EVENT(RHICmdList, FillColors);

			FRHIRenderPassInfo RPInfo(
				SceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_DontStore,
				SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
			);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXFillColor"));

			TUniformBufferRef<FOpaqueBasePassUniformParameters> BasePassUniformBuffer;

			//the basepass buffer on the scene gets updated inside CreateOpaqueBasePassUniformBuffer
			CreateOpaqueBasePassUniformBuffer(RHICmdList, View, ScreenShadowMaskTexture, BasePassUniformBuffer);
			FMeshPassProcessorRenderState DrawRenderState(View, BasePassUniformBuffer);// first arg is "PassUniformBuffer
			Scene->UniformBuffers.UpdateViewUniformBuffer(View);
			
			DrawClearQuad(RHICmdList,true, FLinearColor::Transparent, false, 0, false, 0);		
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_FillColors].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.EndRenderPass();
		}

		// shortcut pass 4: resolve color
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveColors);

			FTextureRHIParamRef Resources[2] = {
				SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture,
				SceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources,2);


			FRHIRenderPassInfo RPInfo(
				SceneContext.GetSceneColorSurface(), ERenderTargetActions::Load_DontStore,
				SceneContext.GetSceneDepthSurface(), EDepthStencilTargetActions::DontLoad_DontStore
			);

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXResolveColor"));

			TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
			TShaderMapRef<FTressFXShortCut_ResolveColorPS>		PixelShader(View.ShaderMap);

			FRenderingCompositePassContext Context(RHICmdList, View);

			Context.SetViewportAndCallRHI(View.ViewRect);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<
				CW_RGBA
				, BO_Add // color blend op
				, BF_One // color source blend
				, BF_SourceAlpha //color dest blend
				, BO_Add //alpha blend op
				, BF_Zero // alpha src blend
				, BF_Zero //alpha dest blend
			>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				false, CF_Always
			>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
			PixelShader->SetParameters(Context.RHICmdList, View, SceneContext);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Size(),
				SceneContext.GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
			RHICmdList.EndRenderPass();
		}
	}
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

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView())
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

void FSceneRenderer::RenderTressFXBasePass(FRHICommandListImmediate& RHICmdList, int32 TFXRenderType)
{

	SCOPED_DRAW_EVENT(RHICmdList, TressFXBasePass);

	switch (TFXRenderType)
	{
		case ETressFXRenderType::Opaque:
		{
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

void FSceneRenderer::RenderTressfXResolvePass(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture, int32 TFXRenderType)
{

	SCOPED_DRAW_EVENT(RHICmdList, TressFXResolvePass);

	switch (TFXRenderType)
	{
		case ETressFXRenderType::Opaque:
		{
			break;
		}
		case ETressFXRenderType::ShortCut:
		{
			RenderShortcutResolvePass(RHICmdList, Views, Scene, ScreenShadowMaskTexture);
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
FRegisterPassProcessorCreateFunction RegisterTRessFXFillColorPass(&CreateTRessFXFillColorPassProcessor, EShadingPath::Deferred, EMeshPass::TressFX_FillColors, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
#pragma optimize("", on)