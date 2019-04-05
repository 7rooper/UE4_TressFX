
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
//#include "StaticMeshDrawList.h"
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

//extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
extern TAutoConsoleVariable<int32> CVarTressFXKBufferSize;
extern TAutoConsoleVariable<int32> CVarTressFXType;


//#pragma optimize("", off)

namespace TressFXRendering
{
	void TressFXSetupViews(TArray<FViewInfo>& Views)
	{
		//for (auto& View : Views)
		//{
		//	check(View.VisibleTressFX.Num() == 0);

		//	for (auto* PrimitiveInfo : View.PrimitiveViewRelevanceMap)
		//	{
		//		auto& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveInfo->GetIndex()];
		//		if (ViewRelevance.bTressFX)
		//		{
		//			View.VisibleTressFX.Add(PrimitiveInfo);
		//		}
		//	}
		//}
	}
}

void TressFXCopySceneDepth(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext, TRefCountPtr<IPooledRenderTarget> Destination)
{
	FRHISetRenderTargetsInfo RenderTargetsInfo(
		0,
		nullptr,
		FRHIDepthRenderTargetView(
			Destination->GetRenderTargetItem().TargetableTexture,
			ERenderTargetLoadAction::EClear,
			ERenderTargetStoreAction::ENoAction
		)
	);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Destination->GetRenderTargetItem().TargetableTexture);
	RHICmdList.SetRenderTargetsAndClear(RenderTargetsInfo);

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
}

//////////////////////////////////////////////////////////////////////////
//FTRessFXDepthsVelocityPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

void FTRessFXDepthsVelocityPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	FShaderPipeline* ShaderPipeline = nullptr;

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	//SetDepthPassDitheredLODTransitionState(ViewIfDynamicMeshCommand, MeshBatch, StaticMeshId, DrawRenderState);

	FTressFXShaderElementData ShaderElementData(ETressFXRenderUsage::TFXRU_DepthsVelocity, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	TMeshProcessorShaders<
		TTressFX_ShortCutVS<true>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFX_VelocityDepthPS<true>> TFXShaders;
	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFX_VelocityDepthPS<true>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<TTressFX_ShortCutVS<true>>(VertexFactory->GetType());

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

void FTRessFXDepthsVelocityPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	const bool bDraw = true;

	if (bDraw)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		const EBlendMode BlendMode = Material.GetBlendMode();
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, Material);
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);	
		const FMaterialRenderProxy& DefaultProxy = *UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		const FMaterial& DefaultMaterial = *DefaultProxy.GetMaterial(FeatureLevel);
		Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, DefaultProxy, DefaultMaterial, MeshFillMode, MeshCullMode);
	}
}

FTRessFXDepthsVelocityPassMeshProcessor::FTRessFXDepthsVelocityPassMeshProcessor(
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
	return new(FMemStack::Get()) FTRessFXDepthsVelocityPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, TRessFXDepthsVelocityPassState, InDrawListContext);
}

/*-----------------------------------------------------------------------------
FTressFXPrimSet
-----------------------------------------------------------------------------*/

//bool FTressFXPrimSet::DrawPrims(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FMeshPassProcessorRenderState& DrawRenderState, bool bWriteCustomStencilValues, ETressFXRenderUsage UsageType)
//{

	//bool bDirty = false;

	//if (Prims.Num())
	//{
	//	for (int32 PrimIdx = 0; PrimIdx < Prims.Num(); PrimIdx++)
	//	{
	//		FPrimitiveSceneProxy* PrimitiveSceneProxy = Prims[PrimIdx];
	//		const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

	//		if (View.PrimitiveVisibilityMap[PrimitiveSceneInfo->GetIndex()])
	//		{
	//			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneInfo->GetIndex()];
	//			FMeshPassProcessorRenderState DrawRenderStateLocal(DrawRenderState);
	//			{
	//				FInt32Range range = View.GetDynamicMeshElementRange(PrimitiveSceneInfo->GetIndex());

	//				for (int32 MeshBatchIndex = range.GetLowerBoundValue(); MeshBatchIndex < range.GetUpperBoundValue(); MeshBatchIndex++)
	//				{
	//					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

	//					checkSlow(MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneInfo->Proxy);

	//					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
	//					
	//					//UE_LOG(TressFXRenderingLog, Log, TEXT("TressFX Proxy owner name:  %s"), *PrimitiveSceneProxy->GetOwnerName().ToString());
	//					//UE_LOG(TressFXRenderingLog, Log, TEXT("TressFX Proxy resource name:  %s"), *PrimitiveSceneProxy->GetResourceName().ToString());

	//					switch (UsageType)
	//					{
	//					case ETressFXRenderUsage::TFXRU_DepthsVelocity:
	//						FTressFXDepthsVelocityDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, FTressFXDepthsVelocityDrawingPolicyFactory::ContextType(), MeshBatch, true, DrawRenderStateLocal, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
	//						break;
	//					case ETressFXRenderUsage::TFXRU_PPLL:
	//						//FTressFXPPLLDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, FTressFXPPLLDrawingPolicyFactory::ContextType(), MeshBatch, true, DrawRenderStateLocal, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
	//						break;
	//					case ETressFXRenderUsage::TFXRU_DepthsAlpha:
	//						FTressFXDepthsAlphaDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, FTressFXDepthsAlphaDrawingPolicyFactory::ContextType(), MeshBatch, true, DrawRenderStateLocal, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
	//						break;
	//					case ETressFXRenderUsage::TFXRU_FillColor:
	//						//FTressFXFillColorDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, FTressFXFillColorDrawingPolicyFactory::ContextType(), MeshBatch, true, DrawRenderStateLocal, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
	//						break;
	//					default:
	//						break;
	//					}

	//					bDirty = true;

	//				}
	//			}
	//		}
	//	}
	//}
	//return bDirty;
//}

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

//FTressFXDepthsAlphaDrawingPolicy<true>::FTressFXDepthsAlphaDrawingPolicy(const FVertexFactory* InVertexFactory, const FMaterialRenderProxy* InMaterialRenderProxy, const FMaterial& InMaterialResource, const FMeshDrawingPolicyOverrideSettings& InOverrideSettings, ERHIFeatureLevel::Type InFeatureLevel)
//	:FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, DVSM_None)
//{
//	VertexShader = InMaterialResource.GetShader<TTressFX_ShortCutVS<true>>(VertexFactory->GetType());
//	PixelShader = InMaterialResource.GetShader<FTressFX_DepthsAlphaPS<true>>(InVertexFactory->GetType());
//}
//
//FTressFXDepthsAlphaDrawingPolicy<false>::FTressFXDepthsAlphaDrawingPolicy(const FVertexFactory* InVertexFactory, const FMaterialRenderProxy* InMaterialRenderProxy, const FMaterial& InMaterialResource, const FMeshDrawingPolicyOverrideSettings& InOverrideSettings, ERHIFeatureLevel::Type InFeatureLevel)
//	:FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, DVSM_None)
//{
//	VertexShader = InMaterialResource.GetShader<TTressFX_ShortCutVS<false>>(VertexFactory->GetType());
//	PixelShader = InMaterialResource.GetShader<FTressFX_DepthsAlphaPS<false>>(InVertexFactory->GetType());
//}

//bool FTressFXDepthsVelocityDrawingPolicyFactory::DrawDynamicMesh(FRHICommandList & RHICmdList, const FSceneView & View, ContextType DrawingContext, const FMeshBatch & Mesh, bool bPreFog, const FMeshPassProcessorRenderState & DrawRenderState, const FPrimitiveSceneProxy * PrimitiveSceneProxy, FHitProxyId HitProxyId)
//{
	//if (PrimitiveSceneProxy)
	//{
	//	SCOPED_DRAW_EVENT(RHICmdList, TressFXDepthsVelocityPass);
	//	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;

	//	const bool bWantsVelocity = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel())->TressFXShouldRenderVelocity();
	//	EBlendMode BlendMode = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel())->GetBlendMode();

	//	const bool bNoDepth = (BlendMode == EBlendMode::BLEND_Translucent || BlendMode == EBlendMode::BLEND_Additive || BlendMode == EBlendMode::BLEND_AlphaComposite);

	//	if(bNoDepth && !bWantsVelocity)
	//	{
	//		return false;
	//	}
	//	if (bWantsVelocity)
	//	{
	//		FTressFXDepthsVelocityDrawingPolicy<true> DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), ComputeMeshOverrideSettings(Mesh), View.GetFeatureLevel(), bNoDepth);

	//		FMeshPassProcessorRenderState DrawRenderStateLocal(DrawRenderState);
	//		//DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
	//		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
	//		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
	//		DrawingPolicy.SetSharedState(RHICmdList, &View, FTressFXDepthsVelocityDrawingPolicy<true>::ContextDataType(), DrawRenderStateLocal);

	//		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
	//		{
	//			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal);
	//			SCOPED_DRAW_EVENTF(RHICmdList, TressFXDrawMeshDepthsVelocity, TEXT("Asset  %s"), *PrimitiveSceneProxy->GetOwnerName().ToString());
	//			DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex);
	//		}
	//		return true;
	//	}
	//	else
	//	{
	//		FTressFXDepthsVelocityDrawingPolicy<false> DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), ComputeMeshOverrideSettings(Mesh), View.GetFeatureLevel(), bNoDepth);
	//		FMeshPassProcessorRenderState DrawRenderStateLocal(DrawRenderState);
	//		//DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
	//		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
	//		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
	//		DrawingPolicy.SetSharedState(RHICmdList, &View, FTressFXDepthsVelocityDrawingPolicy<false>::ContextDataType(), DrawRenderStateLocal);

	//		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
	//		{
	//			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal);
	//			SCOPED_DRAW_EVENTF(RHICmdList, TressFXDrawMeshDepthsVelocity, TEXT("Asset  %s"), *PrimitiveSceneProxy->GetOwnerName().ToString());
	//			DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex);
	//		}
	//		return true;
	//	}		
	//}
	//return false;
//}

//bool FTressFXDepthsAlphaDrawingPolicyFactory::DrawDynamicMesh(FRHICommandList & RHICmdList, const FSceneView & View, ContextType DrawingContext, const FMeshBatch & Mesh, bool bPreFog, const FMeshPassProcessorRenderState & DrawRenderState, const FPrimitiveSceneProxy * PrimitiveSceneProxy, FHitProxyId HitProxyId)
//{
	//if (PrimitiveSceneProxy)
	//{

	//	SCOPED_DRAW_EVENT(RHICmdList, TressFXShortCutDepthsAlphaPass);
	//	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	//	const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());

	//	//velocity expensive, only render if material wants it
	//	const bool bNeedsVelocity = Material->TressFXShouldRenderVelocity();
	//	if(bNeedsVelocity)
	//	{
	//		FTressFXDepthsAlphaDrawingPolicy<true> DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), ComputeMeshOverrideSettings(Mesh), View.GetFeatureLevel());

	//		FMeshPassProcessorRenderState DrawRenderStateLocal(DrawRenderState);
	//		//DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
	//		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
	//		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
	//		DrawingPolicy.SetSharedState(RHICmdList, &View, FTressFXDepthsAlphaDrawingPolicy<true>::ContextDataType(), DrawRenderStateLocal);

	//		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
	//		{
	//			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal);
	//			SCOPED_DRAW_EVENTF(RHICmdList, TressFXDrawMeshDepthsAlpha, TEXT("Asset  %s"), *PrimitiveSceneProxy->GetOwnerName().ToString());
	//			DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex);
	//		}
	//		return true;
	//	}
	//	else
	//	{
	//		FTressFXDepthsAlphaDrawingPolicy<false> DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), ComputeMeshOverrideSettings(Mesh), View.GetFeatureLevel());

	//		FMeshPassProcessorRenderState DrawRenderStateLocal(DrawRenderState);
	//		DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
	//		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
	//		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
	//		DrawingPolicy.SetSharedState(RHICmdList, &View, FTressFXDepthsAlphaDrawingPolicy<false>::ContextDataType(), DrawRenderStateLocal);

	//		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
	//		{
	//			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal);
	//			SCOPED_DRAW_EVENTF(RHICmdList, TressFXDrawMeshDepthsAlpha, TEXT("Asset  %s"), *PrimitiveSceneProxy->GetOwnerName().ToString());
	//			DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex);
	//		}
	//		return true;
	//	}
	//}
	//return false;
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

bool FSceneRenderer::ShouldRenderTressFX()
{
	int32 TressFXType = CVarTressFXType.GetValueOnAnyThread();
	if (TressFXType <= 0)
	{
		return false;
	}
	TressFXType = TressFXType > 2 ? 2 : TressFXType;

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
	//for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	//{
	//	FViewInfo& View = Views[ViewIndex];

	//	if (View.TressFXMeshBatches.Num() < 1)
	//	{
	//		continue;
	//	}

	//	SCOPED_DRAW_EVENT(RHICmdList, TressFXShortcut_BasePass);

	//	{
	//		// Make a copy of scene depth for us to use
	//		SCOPED_DRAW_EVENT(RHICmdList, CopySceneDepth);
	//		TressFXCopySceneDepth(RHICmdList, View, SceneContext, SceneContext.TressFXSceneDepth);
	//	}

	//	static const uint32 ShortcutClearValue[4] = { SHORTCUT_INITIAL_DEPTH ,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH };
	//	ClearUAV(RHICmdList, SceneContext.FragmentDepthsTexture->GetRenderTargetItem(), ShortcutClearValue);
	//	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	//	// shortcut pass 1, accumulate alpha, depths, and optionally velocity if using temporal aa
	//	{
	//		SCOPED_DRAW_EVENT(RHICmdList, DepthsAlpha);

	//		FRHIRenderTargetView ColorTargetsRTV[2];

	//		ColorTargetsRTV[0] = FRHIRenderTargetView(SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad);
	//		ColorTargetsRTV[1] = FRHIRenderTargetView(SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad);
	//		FRHIDepthRenderTargetView DepthRTV(SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
	//		FUnorderedAccessViewRHIParamRef UAVs[8] = { SceneContext.FragmentDepthsTexture->GetRenderTargetItem().UAV,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr };
	//		//FUAVCountInitializerRHI UAVCountInitializer(0, 0, 0, 0, 0, 0, 0, 0);
	//		RHICmdList.SetRenderTargets(2, ColorTargetsRTV, &DepthRTV, 1, UAVs);
	//		RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToGfx, UAVs, ARRAY_COUNT(UAVs));
	//		FLinearColor ClearColors[] = { FLinearColor::White, FLinearColor::Transparent };
	//		DrawClearQuadMRT( RHICmdList, true, 2, ClearColors, false, 0, false, 0);
	//		FMeshPassProcessorRenderState DrawRenderState(View);
	//		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	//		View.TressFXSet.DrawPrims(RHICmdList, View, DrawRenderState, false, ETressFXRenderUsage::TFXRU_DepthsAlpha);
	//		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UAVs, 1);
	//		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture);
	//	}
	//	// shortcut pass 2, depths resolve
	//	{
	//		SCOPED_DRAW_EVENT(RHICmdList, ResolveDepths);

	//		SetRenderTarget(RHICmdList, NULL, SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture);

	//		TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
	//		TShaderMapRef<FTressFXShortCut_ResolveDepthPS>		PixelShader(View.ShaderMap);

	//		FGraphicsPipelineStateInitializer GraphicsPSOInit;
	//		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	//		GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
	//		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	//		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

	//		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	//		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	//		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	//		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	//		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	//		VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
	//		PixelShader->SetParameters(RHICmdList, View, SceneContext);

	//		const FIntPoint Size = View.ViewRect.Size();
	//		RHICmdList.SetViewport(0, 0, 0, Size.X, Size.Y, 1);
	//		DrawRectangle(
	//			RHICmdList,
	//			0, 0,
	//			Size.X, Size.Y,
	//			0, 0,
	//			Size.X, Size.Y,
	//			Size,
	//			Size,
	//			*VertexShader,
	//			EDRF_Default,
	//			1
	//		);
	//	}

	//	const bool CopyHairDepthToSceneDepth = true;
	//	if(CopyHairDepthToSceneDepth)
	//	{
	//		SCOPED_DRAW_EVENT(RHICmdList, TressFXSCopyHairDepthToSceneDepth);

	//		FTextureRHIParamRef Resources[1] = {
	//			SceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture
	//		};
	//		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources, 1);

	//		TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
	//		TShaderMapRef<FTressFXCopyOpaqueDepthPS>			PixelShader(View.ShaderMap);

	//		SetRenderTarget(RHICmdList, NULL, SceneContext.GetSceneDepthSurface());

	//		FGraphicsPipelineStateInitializer GraphicsPSOInit;
	//		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	//		GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
	//		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	//		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

	//		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	//		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	//		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	//		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	//		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	//		VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
	//		PixelShader->SetParameters(RHICmdList, View, SceneContext.AccumInvAlpha, SceneContext.TressFXSceneDepth);

	//		const FIntPoint Size = View.ViewRect.Size();
	//		RHICmdList.SetViewport(0, 0, 0, Size.X, Size.Y, 1);
	//		DrawRectangle(
	//			RHICmdList,
	//			0, 0,
	//			Size.X, Size.Y,
	//			0, 0,
	//			Size.X, Size.Y,
	//			Size,
	//			Size,
	//			*VertexShader,
	//			EDRF_Default,
	//			1
	//		);
	//	}
	//}
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
	//		View.TressFXSet.DrawPrims(RHICmdList, View, DrawRenderState, false, ETressFXRenderUsage::TFXRU_FillColor);
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
	if (!ShouldRenderTressFX())
	{
		return;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	/*for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1)
		{
			continue;
		}

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		{
			SCOPED_DRAW_EVENT(RHICmdList, TressFXDepthsVelocity);

			FRHIRenderTargetView ColorTargetsRTV[1];
			ColorTargetsRTV[0] = FRHIRenderTargetView(SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad);

			FRHIDepthRenderTargetView DepthRTV(SceneContext.GetSceneDepthSurface(), ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
			RHICmdList.SetRenderTargets(1, ColorTargetsRTV, &DepthRTV, 0, nullptr);
			DrawClearQuad(RHICmdList, true, FLinearColor::Transparent, false, 0, false,0);

			FMeshPassProcessorRenderState DrawRenderState(View);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.TressFXSet.DrawPrims(RHICmdList, View, DrawRenderState, false, ETressFXRenderUsage::TFXRU_DepthsVelocity);
		}
	}*/
}

void FSceneRenderer::RenderTressFXBasePass(FRHICommandListImmediate& RHICmdList)
{
	if (!ShouldRenderTressFX())
	{
		return;
	}

	int32 TressFXType = CVarTressFXType.GetValueOnAnyThread();
	TressFXType = TressFXType > 2 ? 2 : TressFXType;

	SCOPED_DRAW_EVENT(RHICmdList, TressFXBasePass);

	switch (TressFXType)
	{
		case 1:
		{
			//todo
			break;
		}
		case 2:
		{
			RenderShortcutBasePass(RHICmdList, Views);
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
	if(!ShouldRenderTressFX())
	{
		return;
	}

	int32 TressFXType = CVarTressFXType.GetValueOnAnyThread();
	TressFXType = TressFXType > 2 ? 2 : TressFXType;

	SCOPED_DRAW_EVENT(RHICmdList, TressFXResolvePass);

	switch (TressFXType)
	{
		case 1:
		{
			//todo
			break;
		}
		case 2:
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
	if (!ShouldRenderTressFX())
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

//#pragma optimize("", on)