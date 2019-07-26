#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "MeshMaterialShader.h"
#include "TressFX/TressFXSceneProxy.h"
#include "ShaderBaseClasses.h"
#include "GlobalShader.h"


#ifndef TRESSFX_AVSM_EMPTY_NODE
	#define TRESSFX_AVSM_EMPTY_NODE 65504.0f
#endif

struct ETressFXPass
{
	enum Type {
		DepthsAlpha,
		ResolveDepths,
		DepthsVelocity,
		ResolveVelocity,
		FillColor_Shortcut,
		Resolve_Shortcut,
		////
		Num,
		Max = (Num - 1)
	};
};

class FTressFXShaderElementData;

/////////////////////////////////////////////////////////////////////////////////
//  FTressFXCopyOpaqueDepthPS
////////////////////////////////////////////////////////////////////////////////

class FTressFXCopyOpaqueDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXCopyOpaqueDepthPS);

	FTressFXCopyOpaqueDepthPS()
	{}

	FTressFXCopyOpaqueDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		DepthTexture.Bind(Initializer.ParameterMap, TEXT("DepthTexture"));
		AccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("AccumInvAlpha"));
	}

	const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFXCopyVelocityDepth.usf");
	}

	const TCHAR* GetFunctionName()
	{
		return TEXT("CopyOpaqueDepthPs");
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DepthTexture << AccumInvAlpha;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget> InAccumInvAlpha, TRefCountPtr<IPooledRenderTarget> HairSceneDepth)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		SetTextureParameter(RHICmdList, GetPixelShader(), DepthTexture, HairSceneDepth->GetRenderTargetItem().TargetableTexture);
		SetTextureParameter(RHICmdList, GetPixelShader(), AccumInvAlpha, InAccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);
	}

	FShaderResourceParameter DepthTexture;
	FShaderResourceParameter AccumInvAlpha;
};


/////////////////////////////////////////////////////////////////////////////////
//  FTressFXCopyDepthPS - Simple Depth Copy Shader
////////////////////////////////////////////////////////////////////////////////

class FTressFXCopyDepthPS : public FGlobalShader
{

	DECLARE_GLOBAL_SHADER(FTressFXCopyDepthPS)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static const TCHAR* GetSourceFilename();
	static const TCHAR* GetFunctionName();
	FTressFXCopyDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	FTressFXCopyDepthPS() {}


	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View);

public:

	FSceneTextureShaderParameters SceneTextureShaderParameters;
};



/////////////////////////////////////////////////////////////////////////////////
//  FTressFXVS - Vertex Shader for shortcut
////////////////////////////////////////////////////////////////////////////////


template <bool bCalcVelocity>
class FTressFXVS : public FMeshMaterialShader
{

	DECLARE_SHADER_TYPE(FTressFXVS, MeshMaterial)

public:
	FTressFXVS() {}

	FTressFXVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		vFragmentBufferSize.Bind(Initializer.ParameterMap, TEXT("vFragmentBufferSize"));
		PreviousLocalToWorldParameter.Bind(Initializer.ParameterMap, TEXT("PreviousLocalToWorld"));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bCalcVelocity ? 1 : 0);
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
		Ar << vFragmentBufferSize;
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
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
		ShaderBindings.Add(vFragmentBufferSize, ShaderElementData.FragmentBufferSize);
	}

public:
	FShaderParameter vFragmentBufferSize;
	FShaderParameter PreviousLocalToWorldParameter;

private:

};

/////////////////////////////////////////////////////////////////////////////////
//  FTressFXVelocityDepthPS renders depths and velocity, optionally
////////////////////////////////////////////////////////////////////////////////


template <bool bCalcVelocity, int32 TFXRenderType>
class FTressFXVelocityDepthPS : public FMeshMaterialShader
{

	DECLARE_SHADER_TYPE(FTressFXVelocityDepthPS, MeshMaterial)

public:

	FTressFXVelocityDepthPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{

	}


	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bCalcVelocity ? 1 : 0 );
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FTressFXVelocityDepthPS() {}

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
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
	}

public:

};


/////////////////////////////////////////////////////////////////////////////////
//  FTressFXDepthsAlphaPS - Pixel shader for First pass of shortcut
////////////////////////////////////////////////////////////////////////////////


template<bool bNeedsVelocity>
class FTressFXDepthsAlphaPS : public FMeshMaterialShader
{

	DECLARE_SHADER_TYPE(FTressFXDepthsAlphaPS, MeshMaterial)

public:

	FTressFXDepthsAlphaPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		RWFragmentDepthsTexture.Bind(Initializer.ParameterMap, TEXT("RWFragmentDepthsTexture"));
		gListTexSegmentNodesUAV.Bind(Initializer.ParameterMap, TEXT("gListTexSegmentNodesUAV"));
		gListTexFirstSegmentNodeAddressUAV.Bind(Initializer.ParameterMap, TEXT("gListTexFirstSegmentNodeAddressUAV"));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bNeedsVelocity ? 1 : 0);
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FTressFXDepthsAlphaPS() {}

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
		Ar << RWFragmentDepthsTexture << gListTexSegmentNodesUAV << gListTexFirstSegmentNodeAddressUAV;
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
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
	}

public:

	FRWShaderParameter RWFragmentDepthsTexture;
	FRWShaderParameter gListTexSegmentNodesUAV; //actual nodes
	FRWShaderParameter gListTexFirstSegmentNodeAddressUAV; //addresses

};


///////////////////////////////////////////////////////////////////////////////////
////  Resolve velocity values from tressfx intermediate target to ue4
//////////////////////////////////////////////////////////////////////////////////

class FTressFXResolveVelocityPs : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXResolveVelocityPs);

	FTressFXResolveVelocityPs()
	{}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFXCopyVelocityDepth.usf");
	}

	const TCHAR* GetFunctionName()
	{
		return TEXT("ResolveVelocityPS");
	}

	FTressFXResolveVelocityPs(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VelocityTexture;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget> TressFXVelocityRT)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), VelocityTexture, TressFXVelocityRT->GetRenderTargetItem().TargetableTexture);
	}

	FShaderResourceParameter VelocityTexture;
};

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveDepthPS
//////////////////////////////////////////////////////////////////////////////////
template<bool bWriteClosestDepth = false, bool bWriteShadingModelToGBuffer = false>
class FTressFXShortCutResolveDepthPS : public FGlobalShader
{

	DECLARE_GLOBAL_SHADER(FTressFXShortCutResolveDepthPS)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("WRITE_CLOSEST_DEPTH"), bWriteClosestDepth ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("WRITE_GBUFFER_SHADING_MODEL"), bWriteShadingModelToGBuffer ? TEXT("1") : TEXT("0"));
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFXShortCutResolveDepthPS.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("main");
	}


	FTressFXShortCutResolveDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		FragmentDepthsTexture.Bind(Initializer.ParameterMap, TEXT("FragmentDepthsTexture"));
		MinAlphaForSceneDepth.Bind(Initializer.ParameterMap, TEXT("MinAlphaForSceneDepth"));
		SceneTextureShaderParameters.Bind(Initializer);
		if(bWriteClosestDepth)
		{
			AccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("AccumInvAlpha"));
		}
		if (bWriteShadingModelToGBuffer) 
		{
			RWGBufferB.Bind(Initializer.ParameterMap, TEXT("RWGBufferB"));
		}
	}

	FTressFXShortCutResolveDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext, float MinAlphaForShadow);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FragmentDepthsTexture << SceneTextureShaderParameters << MinAlphaForSceneDepth;
		if (bWriteClosestDepth) 
		{
			Ar << AccumInvAlpha;
		}
		if(bWriteShadingModelToGBuffer)
		{
			Ar << RWGBufferB;
		}
		return bShaderHasOutdatedParameters;	
	}

public:

	FShaderResourceParameter FragmentDepthsTexture;
	FSceneTextureShaderParameters SceneTextureShaderParameters;
	FShaderResourceParameter AccumInvAlpha;
	FRWShaderParameter RWGBufferB;
	FShaderParameter MinAlphaForSceneDepth;

};


///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveColorPS
//////////////////////////////////////////////////////////////////////////////////


class FTressFXShortCutResolveColorPS : public FGlobalShader
{

	DECLARE_GLOBAL_SHADER(FTressFXShortCutResolveColorPS)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters & Parameters, FShaderCompilerEnvironment & OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFXShortCutResolveColorPS.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("ShortcutResolvePS");
	}

	FTressFXShortCutResolveColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		FragmentColorsTexture.Bind(Initializer.ParameterMap, TEXT("FragmentColorsTexture"));
		AccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("AccumInvAlpha"));
	}

	FTressFXShortCutResolveColorPS() {}


	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FragmentColorsTexture << AccumInvAlpha;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext);

public:

	FShaderResourceParameter FragmentColorsTexture;
	FShaderResourceParameter AccumInvAlpha;
};

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXShortCutResolveColorCS - compute shader for final pass of shortcut
//////////////////////////////////////////////////////////////////////////////////

class FTressFXShortCutResolveColorCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXShortCutResolveColorCS)

public:

	static const uint8 ThreadSizeX = 32;
	static const uint8 ThreadSizeY = 32;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters & Parameters, FShaderCompilerEnvironment & OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHORTCUT_COMPUTE_RESOLVE"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), FTressFXShortCutResolveColorCS::ThreadSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), FTressFXShortCutResolveColorCS::ThreadSizeY);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFXShortCutResolveColorPS.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("ShortcutResolveCS");
	}

	FTressFXShortCutResolveColorCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		FragmentColorsTexture.Bind(Initializer.ParameterMap, TEXT("FragmentColorsTexture"));
		AccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("AccumInvAlpha"));
		SceneColorTex.Bind(Initializer.ParameterMap, TEXT("SceneColorTex"));
		TextureSize.Bind(Initializer.ParameterMap, TEXT("TextureSize"));
	}

	FTressFXShortCutResolveColorCS() {}


	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FragmentColorsTexture << AccumInvAlpha << TextureSize << SceneColorTex;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FTextureRHIParamRef InAccumInvAlphaSRV,
		const FTextureRHIParamRef InFragmentColorsTextureSRV,
		const FUnorderedAccessViewRHIRef SceneColorUAV,
		FIntPoint TargetSize
	);

	void UnsetParameters(FRHICommandList& RHICmdList);

public:

	FShaderResourceParameter FragmentColorsTexture;
	FShaderResourceParameter AccumInvAlpha;
	FShaderParameter TextureSize;
	FShaderResourceParameter SceneColorTex;

};

void TressFXAVSMModifyCompilationEnvironmentCommon(const FGlobalShaderPermutationParameters & Parameters, FShaderCompilerEnvironment & OutEnvironment);

///////////////////////////////////////////////////////////////////////////////////
////  FTressFXClearAVSMBufferPS
//////////////////////////////////////////////////////////////////////////////////

class FTressFXClearAVSMBufferPS : public FGlobalShader
{

	DECLARE_GLOBAL_SHADER(FTressFXClearAVSMBufferPS)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters & Parameters, FShaderCompilerEnvironment & OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		TressFXAVSMModifyCompilationEnvironmentCommon(Parameters, OutEnvironment);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFX_AVSMClear.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("AVSMClearStructuredBuf_PS");
	}

	FTressFXClearAVSMBufferPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		gAVSMStructBufUAV.Bind(Initializer.ParameterMap, TEXT("gAVSMStructBufUAV"));
		ShadowTextureSize.Bind(Initializer.ParameterMap, TEXT("ShadowTextureSize"));
	}

	FTressFXClearAVSMBufferPS() {}


	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << gAVSMStructBufUAV << ShadowTextureSize;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View);

public:

	FRWShaderParameter gAVSMStructBufUAV;
	FShaderParameter ShadowTextureSize;
};