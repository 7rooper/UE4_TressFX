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

IMPLEMENT_SHADER_TYPE(, FTressFXCopyDepthPS, TEXT("/Engine/Private/TressFXCopyVelocityDepth.usf"), TEXT("CopyDepthPS"), SF_Pixel);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_VS - Main Vertex shader for tressfx
////////////////////////////////////////////////////////////////////////////////


IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_VS<false>, TEXT("/Engine/Private/TressFXShortCut_VS.usf"), TEXT("VF_MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_VS<true>, TEXT("/Engine/Private/TressFXShortCut_VS.usf"), TEXT("VF_MainVS"), SF_Vertex);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_DepthsAlphaPS - Pixel shader for First pass of shortcut
////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_DepthsAlphaPS<true>, TEXT("/Engine/Private/TressFXShortCut_DepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_DepthsAlphaPS<false>, TEXT("/Engine/Private/TressFXShortCut_DepthsAlphaPS.usf"), TEXT("main"), SF_Pixel);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_VelocityDepthPS
////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_VelocityDepthPS<true>, TEXT("/Engine/Private/TressFXVelocityDepthPS.usf"), TEXT("TRessFXVelocityDepthPS"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_VelocityDepthPS<false>, TEXT("/Engine/Private/TressFXVelocityDepthPS.usf"), TEXT("TRessFXVelocityDepthPS"), SF_Pixel);


/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_FillColorPS - Pixel shader for Third pass of shortcut
////////////////////////////////////////////////////////////////////////////////

void TFXFillColorModifyEnvironmentCommon(bool bIsShortcut, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("TFX_SHORTCUT"), bIsShortcut ? TEXT("1") : TEXT("0"));
	OutEnvironment.SetDefine(TEXT("TFX_PPLL"), bIsShortcut ? TEXT("0") : TEXT("1"));
}

FTressFX_FillColorPS<true>::FTressFX_FillColorPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
{
	tressfxShadeParameters.Bind(Initializer.ParameterMap, TEXT("tressfxShadeParameters"));
	BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);
}

//PPLL
FTressFX_FillColorPS<false>::FTressFX_FillColorPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
{
	tressfxShadeParameters.Bind(Initializer.ParameterMap, TEXT("tressfxShadeParameters"));
	BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);
	//JAKETODO
}

void FTressFX_FillColorPS<true>::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	TFXFillColorModifyEnvironmentCommon(true, Platform, Material, OutEnvironment);
}

//PPLL
void FTressFX_FillColorPS<false>::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	TFXFillColorModifyEnvironmentCommon(false, Platform, Material, OutEnvironment);
}

void FTressFX_FillColorPS<true>::GetShaderBindings(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const FMeshPassProcessorRenderState& DrawRenderState,
	const FTressFXShaderElementData& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings) const
{
	FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
	const FTressFXSceneProxy* TFXProxy = (const FTressFXSceneProxy*)PrimitiveSceneProxy;
	ShaderBindings.Add(tressfxShadeParameters, TFXProxy->TressFXHairObject->ShadeParametersUniformBuffer);
}
//PPLL
void FTressFX_FillColorPS<false>::GetShaderBindings(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material,
	const FMeshPassProcessorRenderState& DrawRenderState,
	const FTressFXShaderElementData& ShaderElementData,
	FMeshDrawSingleShaderBindings& ShaderBindings) const
{
	FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
	const FTressFXSceneProxy* TFXProxy = (const FTressFXSceneProxy*)PrimitiveSceneProxy;
	ShaderBindings.Add(tressfxShadeParameters, TFXProxy->TressFXHairObject->ShadeParametersUniformBuffer);
	//JAKETODO
}

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_FillColorPS<true>, TEXT("/Engine/Private/TressFX_FillColorPS.usf"), TEXT("main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FTressFX_FillColorPS<false>, TEXT("/Engine/Private/TressFX_FillColorPS.usf"), TEXT("main"), SF_Pixel);

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCut_ResolveDepthPS - pixel shader for 2nd pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

void FTressFXShortCut_ResolveDepthPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentDepthsTexture, SceneContext.TressFXFragmentDepthsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SceneTextureShaderParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
}

IMPLEMENT_SHADER_TYPE(, FTressFXShortCut_ResolveDepthPS, TEXT("/Engine/Private/TressFXShortCut_ResolveDepthPS.usf"), TEXT("main"), SF_Pixel);

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCut_ResolveColorPS - pixel shader for final pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

void FTressFXShortCut_ResolveColorPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

	SetTextureParameter(RHICmdList, GetPixelShader(), FragmentColorsTexture, SceneContext.TressFXFragmentColorsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SetTextureParameter(RHICmdList, GetPixelShader(), tAccumInvAlpha, SceneContext.TressFXAccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);

	FIntPoint BufferSize = View.ViewRect.Size();
	SetShaderValue(RHICmdList, GetPixelShader(), vFragmentBufferSize, FVector4(BufferSize.X, BufferSize.Y, 0, 0));
}

IMPLEMENT_SHADER_TYPE(, FTressFXShortCut_ResolveColorPS, TEXT("/Engine/Private/TressFXShortCut_ResolveColorPS.usf"), TEXT("main"), SF_Pixel);

