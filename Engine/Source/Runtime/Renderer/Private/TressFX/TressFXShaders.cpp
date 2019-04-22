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

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXDepthsAlphaPS<true>, TEXT("/Engine/Private/TressFXShortCutDepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFXDepthsAlphaPS<false>, TEXT("/Engine/Private/TressFXShortCutDepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);


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
IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER(2); //kbuffer
#undef IMPLEMENT_TRESSFX_DEPTHSVELOCITY_SHADER

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveDepthPS - pixel shader for 2nd pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

void FTressFXShortCutResolveDepthPS<false>::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentDepthsTexture, SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SceneTextureShaderParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
}

void FTressFXShortCutResolveDepthPS<true>::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentDepthsTexture, SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SceneTextureShaderParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
	SetTextureParameter(RHICmdList, ShaderRHI, tAccumInvAlpha, SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);
}


IMPLEMENT_GLOBAL_SHADER(FTressFXShortCutResolveDepthPS<true>, "/Engine/Private/TressFXShortCutResolveDepthPS.usf", "main", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FTressFXShortCutResolveDepthPS<false>, "/Engine/Private/TressFXShortCutResolveDepthPS.usf", "main", SF_Pixel);

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveColorPS - pixel shader for final pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

void FTressFXShortCutResolveColorPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

	SetTextureParameter(RHICmdList, GetPixelShader(), FragmentColorsTexture, SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SetTextureParameter(RHICmdList, GetPixelShader(), tAccumInvAlpha, SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);

	FIntPoint BufferSize = View.ViewRect.Size();
	SetShaderValue(RHICmdList, GetPixelShader(), vFragmentBufferSize, FVector4(BufferSize.X, BufferSize.Y, 0, 0));
}

IMPLEMENT_GLOBAL_SHADER(FTressFXShortCutResolveColorPS, "/Engine/Private/TressFXShortCutResolveColorPS.usf", "main", SF_Pixel);

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

void FTressFXFKBufferResolvePS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FShaderResourceViewRHIParamRef InLinkedListSRV, FTextureRHIParamRef InHeadListSRV)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

	SetSRVParameter(RHICmdList, ShaderRHI, LinkedListSRV, InLinkedListSRV);
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentListHead, InHeadListSRV);
}

IMPLEMENT_GLOBAL_SHADER(FTressFXFKBufferResolvePS, "/Engine/Private/TressFXPPLLResolve.usf", "ResolvePPLL", SF_Pixel);
