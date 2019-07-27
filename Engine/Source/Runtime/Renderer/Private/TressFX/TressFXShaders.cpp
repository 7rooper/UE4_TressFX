#pragma once
#include "TressFXShaders.h"
#include "SceneRendering.h"
#include "BasePassRendering.h"
#include "PrimitiveSceneInfo.h"
#include "TressFX/TressFXSceneProxy.h"

IMPLEMENT_GLOBAL_SHADER(FTressFXResolveVelocityPs, "/Engine/Private/TressFXCopyVelocityDepth.usf", "ResolveVelocityPS", SF_Pixel);

/////////////////////////////////////////////////////////////////////////////////
//  FTressFXCopyOpaqueDepthPS - copies hair depth to scene depth using threshold
////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_GLOBAL_SHADER(FTressFXCopyOpaqueDepthPS, "/Engine/Private/TressFXCopyVelocityDepth.usf", "CopyOpaqueDepthPs", SF_Pixel);

/////////////////////////////////////////////////////////////////////////////////
//  FTressFXCopyDepthPS - Simple Depth Copy Shader
////////////////////////////////////////////////////////////////////////////////

bool FTressFXCopyDepthPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FTressFXCopyDepthPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
}

const TCHAR* FTressFXCopyDepthPS::GetSourceFilename()
{
	return TEXT("/Engine/Private/TressFXCopyVelocityDepth.usf");
}

const TCHAR* FTressFXCopyDepthPS::GetFunctionName()
{
	return TEXT("CopyDepthPS");
}

FTressFXCopyDepthPS::FTressFXCopyDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
{
	SceneTextureShaderParameters.Bind(Initializer);
}

// FShader interface.
bool FTressFXCopyDepthPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << SceneTextureShaderParameters;
	return bShaderHasOutdatedParameters;
}

void FTressFXCopyDepthPS::SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
{
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
	SceneTextureShaderParameters.Set(RHICmdList, GetPixelShader(), View.FeatureLevel, ESceneTextureSetupMode::All);
}

IMPLEMENT_GLOBAL_SHADER(FTressFXCopyDepthPS, "/Engine/Private/TressFXCopyVelocityDepth.usf", "CopyDepthPS", SF_Pixel);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFXVS - Main Vertex shader for tressfx
////////////////////////////////////////////////////////////////////////////////


IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXVS<false>, TEXT("/Engine/Private/TressFXVertexShader.usf"), TEXT("VF_MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXVS<true>, TEXT("/Engine/Private/TressFXVertexShader.usf"), TEXT("VF_MainVS"), SF_Vertex);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_DepthsAlphaPS - Pixel shader for First pass of shortcut
////////////////////////////////////////////////////////////////////////////////

#define IMPLEMENT_TRESSFX_DEPTHSALPHA_SHADER(AVSMNodeCount) \
	typedef FTressFXDepthsAlphaPS<true, AVSMNodeCount> FTressFXDepthsAlphaPS##WithVelocity##AVSMNodeCount; \
	typedef FTressFXDepthsAlphaPS<false, AVSMNodeCount> FTressFXDepthsAlphaPS##NoVelocity##AVSMNodeCount; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXDepthsAlphaPS##NoVelocity##AVSMNodeCount, TEXT("/Engine/Private/TressFXShortCutDepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXDepthsAlphaPS##WithVelocity##AVSMNodeCount, TEXT("/Engine/Private/TressFXShortCutDepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);

IMPLEMENT_TRESSFX_DEPTHSALPHA_SHADER(4);
IMPLEMENT_TRESSFX_DEPTHSALPHA_SHADER(8);
IMPLEMENT_TRESSFX_DEPTHSALPHA_SHADER(12);
IMPLEMENT_TRESSFX_DEPTHSALPHA_SHADER(16);
#undef IMPLEMENT_TRESSFX_DEPTHSALPHA_SHADER

/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_VelocityDepthPS
////////////////////////////////////////////////////////////////////////////////

#define IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(TFXRenderType) \
	typedef FTressFXVelocityDepthPS<true, TFXRenderType> FTressFXVelocityDepthPS##WithVelocity##TFXRenderType; \
	typedef FTressFXVelocityDepthPS<false, TFXRenderType> FTressFXVelocityDepthPS##NoVelocity##TFXRenderType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXVelocityDepthPS##WithVelocity##TFXRenderType, TEXT("/Engine/Private/TressFXVelocityDepthPS.usf"), TEXT("TRessFXVelocityDepthPS"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXVelocityDepthPS##NoVelocity##TFXRenderType, TEXT("/Engine/Private/TressFXVelocityDepthPS.usf"), TEXT("TRessFXVelocityDepthPS"), SF_Pixel);

IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(0); //opaque
IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(1); //shortcut
#undef IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveDepthPS - pixel shader for 2nd pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

#define IMPLEMENT_FTressFXShortCutResolveDepthPS_SetParameters(bWriteClosestDepth, bWriteShadingModelToGBuffer)													\
void FTressFXShortCutResolveDepthPS<bWriteClosestDepth, bWriteShadingModelToGBuffer>::SetParameters(															\
	FRHICommandList& RHICmdList																																	\
	, const FViewInfo& View																																		\
	, FSceneRenderTargets& SceneContext																															\
	, float MinAlphaForShadow)																																	\
{																																								\
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();																									\
																																								\
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);													\
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentDepthsTexture, SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem().ShaderResourceTexture);	\
	SceneTextureShaderParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);													\
	if (bWriteClosestDepth)																																		\
	{																																							\
		SetTextureParameter(RHICmdList, ShaderRHI, AccumInvAlpha, SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);				\
	}																																							\
	SetShaderValue(RHICmdList, ShaderRHI, MinAlphaForSceneDepth, MinAlphaForShadow);																			\
}

IMPLEMENT_FTressFXShortCutResolveDepthPS_SetParameters(true, true)
IMPLEMENT_FTressFXShortCutResolveDepthPS_SetParameters(true, false)
IMPLEMENT_FTressFXShortCutResolveDepthPS_SetParameters(false, false)
IMPLEMENT_FTressFXShortCutResolveDepthPS_SetParameters(false, true)
#undef IMPLEMENT_FTressFXShortCutResolveDepthPS_SetParameters

#define IMPLEMENT_FTressFXShortCutResolveDepthPS(bWriteClosestDepth, bWriteShadingModelToGBuffer)		\
	typedef FTressFXShortCutResolveDepthPS<bWriteClosestDepth, bWriteShadingModelToGBuffer> FTressFXShortCutResolveDepthPS##bWriteClosestDepth##bWriteShadingModelToGBuffer; \
	IMPLEMENT_GLOBAL_SHADER(FTressFXShortCutResolveDepthPS##bWriteClosestDepth##bWriteShadingModelToGBuffer, "/Engine/Private/TressFXShortCutResolveDepthPS.usf", "main", SF_Pixel);

IMPLEMENT_FTressFXShortCutResolveDepthPS(true, true)
IMPLEMENT_FTressFXShortCutResolveDepthPS(true, false)
IMPLEMENT_FTressFXShortCutResolveDepthPS(false, false)
IMPLEMENT_FTressFXShortCutResolveDepthPS(false, true)

#undef IMPLEMENT_FTressFXShortCutResolveDepthPS

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveColorPS - pixel shader for final pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

void FTressFXShortCutResolveColorPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

	SetTextureParameter(RHICmdList, GetPixelShader(), FragmentColorsTexture, SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SetTextureParameter(RHICmdList, GetPixelShader(), AccumInvAlpha, SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);
}

IMPLEMENT_GLOBAL_SHADER(FTressFXShortCutResolveColorPS, "/Engine/Private/TressFXShortCutResolveColorPS.usf", "ShortcutResolvePS", SF_Pixel);


///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveColorCS - compute shader for final pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

void FTressFXShortCutResolveColorCS::SetParameters(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const FTextureRHIParamRef InAccumInvAlphaSRV,
	const FTextureRHIParamRef InFragmentColorsTextureSRV,
	const FUnorderedAccessViewRHIRef SceneColorUAV,
	FIntPoint TargetSize
)
{
	const auto ShaderRHI = GetComputeShader();
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

	SetTextureParameter(RHICmdList, ShaderRHI, FragmentColorsTexture, InFragmentColorsTextureSRV);
	SetTextureParameter(RHICmdList, ShaderRHI, AccumInvAlpha, InAccumInvAlphaSRV);
	SetUAVParameter(RHICmdList, ShaderRHI, SceneColorTex, SceneColorUAV);
	SetShaderValue(RHICmdList, ShaderRHI, TextureSize, FVector4((float)TargetSize.X, (float)TargetSize.Y, 0.0f, 0.0f));
}

void FTressFXShortCutResolveColorCS::UnsetParameters(FRHICommandList& RHICmdList)
{
	const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
	RHICmdList.SetUAVParameter(ShaderRHI, SceneColorTex.GetBaseIndex(), NULL);
}

IMPLEMENT_GLOBAL_SHADER(FTressFXShortCutResolveColorCS, "/Engine/Private/TressFXShortCutResolveColorPS.usf", "ShortcutResolveCS", SF_Compute);


#define TressFXAVSMModifyCompilationEnvironmentCommon_Implement( AVSMNodeCount )																	\
template<>																																			\
void TressFXAVSM<AVSMNodeCount>::ModifyCompilationEnvironmentCommon(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)			\
{																																					\
		OutEnvironment.SetDefine(TEXT("AVSM_NODE_COUNT"), AVSMNodeCount );																			\
		OutEnvironment.SetDefine(TEXT("EMPTY_NODE"), TRESSFX_AVSM_EMPTY_NODE );																		\
}	

TressFXAVSMModifyCompilationEnvironmentCommon_Implement(4);
TressFXAVSMModifyCompilationEnvironmentCommon_Implement(8);
TressFXAVSMModifyCompilationEnvironmentCommon_Implement(12);
TressFXAVSMModifyCompilationEnvironmentCommon_Implement(16);

#undef TressFXAVSMModifyCompilationEnvironmentCommon_Implement

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXClearAVSMBufferPS
//////////////////////////////////////////////////////////////////////////////////
extern int32 GTressFXAVSMTextureSize;
extern int32 GTressFXAVSMNodeCount;


void FTressFXClearAVSMBufferPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
	const int32 AVSMTextureSize = GTressFXAVSMTextureSizes[FMath::Clamp(static_cast<int32>(GTressFXAVSMTextureSize), 0, GTressFXAVSMTextureSizes.Num() - 1)];
	SetShaderValue(RHICmdList, ShaderRHI, ShadowTextureSize, FVector4((float)AVSMTextureSize, 0, 0, 0));
}

IMPLEMENT_GLOBAL_SHADER(FTressFXClearAVSMBufferPS, "/Engine/Private/TressFX_AVSMClear.usf", "AVSMClearStructuredBuf_PS", SF_Pixel);
