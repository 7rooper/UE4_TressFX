
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
			NodePoolSize.Bind(Initializer.ParameterMap, TEXT("NodePoolSize"));
			RWCounterBuffer.Bind(Initializer.ParameterMap, TEXT("RWCounterBuffer"));
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
			Ar << NodePoolSize;
			Ar << RWCounterBuffer;
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
			//todo
			//FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
			//FIntPoint BufferSize = SceneRenderTargets.GetBufferSizeXY();
			//int32 KBufferSize = CVarOITKBufferSize.GetValueOnAnyThread();
			//KBufferSize = KBufferSize <= MAX_KBUFFER_SIZE ? KBufferSize : MAX_KBUFFER_SIZE;
			//KBufferSize = KBufferSize >= MIN_KBUFFER_SIZE ? KBufferSize : MIN_KBUFFER_SIZE;
			//SetShaderValue(RHICmdList, ShaderRHI, nNodePoolSize, BufferSize.X * BufferSize.Y * KBufferSize);
		}
	}

public:

	FShaderUniformBufferParameter tressfxShadeParameters;

	//Kbuffer
	FRWShaderParameter RWFragmentListHead;
	FRWShaderParameter RWLinkedListUAV;
	FRWShaderParameter RWCounterBuffer;
	FShaderParameter NodePoolSize;
};

typedef FTressFXFillColorPS<ETressFXPass::FillColor_Shortcut, 0> FTressFXFillColorPS_Shortcut;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXFillColorPS_Shortcut, TEXT("/Engine/Private/TressFXFillColorPS.usf"), TEXT("main"), SF_Pixel);


#define IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(KBufferSize) \
	typedef FTressFXFillColorPS<ETressFXPass::FillColor_KBuffer, KBufferSize> FTressFXFillColorPS_KBuffer##KBufferSize; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXFillColorPS_KBuffer##KBufferSize, TEXT("/Engine/Private/TressFXFillColorPS.usf"), TEXT("main"), SF_Pixel);

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
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(17)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(18)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(19)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(20)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(21)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(22)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(23)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(24)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(25)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(26)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(27)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(28)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(29)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(30)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(31)
IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(32)

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
		FTressFXVS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXVelocityDepthPS<bCalcVelocity>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFXVelocityDepthPS<bCalcVelocity>>(VertexFactory->GetType());
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

template<bool bWantsVelocity, int32 KBufferSize>
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

	FTressFXShaderElementData ShaderElementData(ETressFXPass::FillColor_KBuffer, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);
	TMeshProcessorShaders<
		FTressFXVS<bWantsVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXFillColorPS<ETressFXPass::FillColor_KBuffer, KBufferSize>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFXFillColorPS<ETressFXPass::FillColor_KBuffer, KBufferSize>>(VertexFactory->GetType());
	TFXShaders.VertexShader = MaterialResource.GetShader<FTressFXVS<bWantsVelocity>>(VertexFactory->GetType());

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

#define PROCESS_KBUFFER(bNeedsVelocity, KBufferSize, MeshBatch, BatchElementMask,StaticMeshId,PrimitiveSceneProxy,MaterialRenderProxy,MaterialResource,MeshFillMode,MeshCullMode)	\
if( bNeedsVelocity )																																								\
{																																													\
	ProcessKBuffer< true, KBufferSize >(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode);			\
}																																													\
else																																												\
{																																													\
	ProcessKBuffer< false, KBufferSize >(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode);		\
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
			const bool bWantsVelocity = MeshBatchMaterial.TressFXShouldRenderVelocity();

			//there has to be a better way to do this, but im not clever enough
			int32 KBufferSize = FMath::Clamp(static_cast<int32>(GTressFXKBufferSize), MIN_TFX_KBUFFER_SIZE, MAX_TFX_KBUFFER_SIZE);
			switch (KBufferSize)
			{
				case 4 : PROCESS_KBUFFER(bWantsVelocity, 4, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 5 : PROCESS_KBUFFER(bWantsVelocity, 5, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 6 : PROCESS_KBUFFER(bWantsVelocity, 6, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 7 : PROCESS_KBUFFER(bWantsVelocity, 7, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 8 : PROCESS_KBUFFER(bWantsVelocity, 8, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 9 : PROCESS_KBUFFER(bWantsVelocity, 9, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 10 : PROCESS_KBUFFER(bWantsVelocity, 10, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 11 : PROCESS_KBUFFER(bWantsVelocity, 11, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 12 : PROCESS_KBUFFER(bWantsVelocity, 12, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 13 : PROCESS_KBUFFER(bWantsVelocity, 13, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 14 : PROCESS_KBUFFER(bWantsVelocity, 14, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 15 : PROCESS_KBUFFER(bWantsVelocity, 15, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 16 : PROCESS_KBUFFER(bWantsVelocity, 16, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 17 : PROCESS_KBUFFER(bWantsVelocity, 17, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 18 : PROCESS_KBUFFER(bWantsVelocity, 18, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 19 : PROCESS_KBUFFER(bWantsVelocity, 19, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 20 : PROCESS_KBUFFER(bWantsVelocity, 20, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 21 : PROCESS_KBUFFER(bWantsVelocity, 21, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 22 : PROCESS_KBUFFER(bWantsVelocity, 22, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 23 : PROCESS_KBUFFER(bWantsVelocity, 23, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 24 : PROCESS_KBUFFER(bWantsVelocity, 24, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 25 : PROCESS_KBUFFER(bWantsVelocity, 25, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 26 : PROCESS_KBUFFER(bWantsVelocity, 26, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 27 : PROCESS_KBUFFER(bWantsVelocity, 28, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 28 : PROCESS_KBUFFER(bWantsVelocity, 28, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 29 : PROCESS_KBUFFER(bWantsVelocity, 29, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 30 : PROCESS_KBUFFER(bWantsVelocity, 30, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 31 : PROCESS_KBUFFER(bWantsVelocity, 31, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				case 32 : PROCESS_KBUFFER(bWantsVelocity, 32, MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, MeshFillMode, MeshCullMode) break;
				default: check(0);
			}
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
			Process<ETressFXRenderType::KBuffer>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
		}
		else 
		{
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