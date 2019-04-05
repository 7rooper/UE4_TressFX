#pragma once
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "ScreenRendering.h"
#include "TressFX/TressFXSceneProxy.h"
#include "UniformBuffer.h"
#include "BasePassRendering.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "ScreenRendering.h"
#include "TressFX/TressFXSceneProxy.h"
#include "UniformBuffer.h"
#include "TressFX/TressFXSimulation.h"
#include "BasePassRendering.h"
#include "TressFX/TressFXTypes.h"
#include "Engine/TressFXAsset.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
//
//class FTressFXShortCut_FillColorsPS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFXShortCut_FillColorsPS, Global)
//
//public:
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
//	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
//
//
//	static const TCHAR* GetSourceFilename();
//
//	static const TCHAR* GetFunctionName();
//
//	FTressFXShortCut_FillColorsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
//
//	FTressFXShortCut_FillColorsPS() {}
//
//
//	// FShader interface.
//	virtual bool Serialize(FArchive& Ar) override;
//
//	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext, FTressFXSceneProxy* Proxy);
//
//
//public:
//
//	FShaderParameter g_vViewport;
//	FShaderParameter g_mInvViewProj;
//	FForwardLightingParameters ForwardLightingParameters;
//	//FShaderResourceParameter FragmentDepthsTextureSRV
//};

class FTressFXShortCut_ResolveColorPS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FTressFXShortCut_ResolveColorPS, Global)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FTressFXShortCut_ResolveColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	FTressFXShortCut_ResolveColorPS() {}


	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext);

public:

	FShaderResourceParameter FragmentColorsTexture;
	FShaderResourceParameter tAccumInvAlpha;
	FShaderParameter vFragmentBufferSize;
};


class FTressFXShortCut_ResolveDepthPS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FTressFXShortCut_ResolveDepthPS, Global)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);


	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FTressFXShortCut_ResolveDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	FTressFXShortCut_ResolveDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;

public:

	FShaderResourceParameter FragmentDepthsTexture;
	FSceneTextureShaderParameters SceneTextureShaderParameters;

};
