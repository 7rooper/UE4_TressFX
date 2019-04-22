
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
extern int32 GTressFXKBufferSize;

/////////////////////////////////////////////////////////////////////////////////
//  FTressFXFillColorPS - Pixel shader for Third pass of shortcut, and PPLL build of kbuffer
////////////////////////////////////////////////////////////////////////////////

template <ETressFXPass::Type ColorPassType, int32 KBufferSize>
class FTressFXFillColorPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FTressFXFillColorPS, MeshMaterial)

public:

	FTressFXFillColorPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		tressfxShadeParameters.Bind(Initializer.ParameterMap, TEXT("tressfxShadeParameters"));
		BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);

		if (ColorPassType == ETressFXPass::FillColor_KBuffer)
		{
			RWFragmentListHead.Bind(Initializer.ParameterMap, TEXT("RWFragmentListHead"));
			RWLinkedListUAV.Bind(Initializer.ParameterMap, TEXT("RWLinkedListUAV"));
			RWCounterBuffer.Bind(Initializer.ParameterMap, TEXT("RWCounterBuffer"));
			KBufferUniformBuffer.Bind(Initializer.ParameterMap, FTressFXKBufferFillPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
		}
	}

	FTressFXFillColorPS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TFX_SHORTCUT"), ColorPassType == ETressFXPass::FillColor_Shortcut ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("TFX_PPLL"), ColorPassType == ETressFXPass::FillColor_KBuffer ? TEXT("1") : TEXT("0"));

		const bool bNeedsVelocity = ColorPassType == ETressFXPass::FillColor_KBuffer && Material->TressFXShouldRenderVelocity();
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bNeedsVelocity ? TEXT("1") : TEXT("0"));

		if (ColorPassType == ETressFXPass::FillColor_KBuffer)
		{
			OutEnvironment.SetDefine(TEXT("KBUFFER_SIZE"), KBufferSize);			
			FShaderUniformBufferParameter::ModifyCompilationEnvironment(
				FTressFXKBufferFillPassUniformParameters::StaticStructMetadata.GetShaderVariableName(),
				FTressFXKBufferFillPassUniformParameters::StaticStructMetadata,
				Platform,
				OutEnvironment
			);
		}
	}


	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FTressFXVertexFactory"), FNAME_Find)) && (Material->IsUsedWithTressFX() || Material->IsSpecialEngineMaterial()))
		{
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
		}

		return false;
	}


	virtual bool Serialize(FArchive& Ar) override
	{
		const bool result = FMeshMaterialShader::Serialize(Ar);
		Ar << tressfxShadeParameters;
		if (ColorPassType == ETressFXPass::FillColor_KBuffer)
		{
			Ar << RWFragmentListHead;
			Ar << RWLinkedListUAV;
			Ar << RWCounterBuffer;
			Ar << KBufferUniformBuffer;
		}
		return result;
	}


	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FTressFXShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		check(ColorPassType == ETressFXPass::FillColor_KBuffer || ColorPassType == ETressFXPass::FillColor_Shortcut)

		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FTressFXSceneProxy* TFXProxy = (const FTressFXSceneProxy*)PrimitiveSceneProxy;
		ShaderBindings.Add(tressfxShadeParameters, TFXProxy->TressFXHairObject->ShadeParametersUniformBuffer);

		if (ColorPassType == ETressFXPass::FillColor_KBuffer) 
		{
			//kbuffer unform buff is set as pass uniform buffer
			ShaderBindings.Add(KBufferUniformBuffer, DrawRenderState.GetPassUniformBuffer());
		}
	}

public:

	FShaderUniformBufferParameter tressfxShadeParameters;
	//Kbuffer
	FRWShaderParameter RWFragmentListHead;
	FShaderUniformBufferParameter KBufferUniformBuffer;
	FRWShaderParameter RWLinkedListUAV;
	FRWShaderParameter RWCounterBuffer;
};

typedef FTressFXFillColorPS<ETressFXPass::FillColor_Shortcut, 0> FTressFXFillColorPS_Shortcut;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXFillColorPS_Shortcut, TEXT("/Engine/Private/TressFXFillColorPS.usf"), TEXT("main"), SF_Pixel);


#define IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(KBufferSize) \
	typedef FTressFXFillColorPS<ETressFXPass::FillColor_KBuffer, KBufferSize> FTressFXFillColorPS_KBuffer##KBufferSize; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXFillColorPS_KBuffer##KBufferSize, TEXT("/Engine/Private/TressFXFillColorPS.usf"), TEXT("main"), SF_Pixel);

IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(2)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(3)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(4)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(5)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(6)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(7)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(8)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(9)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(10)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(11)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(12)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(13)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(14)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(15)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(16)

#undef IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS

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

#define PROCESS_DEPTHSVELOCITY(bCalcVelocity,TFXRenderType, ...)			\
	if(bCalcVelocity)														\
	{																		\
		Process<true, TFXRenderType>(__VA_ARGS__);							\
	}																		\
	else																	\
	{																		\
		Process<false, TFXRenderType>(__VA_ARGS__);							\
	}

#define PROCESS_DEPTHSVELOCITY2(bCalcVelocity,TFXRenderType, ...)																		\
switch (TFXRenderType)																													\
{																																		\
	case ETressFXRenderType::KBuffer:  PROCESS_DEPTHSVELOCITY(bCalcVelocity,ETressFXRenderType::KBuffer, __VA_ARGS__) break;			\
	case ETressFXRenderType::Opaque:  PROCESS_DEPTHSVELOCITY(bCalcVelocity,ETressFXRenderType::Opaque, __VA_ARGS__) break;				\
	case ETressFXRenderType::ShortCut:  PROCESS_DEPTHSVELOCITY(bCalcVelocity,ETressFXRenderType::ShortCut, __VA_ARGS__) break;			\
	default: check(0);																													\
}

template<bool bCalcVelocity, ETressFXRenderType::Type TFXRenderType>
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
		FTressFXVS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXVelocityDepthPS<bCalcVelocity, TFXRenderType>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFXVelocityDepthPS<bCalcVelocity, TFXRenderType>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFXVS<bCalcVelocity>>(VertexFactory->GetType());

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
		PROCESS_DEPTHSVELOCITY2(bWantsVelocity, TFXRenderType, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode, bNoDepth);
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

#undef PROCESS_DEPTHSVELOCITY
#undef PROCESS_DEPTHSVELOCITY2

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
		FTressFXVS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXDepthsAlphaPS<bCalcVelocity>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFXDepthsAlphaPS<bCalcVelocity>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFXVS<bCalcVelocity>>(VertexFactory->GetType());

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

template<int32 KBufferSize>
void FTressFXFillColorPassMeshProcessor::ProcessKBuffer(
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
	DrawRenderState.SetBlendState(
		TStaticBlendState<
		EColorWriteMask::CW_NONE
		>::GetRHI());
	DrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		0xff, 0xff, DWM_Zero
		>::GetRHI());

	FTressFXShaderElementData ShaderElementData(ETressFXPass::FillColor_KBuffer, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);
	TMeshProcessorShaders<
		FTressFXVS<false>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXFillColorPS<ETressFXPass::FillColor_KBuffer, KBufferSize>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFXFillColorPS<ETressFXPass::FillColor_KBuffer, KBufferSize>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFXVS<false>>(VertexFactory->GetType());

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

void FTressFXFillColorPassMeshProcessor::ProcessShortcut(
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

	FTressFXShaderElementData ShaderElementData(ETressFXPass::FillColor_Shortcut, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);
	TMeshProcessorShaders<
		FTressFXVS<false>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXFillColorPS<ETressFXPass::FillColor_Shortcut,0>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFXFillColorPS<ETressFXPass::FillColor_Shortcut,0>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFXVS<false>>(VertexFactory->GetType());

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



#define PROCESS_KBUFFER(KBufferSize, ...)	\
ProcessKBuffer<KBufferSize>(__VA_ARGS__);									

//there has to be a better way to do this, but im not clever enough

#define PROCESS_KBUFFER2(KBufferSize, ...)					\
switch (KBufferSize)										\
{															\
	case 4:  PROCESS_KBUFFER(4, __VA_ARGS__) break;			\
	case 5:  PROCESS_KBUFFER(5, __VA_ARGS__) break;			\
	case 6:  PROCESS_KBUFFER(6, __VA_ARGS__) break;			\
	case 7:  PROCESS_KBUFFER(7, __VA_ARGS__) break;			\
	case 8:  PROCESS_KBUFFER(8, __VA_ARGS__) break;			\
	case 9:  PROCESS_KBUFFER(9, __VA_ARGS__) break;			\
	case 10: PROCESS_KBUFFER(10, __VA_ARGS__) break;		\
	case 11: PROCESS_KBUFFER(11, __VA_ARGS__) break;		\
	case 12: PROCESS_KBUFFER(12, __VA_ARGS__) break;		\
	case 13: PROCESS_KBUFFER(13, __VA_ARGS__) break;		\
	case 14: PROCESS_KBUFFER(14, __VA_ARGS__) break;		\
	case 15: PROCESS_KBUFFER(15, __VA_ARGS__) break;		\
	case 16: PROCESS_KBUFFER(16, __VA_ARGS__) break;		\
	default: check(0);										\
}


template<ETressFXRenderType::Type RenderType>
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
	switch(RenderType)
	{
		case ETressFXRenderType::KBuffer: 
		{
			const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
			const FMaterial& MeshBatchMaterial = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

			int32 KBufferSize = FMath::Clamp(static_cast<int32>(GTressFXKBufferSize), MIN_TFX_KBUFFER_SIZE, MAX_TFX_KBUFFER_SIZE);
			PROCESS_KBUFFER2(KBufferSize, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode)			
			break;
		}
		case ETressFXRenderType::ShortCut:
		{
			ProcessShortcut(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				MaterialResource,
				MeshFillMode,
				MeshCullMode
			);
			break;
		}
		default: 
		{ 
			check(0); 
		}
	}	
}

#undef PROCESS_KBUFFER
#undef PROCESS_KBUFFER2

void FTressFXFillColorPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
	const FMaterial& MeshBatchMaterial = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

	const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

	const EBlendMode BlendMode = MeshBatchMaterial.GetBlendMode();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

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
			PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.TressFXKBufferPassUniformBuffer);
			Process<ETressFXRenderType::KBuffer>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
		}
		else 
		{
			PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.OpaqueBasePassUniformBuffer);
			Process<ETressFXRenderType::ShortCut>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
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
	//pass unfiform buffer is set during process
}

void SetupFillColorPassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	//set up is done during Process()
}

FMeshPassProcessor* CreateTRessFXFillColorPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	SetupFillColorPassState(PassDrawRenderState);
	return new(FMemStack::Get()) FTressFXFillColorPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext);
}

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


void RenderDepthsAndVelocity(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views, FScene* Scene, int32 TFXRenderType)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView())
		{
			continue;
		}

		Scene->UniformBuffers.UpdateViewUniformBuffer(View);



		if (TFXRenderType == ETressFXRenderType::KBuffer)
		{
			//copy scene depth, so we have a copy that doesnt have hair depths in it
			SCOPED_DRAW_EVENT(RHICmdList, TressFXCopySceneDepth);
			TressFXCopySceneDepth(RHICmdList, View, SceneContext, SceneContext.TressFXSceneDepth);
		}


		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		{
			SCOPED_DRAW_EVENT(RHICmdList, TressFXDepthsVelocity);

			//Clear_Store to clear the TFX Velocity Target
			FRHIRenderPassInfo RPInfo(SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Clear_Store);
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_Store, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthSurface();;
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXDepthsVelocity"));
			RHICmdList.BindClearMRTValues(true, false, false);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_DepthsVelocity].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.EndRenderPass();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//Shortcut Passes
/////////////////////////////////////////////////////////////////////////

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
			ClearUAV(RHICmdList, SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem(), ShortcutClearValue);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);


			FUnorderedAccessViewRHIParamRef UAVs[] = { SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem().UAV};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToGfx, UAVs, ARRAY_COUNT(UAVs));
			
			FRHITexture* ColorTargets[] = {
				SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().TargetableTexture,
				SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture
			};

			FRHIRenderPassInfo RPInfo(ARRAY_COUNT(ColorTargets), ColorTargets, ERenderTargetActions::Load_Store);
			RPInfo.UAVIndex = 0;
			RPInfo.UAVs[RPInfo.NumUAVs++] = SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem().UAV;			
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
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().TargetableTexture);
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
			TShaderMapRef<FTressFXShortCutResolveDepthPS>		PixelShader(View.ShaderMap);

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
				SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().TargetableTexture
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
			PixelShader->SetParameters(RHICmdList, View, SceneContext.TressFXAccumInvAlpha, SceneContext.TressFXSceneDepth);

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
				SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_DontStore,
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
				SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().TargetableTexture,
				SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().TargetableTexture
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources,2);


			FRHIRenderPassInfo RPInfo(
				SceneContext.GetSceneColorSurface(), ERenderTargetActions::Load_DontStore,
				SceneContext.GetSceneDepthSurface(), EDepthStencilTargetActions::DontLoad_DontStore
			);

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXResolveColor"));

			TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
			TShaderMapRef<FTressFXShortCutResolveColorPS>		PixelShader(View.ShaderMap);

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

//////////////////////////////////////////////////////////////////////////
//K-Buffer Passes
/////////////////////////////////////////////////////////////////////////


void RenderKbufferBasePass(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views, FScene* Scene)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	RenderDepthsAndVelocity(RHICmdList, Views, Scene, ETressFXRenderType::KBuffer);


}

void RenderKBufferResolvePasses(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views, FScene* Scene, TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView())
		{
			continue;
		}

		SCOPED_DRAW_EVENT(RHICmdList, TressFXKBufferPasses);

		TRefCountPtr<IPooledRenderTarget> TressFXKBufferListHeads;
		FRWBufferStructured* TressFXKBufferNodes = nullptr;
		FRWBuffer* TressFXKBufferCounter = nullptr;
		int32 TressFXKBufferNodePoolSize;
		SceneContext.GetTressFXKBufferResources(RHICmdList, TressFXKBufferListHeads, TressFXKBufferNodes, TressFXKBufferCounter, TressFXKBufferNodePoolSize);

		// Fill k-buffer
		{
			SCOPED_DRAW_EVENT(RHICmdList, FillKbuffer);
			
			const uint32 FragmentListClearValue[4] = { TFX_PPLL_NULL, TFX_PPLL_NULL, TFX_PPLL_NULL, TFX_PPLL_NULL };
			ClearUAV(RHICmdList, TressFXKBufferListHeads->GetRenderTargetItem(), FragmentListClearValue);
			TressFXKBufferNodes->AcquireTransientResource();
			TressFXKBufferCounter->AcquireTransientResource();	
			ClearUAV(RHICmdList, *TressFXKBufferNodes, 0);
			
			//starts at one so if it rolls over to 0 after max uint it wont try to add more
			static const uint32 Values[4] = { 1, 1, 1, 1 };
			RHICmdList.ClearTinyUAV(TressFXKBufferCounter->UAV, Values);

			FUnorderedAccessViewRHIParamRef UAVS[] = {
				TressFXKBufferListHeads->GetRenderTargetItem().UAV,
				TressFXKBufferNodes->UAV,
				TressFXKBufferCounter->UAV
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToGfx, UAVS, ARRAY_COUNT(UAVS));

			FRHIRenderPassInfo RPInfo(
				ARRAY_COUNT(UAVS), UAVS
			);

			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_DontStore, ERenderTargetActions::Load_DontStore);
			// in k-buffer path, tressfx scene depth is the original scene depth without hair depth
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture;
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXFillKBuffer"));

			TUniformBufferRef<FTressFXKBufferFillPassUniformParameters> TFXKBufferUniformBuffer;

			//the basepass buffer on the scene gets updated inside CreateOpaqueBasePassUniformBuffer
			CreateTressFXKBufferPassUniformBuffer(RHICmdList, View, ScreenShadowMaskTexture, TFXKBufferUniformBuffer, TressFXKBufferNodePoolSize);
			FMeshPassProcessorRenderState DrawRenderState(View, TFXKBufferUniformBuffer);
			Scene->UniformBuffers.UpdateViewUniformBuffer(View);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_FillColors].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.EndRenderPass();
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveKbuffer);

			FRHIRenderPassInfo RPInfo(
				SceneContext.GetSceneColorSurface(), ERenderTargetActions::Load_Store,
				SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
			);

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXResolveKbuffer"));

			TShaderMapRef<FScreenVS>							VertexShader(View.ShaderMap);
			TShaderMapRef<FTressFXFKBufferResolvePS>			PixelShader(View.ShaderMap);

			FRenderingCompositePassContext Context(RHICmdList, View);
			Context.SetViewportAndCallRHI(View.ViewRect);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<
				EColorWriteMask::CW_RGBA,
				EBlendOperation::BO_Add,  // color blend op
				EBlendFactor::BF_One,  // color source blend
				EBlendFactor::BF_SourceAlpha, //color dest blend
				EBlendOperation::BO_Add,   //alpha blend op
				EBlendFactor::BF_Zero, // alpha src blend
				EBlendFactor::BF_Zero //alpha dest blend
			>::GetRHI();

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
				true, CF_Greater,
				false, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0xff, 0x00, EDepthWriteMask::DWM_Zero, true>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			FUnorderedAccessViewRHIParamRef Uavs[2] = { 
				TressFXKBufferListHeads->GetRenderTargetItem().UAV,
				TressFXKBufferNodes->UAV
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, Uavs, 2);

			VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
			PixelShader->SetParameters(Context.RHICmdList, View, TressFXKBufferNodes->SRV, TressFXKBufferListHeads->GetRenderTargetItem().ShaderResourceTexture);

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


		if (IsTransientResourceBufferAliasingEnabled()) 
		{
			TressFXKBufferNodes->DiscardTransientResource();
			TressFXKBufferCounter->DiscardTransientResource();
		}
	}
}

void FSceneRenderer::RenderTressFXDepthsAndVelocity(FRHICommandListImmediate& RHICmdList, int32 TFXRenderType)
{
	if (!ShouldRenderTressFX((int32)ETressFXPass::DepthsVelocity))
	{
		return;
	}
	RenderDepthsAndVelocity(RHICmdList, Views, Scene, TFXRenderType);	
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
		case ETressFXRenderType::KBuffer:
		{
			if (ShouldRenderTressFX(ETressFXPass::DepthsVelocity))
			{
				RenderKbufferBasePass(RHICmdList, Views, Scene);
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
		case ETressFXRenderType::KBuffer:
		{
			RenderKBufferResolvePasses(RHICmdList, Views, Scene, ScreenShadowMaskTexture);
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