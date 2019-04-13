#pragma once
#include "TressFXShaders.h"
#include "SceneRendering.h"
#include "BasePassRendering.h"
#include "PrimitiveSceneInfo.h"
#include "TressFX/TressFXSceneProxy.h"

extern TAutoConsoleVariable<int32> CVarTressFXKBufferSize;


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
//  TTressFX_ShortCutVS - Vertex Shader for shortcut
////////////////////////////////////////////////////////////////////////////////


IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TTressFX_ShortCutVS<false>, TEXT("/Engine/Private/TressFXShortCut_VS.usf"), TEXT("VF_MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TTressFX_ShortCutVS<true>, TEXT("/Engine/Private/TressFXShortCut_VS.usf"), TEXT("VF_MainVS"), SF_Vertex);


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


FTressFX_FillColorPS::FTressFX_FillColorPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
{
	tressfxShadeParameters.Bind(Initializer.ParameterMap, TEXT("tressfxShadeParameters"));
	BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);
}

void FTressFX_FillColorPS::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
}

void FTressFX_FillColorPS::GetShaderBindings(
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

	if (ShaderElementData.ForwardLightDataBuffer.IsValid() && ShaderElementData.ForwardLightDataBuffer->IsValid())
	{
		//not sure if this is needed, I THINK its included in the opaque bass pass buffer shared params...
		ShaderBindings.Add(GetUniformBufferParameter<FForwardLightData>(), ShaderElementData.ForwardLightDataBuffer);
	}
}

IMPLEMENT_MATERIAL_SHADER_TYPE(, FTressFX_FillColorPS, TEXT("/Engine/Private/TressFX_FillColorPS.usf"), TEXT("main"), SF_Pixel);


/////////////////////////////////////////////////////////////////////////////////
//  JAKETODO
////////////////////////////////////////////////////////////////////////////////


//bool FTressFXPPLLGather2_PS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//{
//	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//}
//
//void FTressFXPPLLGather2_PS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
//{
//	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//	FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
//	OutEnvironment.SetDefine(TEXT("KBUFFER_SIZE"), CVarTressFXKBufferSize.GetValueOnRenderThread());
//}
//
//const TCHAR* FTressFXPPLLGather2_PS::GetSourceFilename()
//{
//	return TEXT("TressFXPPLLGather");
//}
//
//const TCHAR* FTressFXPPLLGather2_PS::GetFunctionName()
//{
//	return TEXT("PPLLGather_MainPS");
//}
//
//FTressFXPPLLGather2_PS::FTressFXPPLLGather2_PS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//	: FGlobalShader(Initializer)
//{
//
//	tFragmentListHead.Bind(Initializer.ParameterMap, TEXT("tFragmentListHead"));
//	LinkedListSRV.Bind(Initializer.ParameterMap, TEXT("LinkedListSRV"));
//	g_vViewport.Bind(Initializer.ParameterMap, TEXT("g_vViewport"));
//	g_mInvViewProj.Bind(Initializer.ParameterMap, TEXT("g_mInvViewProj"));
//	g_vEye.Bind(Initializer.ParameterMap, TEXT("g_vEye"));
//}
//
//
//bool FTressFXPPLLGather2_PS::Serialize(FArchive& Ar)
//{
//	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//	Ar << tFragmentListHead << LinkedListSRV << g_vViewport << g_mInvViewProj << g_vEye;
//
//	return bShaderHasOutdatedParameters;
//}
//
//
//void FTressFXPPLLGather2_PS::SetShaderParams(FRHICommandList& RHICmdList, const FViewInfo& view, FShaderResourceViewRHIParamRef ParamLinkedListSRV, FTextureRHIParamRef HeadListSRV)
//{
//
//	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
//	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, view.ViewUniformBuffer);
//
//	FIntRect ViewRect = view.UnscaledViewRect;
//	SetShaderValue(RHICmdList, ShaderRHI, g_mInvViewProj, view.ViewMatrices.GetInvViewProjectionMatrix());
//	SetShaderValue(RHICmdList, ShaderRHI, g_vViewport, FVector4(0, 0, ViewRect.Width(), ViewRect.Height()));
//	SetShaderValue(RHICmdList, ShaderRHI, g_vEye, view.ViewLocation);
//	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), view.ViewUniformBuffer);
//
//	SetSRVParameter(RHICmdList, ShaderRHI, LinkedListSRV, ParamLinkedListSRV);
//	SetTextureParameter(RHICmdList, ShaderRHI, tFragmentListHead, HeadListSRV);
//
//	SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FForwardLightData>(), view.ForwardLightingResources->ForwardLightDataUniformBuffer);
//}
//
//
//IMPLEMENT_SHADER_TYPE(, FTressFXPPLLGather2_PS, TEXT("/Engine/Private/TressFXPPLLGather.usf"), TEXT("PPLLGather_MainPS"), SF_Pixel);

/////////////////////////////////////////////////////////////////////////////////
//  JAKETODO
////////////////////////////////////////////////////////////////////////////////
//
//FTressFX_PPLLBuildVS::FTressFX_PPLLBuildVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :FMeshMaterialShader(Initializer)
//{
//
//}
//
//bool FTressFX_PPLLBuildVS::ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
//{
//	// Only the local vertex factory supports the position-only stream
//	if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FTressFXVertexFactory"), FNAME_Find)) && (Material->IsUsedWithTressFX() || Material->IsSpecialEngineMaterial()))
//	{
//		return true;
//	}
//
//	return false;
//}
//
//bool FTressFX_PPLLBuildVS::Serialize(FArchive& Ar)
//{
//	const bool result = FMeshMaterialShader::Serialize(Ar);
//
//	return result;
//}
//
//void FTressFX_PPLLBuildVS::SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
//{
//	FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
//}
//
//void FTressFX_PPLLBuildVS::SetParameters(
//	FRHICommandList& RHICmdList,
//	const FMaterialRenderProxy* MaterialRenderProxy,
//	const FMaterial& MaterialResource,
//	const FSceneView& View,
//	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
//	const bool bIsInstancedStereo,
//	const bool bIsInstancedStereoEmulated,
//	const FDrawingPolicyRenderState& DrawRenderState
//)
//{
//	FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, MaterialResource, View, ViewUniformBuffer, DrawRenderState.GetPassUniformBuffer());
//}
//
//
//IMPLEMENT_MATERIAL_SHADER_TYPE(, FTressFX_PPLLBuildVS, TEXT("/Engine/Private/TressFXPPLLBuild.usf"), TEXT("Build_MainVS"), SF_Vertex);
//
//
///////////////////////////////////////////////////////////////////////////////////
////  JAKETODO
//////////////////////////////////////////////////////////////////////////////////
//
//
//
//
//FTressFX_PPLLBuildPS::FTressFX_PPLLBuildPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :	FMeshMaterialShader(Initializer)
//{
//	tRWFragmentListHead.Bind(Initializer.ParameterMap, TEXT("tRWFragmentListHead"));
//	LinkedListUAV.Bind(Initializer.ParameterMap, TEXT("LinkedListUAV"));
//	nNodePoolSize.Bind(Initializer.ParameterMap, TEXT("nNodePoolSize"));
//	tressfxShadeParameters.Bind(Initializer.ParameterMap, TEXT("tressfxShadeParameters"));
//}
//
//
//bool FTressFX_PPLLBuildPS::ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
//{
//	// Only the local vertex factory supports the position-only stream
//	if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FTressFXVertexFactory"), FNAME_Find)) && (Material->IsUsedWithTressFX() || Material->IsSpecialEngineMaterial()))
//	{
//		return true;
//	}
//
//	return false;
//}
//
//	bool FTressFX_PPLLBuildPS::Serialize(FArchive& Ar)
//{
//	const bool result = FMeshMaterialShader::Serialize(Ar);
//
//	Ar << tRWFragmentListHead << LinkedListUAV << nNodePoolSize << tressfxShadeParameters;
//
//	return result;
//}
//
//void FTressFX_PPLLBuildPS::SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
//{
//	FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
//
//	const FTressFXSceneProxy* TFXProxy = (const FTressFXSceneProxy*)Proxy;
//
//	SetUniformBufferParameter(RHICmdList, GetPixelShader(), tressfxShadeParameters, TFXProxy->TressFXHairObject->ShadeParametersUniformBuffer);
//
//}
//
//void FTressFX_PPLLBuildPS::SetParameters(
//	FRHICommandList& RHICmdList,
//	const FMaterialRenderProxy* MaterialRenderProxy,
//	const FMaterial& MaterialResource,
//	const FSceneView& View,
//	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
//	const bool bIsInstancedStereo,
//	const bool bIsInstancedStereoEmulated,
//	const FDrawingPolicyRenderState& DrawRenderState
//)
//{
//	FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, MaterialResource, View, ViewUniformBuffer, DrawRenderState.GetPassUniformBuffer());
//	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
//	//View.UnscaledViewRect.Size().X*View.UnscaledViewRect.Size().Y * 4;
//	auto BufferSize = SceneRenderTargets.GetBufferSizeXY();
//	int32 KBufferSize = CVarTressFXKBufferSize.GetValueOnRenderThread();
//	SetShaderValue(RHICmdList, GetPixelShader(), nNodePoolSize, BufferSize.X*BufferSize.Y * KBufferSize);
//}
//
//IMPLEMENT_MATERIAL_SHADER_TYPE(, FTressFX_PPLLBuildPS, TEXT("/Engine/Private/TressFXPPLLBuild.usf"), TEXT("Build_MainPS"), SF_Pixel);


/////////////////////////////////////////////////////////////////////////////////
//  JAKETODO
////////////////////////////////////////////////////////////////////////////////