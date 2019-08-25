
#pragma once
#include "TressFXRendering.h"
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
#include "Engine/Public/TressFXPublicDef.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessing.h"
#include "RectLightSceneProxy.h"
#include "PipelineStateCache.h"

DEFINE_LOG_CATEGORY(TressFXRenderingLog);

extern int32 GTressFXOITMode;
extern int32 GTressFXKBufferSize;

int32 GBTressFXPreferCompute = 1;
FAutoConsoleVariableRef CVarTressFXUseComputeResolves(
	TEXT("tfx.PreferCompute"),
	GBTressFXPreferCompute,
	TEXT("1: (default) Use compute shaders for resolve passes, if supported. \n")
	TEXT("0: Use full screen pixel shaders for resolve passes"),
	ECVF_RenderThreadSafe
);

float GTressFXMinAlphaForDepth = 0.5f;
FAutoConsoleVariableRef CVarTressFXMinAlphaForSceneDepth(
	TEXT("tfx.MinAlphaForSceneDepth"),
	GTressFXMinAlphaForDepth,
	TEXT("The minimum alpha value for hair to be considered for hair shadows. This only affects shortcut rendering.")
	TEXT("Shadows cast by hair onto the scene dont yet take opacity into account.")	,
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarTressFXEnableSurfaceShadow(
	TEXT("tfx.OITSurfaceShadow"),
	1,
	TEXT("Enable surface shadows during shadow projection. If 0, only subsurface is used.\n")
	TEXT("This is mainly for debugging. Recommended to keep it at 1"),
	ECVF_RenderThreadSafe
);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFXFillColorPS - Pixel shader for Third pass of shortcut, and PPLL build of kbuffer
////////////////////////////////////////////////////////////////////////////////

//#pragma optimize("",off)

template <ETressFXPass::Type ColorPassType, int32 KBufferSize>
class FTressFXFillColorPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FTressFXFillColorPS, MeshMaterial)

public:

	FTressFXFillColorPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		TressfxShadeParameters.Bind(Initializer.ParameterMap, TEXT("TressfxShadeParameters"));
		ColorPassUniformBuffer.Bind(Initializer.ParameterMap, FTressFXColorPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
		if (ColorPassType == ETressFXPass::FillColor_KBuffer)
		{
			RWFragmentListHead.Bind(Initializer.ParameterMap, TEXT("RWFragmentListHead"));
			RWLinkedListUAV.Bind(Initializer.ParameterMap, TEXT("RWLinkedListUAV"));
			RWCounterBuffer.Bind(Initializer.ParameterMap, TEXT("RWCounterBuffer"));
			RWPixelCounter.Bind(Initializer.ParameterMap, TEXT("RWPixelCounter"));
		}
	}

	FTressFXFillColorPS() {}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		FShaderUniformBufferParameter::ModifyCompilationEnvironment(
			FTressFXColorPassUniformParameters::StaticStructMetadata.GetShaderVariableName(),
			FTressFXColorPassUniformParameters::StaticStructMetadata,
			Parameters.Platform,
			OutEnvironment
		);

		OutEnvironment.SetDefine(TEXT("TFX_SHORTCUT"), ColorPassType == ETressFXPass::FillColor_Shortcut ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("TFX_PPLL"), ColorPassType == ETressFXPass::FillColor_KBuffer ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("APPROX_DEEP_SHADOW"), Parameters.Material->TressFXApproximateDeepShadow() ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("ATTENUATE_SHADOW_BY_ALPHA"), Parameters.Material->TressFXAttenuateShadowByAlpha() ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("ENABLE_GLINT"), Parameters.Material->TressFXEnableGlint() ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("SUPPORT_RECT_LIGHT"), Parameters.Material->TressFXEnableRectLights() ? TEXT("1") : TEXT("0"));

		if (ColorPassType == ETressFXPass::FillColor_KBuffer)
		{
			OutEnvironment.SetDefine(TEXT("KBUFFER_SIZE"), KBufferSize);
		}
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		if (Parameters.VertexFactoryType == FindVertexFactoryType(FName(TEXT("FTressFXVertexFactory"), FNAME_Find)) && (Parameters.Material->IsUsedWithTressFX() || Parameters.Material->IsSpecialEngineMaterial()))
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

		return false;
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool result = FMeshMaterialShader::Serialize(Ar);
		Ar << TressfxShadeParameters;
		Ar << ColorPassUniformBuffer;
		if (ColorPassType == ETressFXPass::FillColor_KBuffer)
		{
			Ar << RWFragmentListHead;
			Ar << RWLinkedListUAV;
			Ar << RWCounterBuffer;
			Ar << RWPixelCounter;
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
		const FRHIPixelShader* ShaderRHI = GetPixelShader();
		ITressFXSceneProxy* TFXProxy = (ITressFXSceneProxy*)PrimitiveSceneProxy;
		ShaderBindings.Add(TressfxShadeParameters, TFXProxy->GetHairObjectShaderUniformBufferParam());
		ShaderBindings.Add(ColorPassUniformBuffer, DrawRenderState.GetPassUniformBuffer());
	}

public:
	FShaderUniformBufferParameter ColorPassUniformBuffer;
	FShaderUniformBufferParameter TressfxShadeParameters;

	//Kbuffer
	FRWShaderParameter RWFragmentListHead;
	FRWShaderParameter RWLinkedListUAV;
	FRWShaderParameter RWCounterBuffer;
	FRWShaderParameter RWPixelCounter;
};

typedef FTressFXFillColorPS<ETressFXPass::FillColor_Shortcut,0> FTressFXFillColorPS_Shortcut;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXFillColorPS_Shortcut, TEXT("/Engine/Private/TressFXFillColorPS.usf"), TEXT("main"), SF_Pixel);

#define IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS(KBufferSize) \
	typedef FTressFXFillColorPS<ETressFXPass::FillColor_KBuffer, KBufferSize> FTressFXFillColorPS_KBuffer##KBufferSize; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXFillColorPS_KBuffer##KBufferSize, TEXT("/Engine/Private/TressFXFillColorPS.usf"), TEXT("main"), SF_Pixel);

static_assert(2 >= MIN_TFX_KBUFFER_SIZE, "MIN_TFX_KBUFFER_SIZE is not >= 2!");
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
static_assert(16 <= MAX_TFX_KBUFFER_SIZE, "MAX_TFX_KBUFFER_SIZE is not <= 16!");
#undef IMPLEMENT_TRESSFX_KBUFFER_FILL_PASS


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

static bool IsSM6(ERHIFeatureLevel::Type CurrentFeatureLevel) 
{
	//for now just use GRHISupportsWaveOperations to detect sm6 - which will be dx12 only i think
	// really would like an sm6 in ERHIFeatureLevel....
	return GRHISupportsWaveOperations && CurrentFeatureLevel >= ERHIFeatureLevel::SM5;
}

bool FSceneRenderer::TressFXCanUseComputeResolves(const FSceneRenderTargets& SceneContext)
{
	//scene color automatically gets uav flag if greater >= sm5 and deferred shading, but checking is good idea
	const FPooledRenderTargetDesc SceneColorDesc = SceneContext.GetSceneColor()->GetDesc();
	const uint32 SceneColorFlags = SceneColorDesc.TargetableFlags;

	//this does not work correctly since there is no SP_PCD3D_SM6, or SM6 enumeration yet :*(
	//const bool SupportsTypedUAVLoads = RHISupports4ComponentUAVReadWrite(ShaderPlatform);

	const bool SupportsTypedUAVLoads = true || IsSM6(SceneContext.GetCurrentFeatureLevel()); //temporarily just setting this to true
	const bool bUseComputeResolve = SupportsTypedUAVLoads && (static_cast<uint32>(GBTressFXPreferCompute) > 0) && (SceneColorFlags & TexCreate_UAV);
	return bUseComputeResolve;
}

//////////////////////////////////////////////////////////////////////////
//FTressFXDepthsVelocityPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

#define PROCESS_DEPTHSVELOCITY(bCalcVelocity,OITMode, ...)					\
	if(bCalcVelocity)														\
	{																		\
		Process<true, OITMode>(__VA_ARGS__);								\
	}																		\
	else																	\
	{																		\
		Process<false, OITMode>(__VA_ARGS__);								\
	}

#define PROCESS_DEPTHSVELOCITY2(bCalcVelocity,OITMode, ...)																				\
switch (OITMode)																														\
{																																		\
	case ETressFXOITMode::KBuffer:  PROCESS_DEPTHSVELOCITY(bCalcVelocity,ETressFXOITMode::KBuffer, __VA_ARGS__) break;					\
	case ETressFXOITMode::None:  PROCESS_DEPTHSVELOCITY(bCalcVelocity,ETressFXOITMode::None, __VA_ARGS__) break;						\
	default: check(0);																													\
}

template<bool bCalcVelocity, ETressFXOITMode::Type OITMode>
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

	FTressFXShaderElementData ShaderElementData(bIsOpaquePass ? ETressFXPass::DepthsVelocity_Opaque : ETressFXPass::DepthsVelocity_KBuffer, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	TMeshProcessorShaders<
		FTressFXVS<bCalcVelocity>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXVelocityDepthPS<bCalcVelocity, OITMode>> TFXShaders;

	TFXShaders.PixelShader = MaterialResource.GetShader<FTressFXVelocityDepthPS<bCalcVelocity, OITMode>>(VertexFactory->GetType());
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
	const ETressFXRenderMode TFXMaterialRenderMode = MeshBatchMaterial.GetTressFXRenderMode();
	const bool bIsOpaqueTressFX = TFXMaterialRenderMode == ETressFXRenderMode::TressFXRender_Opaque;
	const bool bIsTranslucentTressFX = TFXMaterialRenderMode == ETressFXRenderMode::TressFXRender_Translucent;

	check(bIsOpaqueTressFX || bIsTranslucentTressFX);

	const EBlendMode BlendMode = MeshBatchMaterial.GetBlendMode();
	const bool bWantsVelocity = MeshBatchMaterial.TressFXShouldRenderVelocity();
	
	int32 OITMode = static_cast<uint32>(GTressFXOITMode);
	OITMode = FMath::Clamp(OITMode, 0, (int32)ETressFXOITMode::Max);

	//blend mode masked will render hair depth in the regular pass (pre pass if its on)
	const bool bNoDepth = BlendMode == EBlendMode::BLEND_Masked && bIsOpaqueTressFX;

	if (bWantsVelocity == false && bNoDepth == true)
	{
		//nothing to do
		return;
	}

	bool bDraw = MeshBatchMaterial.IsUsedWithTressFX() && MeshBatch.bTressFX;
	
	if(bDraw && bIsTranslucentTressFX)
	{
		bDraw &= ((!bIsOpaquePass) && OITMode == ETressFXOITMode::KBuffer);
	}
	else if (bDraw && bIsOpaqueTressFX)
	{
		bDraw &= (bIsOpaquePass && bIsOpaqueTressFX);
	}

	if (bDraw)
	{
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, MeshBatchMaterial);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, MeshBatchMaterial);
		PROCESS_DEPTHSVELOCITY2(
			bWantsVelocity, 
			bIsOpaqueTressFX ? ETressFXOITMode::Num : OITMode,
			MeshBatch, 
			BatchElementMask, 
			StaticMeshId, 
			PrimitiveSceneProxy, 
			MaterialRenderProxy, 
			MeshBatchMaterial, 
			MeshFillMode, 
			MeshCullMode, 
			bNoDepth
		);
	}
}

FTressFXDepthsVelocityPassMeshProcessor::FTressFXDepthsVelocityPassMeshProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext,
	ETressFXPass::Type PassType)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
{
	check(PassType == ETressFXPass::DepthsVelocity_KBuffer || PassType == ETressFXPass::DepthsVelocity_Opaque);
	PassDrawRenderState = InPassDrawRenderState;
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.DepthPassUniformBuffer);
	bIsOpaquePass = PassType == ETressFXPass::DepthsVelocity_Opaque;
}

void SetupDepthsVelocityPassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
}

FMeshPassProcessor* CreateOpaqueTressFXDepthsVelocityPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState TRessFXDepthsVelocityPassState;
	SetupDepthsVelocityPassState(TRessFXDepthsVelocityPassState);
	return new(FMemStack::Get()) FTressFXDepthsVelocityPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, TRessFXDepthsVelocityPassState, InDrawListContext, ETressFXPass::DepthsVelocity_Opaque);
}

FMeshPassProcessor* CreateKBufferTressFXDepthsVelocityPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState TRessFXDepthsVelocityPassState;
	SetupDepthsVelocityPassState(TRessFXDepthsVelocityPassState);
	return new(FMemStack::Get()) FTressFXDepthsVelocityPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, TRessFXDepthsVelocityPassState, InDrawListContext, ETressFXPass::DepthsVelocity_KBuffer);
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
		, BO_Add			// color blend op
		, BF_Zero			// color source blend
		, BF_SourceColor	//color dest blend
		, BO_Add			//alpha blend op
		, BF_Zero			// alpha src blend
		, BF_SourceAlpha	//alpha dest blend
		//velocity
		, CW_RGBA
		, BO_Add			// color blend op
		, BF_One			// color source blend
		, BF_Zero			//color dest blend
		, BO_Add			//alpha blend op
		, BF_One			// alpha src blend
		, BF_Zero			//alpha dest blend
	>::GetRHI());
	DrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		0xff, 0xff, DWM_Zero //JAKETODO, DWM_zero only currently implemented in dx11/12, not sure how it will work on other platforms
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
		0xff, 0xff, DWM_Zero //JAKETODO, DWM_zero only currently implemented in dx11/12, not sure how it will work on other platforms
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
		BO_Add,// color blend op
		BF_One,// color source blend
		BF_One,//color dest blend
		BO_Add,//alpha blend op
		BF_One,// alpha src blend
		BF_One //alpha dest blend
		>::GetRHI());
	DrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
		0xff, 0xff, DWM_Zero //JAKETODO, DWM_zero only currently implemented in dx11/12, not sure how it will work on other platforms
		>::GetRHI());

	FTressFXShaderElementData ShaderElementData(ETressFXPass::FillColor_Shortcut, ViewIfDynamicMeshCommand);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);
	TMeshProcessorShaders<
		FTressFXVS<false>,
		FMeshMaterialShader,
		FMeshMaterialShader,
		FTressFXFillColorPS<ETressFXPass::FillColor_Shortcut, 0>> TFXShaders;

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

#define PROCESS_KBUFFER(KBufferSize, ...)						\
switch (KBufferSize)											\
{																\
	case 2:  ProcessKBuffer<2>(__VA_ARGS__); break;				\
	case 3:  ProcessKBuffer<3>(__VA_ARGS__); break;				\
	case 4:  ProcessKBuffer<4>(__VA_ARGS__); break;				\
	case 5:  ProcessKBuffer<5>(__VA_ARGS__); break;				\
	case 6:  ProcessKBuffer<6>(__VA_ARGS__); break;				\
	case 7:  ProcessKBuffer<7>(__VA_ARGS__); break;				\
	case 8:  ProcessKBuffer<8>(__VA_ARGS__); break;				\
	case 9:  ProcessKBuffer<9>(__VA_ARGS__); break;				\
	case 10: ProcessKBuffer<10>(__VA_ARGS__); break;			\
	case 11: ProcessKBuffer<11>(__VA_ARGS__); break;			\
	case 12: ProcessKBuffer<12>(__VA_ARGS__); break;			\
	case 13: ProcessKBuffer<13>(__VA_ARGS__); break;			\
	case 14: ProcessKBuffer<14>(__VA_ARGS__); break;			\
	case 15: ProcessKBuffer<15>(__VA_ARGS__); break;			\
	case 16: ProcessKBuffer<16>(__VA_ARGS__); break;			\
	default: check(0);											\
}

template<ETressFXOITMode::Type OITMode>
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

	switch (OITMode)
	{
		case ETressFXOITMode::KBuffer:
		{
			int32 KBufferSize = FMath::Clamp(static_cast<int32>(GTressFXKBufferSize), MIN_TFX_KBUFFER_SIZE, MAX_TFX_KBUFFER_SIZE);
			PROCESS_KBUFFER(
				KBufferSize,
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
		case ETressFXOITMode::ShortCut:
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
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, MeshBatchMaterial);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, MeshBatchMaterial);
		int32 TressFXOITMode = static_cast<uint32>(GTressFXOITMode);
		TressFXOITMode = FMath::Clamp(TressFXOITMode, 0, (int32)ETressFXOITMode::Max);
		 if(TressFXOITMode == ETressFXOITMode::ShortCut)
		{
			 Process<ETressFXOITMode::ShortCut>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
		}
		else if(TressFXOITMode == ETressFXOITMode::KBuffer)
		{
			Process<ETressFXOITMode::KBuffer>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, MeshBatchMaterial, MeshFillMode, MeshCullMode);
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
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.TressFXColorPassUniformBuffer);
}

void SetupFillColorPassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	//set up is done during Process() since its depends whether its kbuffer or shortcut
}

FMeshPassProcessor* CreateTRessFXFillColorPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	SetupFillColorPassState(PassDrawRenderState);
	return new(FMemStack::Get()) FTressFXFillColorPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext);
}

void FSceneRenderer::GetAnyViewHasTressFX(bool &bOutHasTranslucentTressFX, bool &bOutHasOpaqueTressFX)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		bOutHasTranslucentTressFX |= View.bHasTranslucentTressFX;
		bOutHasOpaqueTressFX |= View.bHasOpaqueTressFX;
	}
}

bool FSceneRenderer::ShouldRenderTressFX(int32 TressFXPass)
{
	if (TressFXPass < 0 || TressFXPass > ETressFXPass::Max)
	{
		return false;
	}

	const bool bRenderInPreview = true;//easy to turn off later...
	const bool isEditorPreview = Scene->World->WorldType == EWorldType::EditorPreview;
	if (!Scene->World || (Scene->World->WorldType != EWorldType::Inactive))
	{
		return true;
	}
	if (isEditorPreview) 
	{
		return bRenderInPreview;
	}
	return false;
}

void RenderDepthsAndVelocity(
	FRHICommandListImmediate& RHICmdList
	, TArray<FViewInfo>& Views
	, FScene* Scene
	, int32 OITMode
)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView() || !View.Family->EngineShowFlags.TressFX)
		{
			continue;
		}

		if (OITMode == ETressFXOITMode::KBuffer)
		{
			//copy scene depth, so we have a copy that doesnt have OIT hair depths in it, opaque hairs will be in it though, and thats ok
			SCOPED_DRAW_EVENT(RHICmdList, TressFXCopySceneDepth);
			TressFXCopySceneDepth(RHICmdList, View, SceneContext, SceneContext.TressFXSceneDepth);
		}


		Scene->UniformBuffers.UpdateViewUniformBuffer(View);
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		
		{
			SCOPED_DRAW_EVENT(RHICmdList, TressFXDepthsVelocity);

			/*
			 we need to clear here on 2 conditions: 
			 1. if its the opaque pass. 
				OR
			 2. if its the k-buffer pass and there was no opaque pass 
			*/
			const bool bVelocityNeedsClear = OITMode == ETressFXOITMode::None || (OITMode == ETressFXOITMode::KBuffer && !View.bHasOpaqueTressFX);

			//Clear_Store to clear the TFX Velocity Target
			FRHIRenderPassInfo RPInfo(SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture, bVelocityNeedsClear ? ERenderTargetActions::Clear_Store : ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_Store, ERenderTargetActions::Load_Store);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.GetSceneDepthSurface();;
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;

			
	
			if (bVelocityNeedsClear)
			{
				RHICmdList.BindClearMRTValues(true, false, false);
			}
			
			if(OITMode == ETressFXOITMode::KBuffer)
			{
				const TRefCountPtr<IPooledRenderTarget> GBufferB = SceneContext.GBufferB;
				RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, GBufferB->GetRenderTargetItem().UAV);
				RPInfo.UAVIndex = 1;
				RPInfo.UAVs[RPInfo.NumUAVs++] = GBufferB->GetRenderTargetItem().UAV;
			}
			const EMeshPass::Type Pass = OITMode == ETressFXOITMode::KBuffer ? EMeshPass::TressFX_DepthsVelocity_KBuffer : EMeshPass::TressFX_DepthsVelocity_Opaque;

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXDepthsVelocity"));
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[Pass].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.EndRenderPass();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//Shortcut Passes
/////////////////////////////////////////////////////////////////////////
template<bool bWriteClosestDepth, bool bWriteShadingModel>
void ShortcutDepthsResolve_Impl(
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderTargets& SceneContext, 
	FViewInfo& View, 
	FRHITexture* DepthTarget, 
	const FPooledRenderTargetDesc DepthTargetDesc,
	TRefCountPtr<IPooledRenderTarget>* GBufferB = nullptr
)
{

	FRHIRenderPassInfo RPInfo;
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	if(bWriteShadingModel)
	{
		check(GBufferB && GBufferB->IsValid());
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, GBufferB->GetReference()->GetRenderTargetItem().UAV);
		RPInfo = FRHIRenderPassInfo(
			DepthTarget,
			EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
		);
		RPInfo.UAVIndex = 1;
		RPInfo.UAVs[RPInfo.NumUAVs++] = GBufferB->GetReference()->GetRenderTargetItem().UAV;
	}
	else
	{
		RPInfo = FRHIRenderPassInfo(
			DepthTarget,
			EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
		);
	
	}
	GraphicsPSOInit.BlendState = TStaticBlendState <>::GetRHI();
	RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXshortcutResolveDepth"));

	TShaderMapRef<FScreenVS> VertexShader(View.ShaderMap);
	TShaderMapRef<
		FTressFXShortCutResolveDepthPS<bWriteClosestDepth, bWriteShadingModel>
	>	PixelShader(View.ShaderMap);

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, EApplyRendertargetOption::ForceApply);

	VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
	
	float MinAlphaForShadow = FMath::Clamp(static_cast< float > (GTressFXMinAlphaForDepth), 0.1f, 1.0f);
	PixelShader->SetParameters(RHICmdList, View, SceneContext, MinAlphaForShadow);

	const FIntPoint BufferSize = SceneContext.GetBufferSizeXY();
	const FIntPoint DepthTargetSize = DepthTargetDesc.Extent;
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	const FIntPoint Size = SceneContext.GetBufferSizeXY();
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
	if (bWriteShadingModel)
	{
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, GBufferB->GetReference()->GetRenderTargetItem().UAV);
	}

	RHICmdList.EndRenderPass();
}

void RenderShortcutBasePass(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView() || !View.Family->EngineShowFlags.TressFX)
		{
			continue;
		}

		SCOPED_DRAW_EVENT(RHICmdList, TressFXShortcut_BasePass);

		{
			// Make a copy of scene depth for us to use
			SCOPED_DRAW_EVENT(RHICmdList, CopySceneDepth);
			TressFXCopySceneDepth(RHICmdList, View, SceneContext, SceneContext.TressFXSceneDepth);
		}

		
		// shortcut pass 1, accumulate alpha, depths, and optionally velocity if the material wants it (true by default)
		{
			SCOPED_DRAW_EVENT(RHICmdList, DepthsAlpha);

			static const uint32 ShortcutClearValue[4] = { SHORTCUT_INITIAL_DEPTH ,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH };
			ClearUAV(RHICmdList, SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem(), ShortcutClearValue);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			
			FRHIUnorderedAccessView* UAVs[] = { SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem().UAV};
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

			//only clear velocity if opaque pass has not already cleared it
			const bool bVelocityNeedsClear = !View.bHasOpaqueTressFX;

			if (bVelocityNeedsClear) 
			{
				//clear all
				FLinearColor ClearColors[] = { FLinearColor::White, FLinearColor::Transparent };
				DrawClearQuadMRT(RHICmdList, true, 2, ClearColors, false, 0, false, 0);
			}
			else
			{
				//clear AccumInvAlpha, but not velocity
				DrawClearQuad(RHICmdList, true, FLinearColor::White, false, 0, false, 0);
			}

			FMeshPassProcessorRenderState DrawRenderState(View);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_DepthsAlpha].DispatchDraw(nullptr, RHICmdList);

			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UAVs, ARRAY_COUNT(UAVs));
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().TargetableTexture);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture);
			RHICmdList.EndRenderPass();
		}

		// shortcut pass 2, resolve depths closer to the camera to ue4 scene depth, mainly for shadows and lighting
		// also needs to write to gbufferb, doesnt actually write the shading model, just the TressFX mask.
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveCloserDepthsToSceneDepth);
			ShortcutDepthsResolve_Impl<true, true>(
				RHICmdList,
				SceneContext,
				View,
				SceneContext.GetSceneDepthSurface(),
				SceneContext.SceneDepthZ->GetDesc(),
				&SceneContext.GBufferB
			);
		}
	}
}

// gather shadow infos for Approximate deep shadows. Only supported for a single directional light ATM.
// And for now I am assuming a single directional light in the scene. This is mainly just to show how to do it, i dont plan on using.
TArray<FProjectedShadowInfo*> GatherTressFXProjectionShadows(TArray<FVisibleLightInfo, SceneRenderingAllocator> VisibleLightInfos)
{
	TArray<FProjectedShadowInfo*> TressFXPerObjectShadowInfos;
	{
		for (int32 VisibleLightIndex = 0; VisibleLightIndex < VisibleLightInfos.Num(); VisibleLightIndex++)
		{
			for (int32 ShadowsToProjectIndex = 0; ShadowsToProjectIndex < VisibleLightInfos[VisibleLightIndex].ShadowsToProject.Num(); ShadowsToProjectIndex++)
			{
				FProjectedShadowInfo* Shadow = VisibleLightInfos[VisibleLightIndex].ShadowsToProject[ShadowsToProjectIndex];
				const bool bIsPerObjectTressFX = Shadow->bIsPerObjectTressFX;
				const bool bIsDirectional = Shadow->bDirectionalLight;
				if (bIsPerObjectTressFX && bIsDirectional)
				{
					TressFXPerObjectShadowInfos.Add(Shadow);
				}
			}
		}
	}
	return TressFXPerObjectShadowInfos;
}

FTressFXRectLightData GetRectLightInfos(const FScene* Scene, TArray<FVisibleLightInfo, SceneRenderingAllocator> VisibleLightInfos, const FViewInfo& View)
{
	/* 
		This is a very "hacky" and non-optimal way to add rect light support in a forward-rendering type path. 
		Proper support would have to modify the light grid culling shaders, which i am not sure I want to do.
	*/

	FTressFXRectLightData RectLightData;
	// channel flag < 0 will indicate not rect light
	RectLightData.RectLightShadowChannelFlags = FIntVector4(-1, -1, -1, -1);
	if (View.Family->EngineShowFlags.RectLights) 
	{
		for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
			const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
			const FLightSceneProxy* LightProxy = LightSceneInfo->Proxy;
			do
			{
				if (!(LightProxy->IsRectLight() && LightProxy->CastsTressFXDynamicShadow()))
				{
					break;
				}
				if (!LightSceneInfo->ShouldRenderLight(View))
				{
					break;
				}
				const int32 ShadowMapChannel = LightSceneInfo->GetDynamicShadowMapChannel();
				if (ShadowMapChannel < 0 || ShadowMapChannel > 3)
				{
					break;
				}

				// we use the dynamic shadow map channel to look up whether its a rect light or not
				// mark as rect light
				RectLightData.RectLightShadowChannelFlags[ShadowMapChannel] = 1;

				const FRectLightSceneProxy* RectProxy = (const FRectLightSceneProxy*)LightProxy;

				FLightShaderParameters RectLightShaderParameters;
				RectProxy->GetLightShaderParameters(RectLightShaderParameters);

				RectLightData.RectLightInfos[ShadowMapChannel] = FVector4(
					RectLightShaderParameters.RectLightBarnCosAngle,
					RectLightShaderParameters.RectLightBarnLength,
					-1, //  unused for now
					-1 // unused for now
				);
			

			} while (false);
		}
	}

	//initialize to dummy values if not a rect light
	for (int32 ChannelIndex = 0; ChannelIndex < 4; ChannelIndex++)
	{
		if (RectLightData.RectLightShadowChannelFlags[ChannelIndex] < 0)
		{
			RectLightData.RectLightInfos[ChannelIndex] = FVector4(-1.f, -1.f, -1.f, -1.f);
		}
	}
	RectLightData.RectTextureDummy = GSystemTextures.WhiteDummy->GetRenderTargetItem().ShaderResourceTexture;
	return RectLightData;
}

void RenderShortcutResolvePass(
	FRHICommandListImmediate& RHICmdList, 
	TArray<FViewInfo>& Views, 
	FScene* Scene, 
	TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture,
	const bool bUseComputeResolve,
	FSortedShadowMaps SortedShadowsForShadowDepthPass,
	TArray<FVisibleLightInfo, SceneRenderingAllocator> VisibleLightInfos
)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SCOPED_DRAW_EVENT(RHICmdList, TressFXShortcut_Resolve);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView() || !View.Family->EngineShowFlags.TressFX)
		{
			continue;
		}

		// shortcut pass 2.5, depths resolve to the tressfxscenedepth texture, write the farthest depth
		// its important we dont do this until shadows have been rendered. Because we use the this texture (without hair depth) to render regular shadows behind tressfx
		// and the regular scene depth texture (WITH hair depths) to render tressfx shadows into a separate mask texture
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveFarthestDepthsToTressFXSceneDepth);
			ShortcutDepthsResolve_Impl<false, false>(
				RHICmdList,
				SceneContext,
				View,
				SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture,
				SceneContext.TressFXSceneDepth->GetDesc()
			);
		}


		TArray<FProjectedShadowInfo*> TressFXPerObjectShadowInfos = GatherTressFXProjectionShadows(VisibleLightInfos);
		const FTressFXRectLightData RectLightInfo = GetRectLightInfos(Scene, VisibleLightInfos, View);

		// shortcut pass 3, fill colors
		{
			SCOPED_DRAW_EVENT(RHICmdList, FillColors);

			FRHIRenderPassInfo RPInfo(
				SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::Load_DontStore,
				SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
			);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXFillColor"));

			TUniformBufferRef<FTressFXColorPassUniformParameters> TFXColorPassUniformBuffer;

			CreateTressFXColorPassUniformBuffer(
				  RHICmdList
				, View
				, ScreenShadowMaskTexture
				, TFXColorPassUniformBuffer
				, SortedShadowsForShadowDepthPass
				, TressFXPerObjectShadowInfos
				, RectLightInfo
			);
			FMeshPassProcessorRenderState DrawRenderState(View, TFXColorPassUniformBuffer);
			Scene->UniformBuffers.UpdateViewUniformBuffer(View);
			
			DrawClearQuad(RHICmdList,true, FLinearColor::Transparent, false, 0, false, 0);		
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_FillColors].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.EndRenderPass();
		}

		{
			//readable for resolve pass
			FRHITexture* Resources[2] = {
				SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().TargetableTexture,
				SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().TargetableTexture
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources, 2);
		}

		// shortcut pass 4: resolve color
		if (bUseComputeResolve)
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveColorsCS);
			UnbindRenderTargets(RHICmdList);
			const FIntRect Rect = View.ViewRect;
			FIntPoint DestSize(Rect.Width(), Rect.Height());

			FRenderingCompositePassContext Context(RHICmdList, View);
			Context.SetViewportAndCallRHI(Rect, 0.0f, 1.0f);
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SceneContext.GetSceneColorTextureUAV());
			TShaderMapRef<FTressFXShortCutResolveColorCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());


			ComputeShader->SetParameters(
				RHICmdList,
				View,
				SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture,
				SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().ShaderResourceTexture,
				SceneContext.GetSceneColorTextureUAV(),
				DestSize
			);

			uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, (int32)FTressFXShortCutResolveColorCS::ThreadSizeX);
			uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, (int32)FTressFXShortCutResolveColorCS::ThreadSizeY);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

			ComputeShader->UnsetParameters(RHICmdList);
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, SceneContext.GetSceneColorTextureUAV());
		}
		else
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveColorsPS);

			FRHIRenderPassInfo RPInfo(
				SceneContext.GetSceneColorSurface(), ERenderTargetActions::Load_Store,
				SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
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
				, BO_Add				// color blend op
				, BF_One				// color source blend
				, BF_SourceAlpha		//color dest blend
				, BO_Add				//alpha blend op
				, BF_Zero				// alpha src blend
				, BF_Zero				//alpha dest blend
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
	RenderDepthsAndVelocity(RHICmdList, Views, Scene, ETressFXOITMode::KBuffer);
}

void RenderKBufferResolvePasses(
	FRHICommandListImmediate& RHICmdList,
	TArray<FViewInfo>& Views, FScene* Scene,
	TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture,
	const bool bUseComputeResolve,
	FSortedShadowMaps SortedShadowsForShadowDepthPass,
	TArray<FVisibleLightInfo, SceneRenderingAllocator> VisibleLightInfos
)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.TressFXMeshBatches.Num() < 1 || !View.ShouldRenderView() || !View.Family->EngineShowFlags.TressFX)
		{
			continue;
		}

		TArray<FProjectedShadowInfo*> TressFXPerObjectShadowInfos = GatherTressFXProjectionShadows(VisibleLightInfos);

		SCOPED_DRAW_EVENT(RHICmdList, TressFXKBufferPasses);

		TRefCountPtr<IPooledRenderTarget> TressFXKBufferListHeads;
		TRefCountPtr<IPooledRenderTarget> TressFXOpacityThresholdingUAV;
		FRWBufferStructured* TressFXKBufferNodes = nullptr;
		FRWBuffer* TressFXKBufferCounter = nullptr;
		int32 TressFXKBufferNodePoolSize;
		SceneContext.GetTressFXKBufferResources(
			  RHICmdList
			, TressFXKBufferListHeads
			, TressFXOpacityThresholdingUAV
			, TressFXKBufferNodes
			, TressFXKBufferCounter
			, TressFXKBufferNodePoolSize
		);

		// Fill k-buffer
		{
			SCOPED_DRAW_EVENT(RHICmdList, FillKbuffer);

			const uint32 FragmentListClearValue[4] = { TFX_PPLL_NULL, TFX_PPLL_NULL, TFX_PPLL_NULL, TFX_PPLL_NULL };
			ClearUAV(RHICmdList, TressFXKBufferListHeads->GetRenderTargetItem(), FragmentListClearValue);
			TressFXKBufferNodes->AcquireTransientResource();
			TressFXKBufferCounter->AcquireTransientResource();
			ClearUAV(RHICmdList, *TressFXKBufferNodes, 0);
			const uint32 OpacityAccumClear[4] = { 0, 0, 0, 0 };
			ClearUAV(RHICmdList, TressFXOpacityThresholdingUAV->GetRenderTargetItem(), OpacityAccumClear);

			//starts at one so if it rolls over to 0 after max uint it wont try to add more
			static const uint32 Values[4] = { 1, 1, 1, 1 };
			RHICmdList.ClearTinyUAV(TressFXKBufferCounter->UAV, Values);

			FRHIUnorderedAccessView* UAVS[] = {
				TressFXKBufferListHeads->GetRenderTargetItem().UAV,
				TressFXKBufferNodes->UAV,
				TressFXKBufferCounter->UAV,
				TressFXOpacityThresholdingUAV->GetRenderTargetItem().UAV
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, UAVS, ARRAY_COUNT(UAVS));

			FRHIRenderPassInfo RPInfo(
				ARRAY_COUNT(UAVS), UAVS
			);

			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_DontStore, ERenderTargetActions::Load_DontStore);
			// in k-buffer path, tressfx scene depth is the original scene depth without hair depth
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture;
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXFillKBuffer"));

			FTressFXRectLightData RectLightInfo = GetRectLightInfos(Scene, VisibleLightInfos, View);

			TUniformBufferRef<FTressFXColorPassUniformParameters> TFXColorPassUniformBuffer;

			CreateTressFXColorPassUniformBuffer(
				RHICmdList, 
				View, 
				ScreenShadowMaskTexture, 
				TFXColorPassUniformBuffer, 
				SortedShadowsForShadowDepthPass, 
				TressFXPerObjectShadowInfos,
				RectLightInfo,
				TressFXKBufferNodePoolSize
			);
			FMeshPassProcessorRenderState DrawRenderState(View, TFXColorPassUniformBuffer);
			Scene->UniformBuffers.UpdateViewUniformBuffer(View);
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			View.ParallelMeshDrawCommandPasses[EMeshPass::TressFX_FillColors].DispatchDraw(nullptr, RHICmdList);
			RHICmdList.EndRenderPass();
		}

		{
			//readable for resolve pass
			FRHIUnorderedAccessView* UAVS[] = {
				TressFXKBufferListHeads->GetRenderTargetItem().UAV,
				TressFXKBufferNodes->UAV
			};
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UAVS, ARRAY_COUNT(UAVS));
		}

		if (bUseComputeResolve)
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveKBufferCS);
			UnbindRenderTargets(RHICmdList);
			const FIntRect Rect = View.ViewRect;
			FIntPoint DestSize(Rect.Width(), Rect.Height());

			FRenderingCompositePassContext Context(RHICmdList, View);
			Context.SetViewportAndCallRHI(Rect, 0.0f, 1.0f);
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, SceneContext.GetSceneColorTextureUAV());
			TShaderMapRef<FTressFXFKBufferResolveCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());


			ComputeShader->SetParameters(
				RHICmdList,
				View,
				TressFXKBufferNodes->SRV,
				TressFXKBufferListHeads->GetRenderTargetItem().ShaderResourceTexture,
				SceneContext.GetSceneColorTextureUAV(),
				DestSize
			);

			uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, (int32)FTressFXFKBufferResolveCS::ThreadSizeX);
			uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, (int32)FTressFXFKBufferResolveCS::ThreadSizeY);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

			ComputeShader->UnsetParameters(RHICmdList);
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, SceneContext.GetSceneColorTextureUAV());
		}
		else
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveKBufferPS);

			FRHIRenderPassInfo RPInfo(
				SceneContext.GetSceneColorSurface(), ERenderTargetActions::Load_Store,
				SceneContext.TressFXSceneDepth->GetRenderTargetItem().TargetableTexture, EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil
			);

			RHICmdList.BeginRenderPass(RPInfo, TEXT("TressFXResolveKBuffer"));

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

void FSceneRenderer::RenderTressOpaqueFXDepthsAndVelocity(FRHICommandListImmediate& RHICmdList)
{
	if (!ShouldRenderTressFX((int32)ETressFXPass::DepthsVelocity_Opaque))
	{
		return;
	}
	RenderDepthsAndVelocity(RHICmdList, Views, Scene, ETressFXOITMode::None);
}

//this should only get called if scene has any translucent tressfx
void FSceneRenderer::RenderTressFXBasePass(FRHICommandListImmediate& RHICmdList, int32 OITMode)
{

	SCOPED_DRAW_EVENT(RHICmdList, TressFXBasePass);

	switch (OITMode)
	{
		case ETressFXOITMode::ShortCut:
		{
			if (ShouldRenderTressFX(ETressFXPass::DepthsAlpha))
			{
				RenderShortcutBasePass(RHICmdList, Views);
			}
			break;
		}
		case ETressFXOITMode::KBuffer:
		{
			if (ShouldRenderTressFX(ETressFXPass::DepthsVelocity_KBuffer))
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

void FSceneRenderer::RenderTressfXResolvePass(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& ScreenShadowMaskTexture, int32 OITMode)
{

	SCOPED_DRAW_EVENT(RHICmdList, TressFXResolvePass);

	switch (OITMode)
	{
		case ETressFXOITMode::ShortCut:
		{
			if (ShouldRenderTressFX(ETressFXPass::FillColor_Shortcut)) 
			{
				RenderShortcutResolvePass(
					RHICmdList, 
					Views, 
					Scene, 
					ScreenShadowMaskTexture, 
					TressFXCanUseComputeResolves(FSceneRenderTargets::Get(RHICmdList)), 
					SortedShadowsForShadowDepthPass,
					VisibleLightInfos
				);
			}
			break;
		}
		case ETressFXOITMode::KBuffer:
		{
			if (ShouldRenderTressFX(ETressFXPass::FillColor_KBuffer))
			{
				RenderKBufferResolvePasses(
					RHICmdList
					, Views
					, Scene
					, ScreenShadowMaskTexture
					, TressFXCanUseComputeResolves(FSceneRenderTargets::Get(RHICmdList))
					, SortedShadowsForShadowDepthPass
					, VisibleLightInfos
				);
			}
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

	SCOPED_DRAW_EVENT(RHICmdList, TressFXVelocity);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		if (!View.Family->EngineShowFlags.TressFX) 
		{
			continue;
		}

		FRHITexture* Resources[] = {
			SceneContext.TressFXVelocity->GetRenderTargetItem().TargetableTexture,

		};
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources, ARRAY_COUNT(Resources));

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, CM_None);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		TShaderMapRef<FScreenVS>					VertexShader(View.ShaderMap);
		TShaderMapRef<FTressFXResolveVelocityPs>	PixelShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, EApplyRendertargetOption::ForceApply);

		VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
		PixelShader->SetParameters(RHICmdList, SceneContext.TressFXVelocity);

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
	}
}

FRegisterPassProcessorCreateFunction RegisterOpaqueTressFXDepthsVelocityPass(&CreateOpaqueTressFXDepthsVelocityPassProcessor, EShadingPath::Deferred, EMeshPass::TressFX_DepthsVelocity_Opaque, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterKBufferTressFXDepthsVelocityPass(&CreateKBufferTressFXDepthsVelocityPassProcessor, EShadingPath::Deferred, EMeshPass::TressFX_DepthsVelocity_KBuffer, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterTressFXDepthsAlphaPass(&CreateTRessFXDepthsAlphaPassProcessor, EShadingPath::Deferred, EMeshPass::TressFX_DepthsAlpha, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);
FRegisterPassProcessorCreateFunction RegisterTressFXFillColorPass(&CreateTRessFXFillColorPassProcessor, EShadingPath::Deferred, EMeshPass::TressFX_FillColors, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);

//#pragma optimize("", on)