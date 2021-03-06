#pragma once
#include "TressFXShaders.h"
#include "SceneRendering.h"
#include "BasePassRendering.h"
#include "PrimitiveSceneInfo.h"

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

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXDepthsAlphaPS<true>, TEXT("/Engine/Private/TressFXShortCutDepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXDepthsAlphaPS<false>, TEXT("/Engine/Private/TressFXShortCutDepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_VelocityDepthPS
////////////////////////////////////////////////////////////////////////////////

#define IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(OITMode) \
	typedef FTressFXVelocityDepthPS<true, OITMode> FTressFXVelocityDepthPS##WithVelocity##OITMode; \
	typedef FTressFXVelocityDepthPS<false, OITMode> FTressFXVelocityDepthPS##NoVelocity##OITMode; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXVelocityDepthPS##WithVelocity##OITMode, TEXT("/Engine/Private/TressFXVelocityDepthPS.usf"), TEXT("TRessFXVelocityDepthPS"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXVelocityDepthPS##NoVelocity##OITMode, TEXT("/Engine/Private/TressFXVelocityDepthPS.usf"), TEXT("TRessFXVelocityDepthPS"), SF_Pixel);

IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(0); //shortcut, not actually used
IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(1); //kbuffer
IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(2); //opaque
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
	FRHIPixelShader* ShaderRHI = GetPixelShader();																										\
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
	FRHITexture* InAccumInvAlphaSRV,
	FRHITexture* InFragmentColorsTextureSRV,
	TRefCountPtr<FRHIUnorderedAccessView> SceneColorUAV,
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
	FRHIComputeShader* ShaderRHI = GetComputeShader();
	RHICmdList.SetUAVParameter(ShaderRHI, SceneColorTex.GetBaseIndex(), NULL);
}

IMPLEMENT_GLOBAL_SHADER(FTressFXShortCutResolveColorCS, "/Engine/Private/TressFXShortCutResolveColorPS.usf", "ShortcutResolveCS", SF_Compute);



///////////////////////////////////////////////////////////////////////////////
// TressFXFKBufferResolvePS
//////////////////////////////////////////////////////////////////////////////

extern int32 GTressFXKBufferSize;

void FTressFXFKBufferResolvePS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	int32 KBufferSize = FMath::Clamp(static_cast<int32>(GTressFXKBufferSize), MIN_TFX_KBUFFER_SIZE, MAX_TFX_KBUFFER_SIZE);
	OutEnvironment.SetDefine(TEXT("KBUFFER_SIZE"), KBufferSize);
}

void FTressFXFKBufferResolvePS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FRHIShaderResourceView* InLinkedListSRV, FRHITexture* InHeadListSRV)
{
	FRHIPixelShader* ShaderRHI = GetPixelShader();
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

	SetSRVParameter(RHICmdList, ShaderRHI, LinkedListSRV, InLinkedListSRV);
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentListHead, InHeadListSRV);
}

IMPLEMENT_GLOBAL_SHADER(FTressFXFKBufferResolvePS, "/Engine/Private/TressFXPPLLResolve.usf", "ResolvePPLL", SF_Pixel);

///////////////////////////////////////////////////////////////////////////////
// TressFXFKBufferResolveCS
//////////////////////////////////////////////////////////////////////////////

void FTressFXFKBufferResolveCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), FTressFXFKBufferResolveCS::ThreadSizeX);
	OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), FTressFXFKBufferResolveCS::ThreadSizeY);
	int32 KBufferSize = FMath::Clamp(static_cast<int32>(GTressFXKBufferSize), MIN_TFX_KBUFFER_SIZE, MAX_TFX_KBUFFER_SIZE);
	OutEnvironment.SetDefine(TEXT("KBUFFER_SIZE"), KBufferSize);
	OutEnvironment.SetDefine(TEXT("PPLL_COMPUTE_RESOLVE"), TEXT("1"));
}

void FTressFXFKBufferResolveCS::SetParameters(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	FRHIShaderResourceView* InLinkedListSRV,
	FRHITexture* InHeadListSRV,
	TRefCountPtr<FRHIUnorderedAccessView> SceneColorUAV,
	FIntPoint TargetSize
)
{
	const auto ShaderRHI = GetComputeShader();
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

	SetSRVParameter(RHICmdList, ShaderRHI, LinkedListSRV, InLinkedListSRV);
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentListHead, InHeadListSRV);
	SetUAVParameter(RHICmdList, ShaderRHI, SceneColorTex, SceneColorUAV);
	SetShaderValue(RHICmdList, ShaderRHI, TextureSize, FVector4((float)TargetSize.X, (float)TargetSize.Y, 0.0f, 0.0f));
}

void FTressFXFKBufferResolveCS::UnsetParameters(FRHICommandList& RHICmdList)
{
	FRHIComputeShader* ShaderRHI = GetComputeShader();
	RHICmdList.SetUAVParameter(ShaderRHI, SceneColorTex.GetBaseIndex(), NULL);
}

IMPLEMENT_GLOBAL_SHADER(FTressFXFKBufferResolveCS, "/Engine/Private/TressFXPPLLResolve.usf", "ResolvePPLL_CS", SF_Compute);
