#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "GlobalShader.h"


struct ETressFXPass
{
	enum Type {
		DepthsAlpha,
		ResolveDepths,
		DepthsVelocity_Opaque,
		DepthsVelocity_KBuffer,
		ResolveVelocity,
		FillColor_Shortcut,
		Resolve_Shortcut,
		FillColor_KBuffer,
		Resolve_KBuffer,
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

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bCalcVelocity ? 1 : 0);
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
//  FTressFXVelocityDepthPS renders depths and velocity
////////////////////////////////////////////////////////////////////////////////


template <bool bCalcVelocity, int32 OITMode>
class FTressFXVelocityDepthPS : public FMeshMaterialShader
{

	DECLARE_SHADER_TYPE(FTressFXVelocityDepthPS, MeshMaterial)

public:

	FTressFXVelocityDepthPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		if (OITMode == ETressFXOITMode::KBuffer)
		{
			RWGBufferB.Bind(Initializer.ParameterMap, TEXT("RWGBufferB"));
		}
	}


	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bCalcVelocity ? 1 : 0 );
		OutEnvironment.SetDefine(TEXT("TFX_PPLL"), OITMode == ETressFXOITMode::KBuffer ? 1 : 0);
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	FTressFXVelocityDepthPS() {}

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
		if (OITMode == ETressFXOITMode::KBuffer)
		{
			Ar << RWGBufferB;
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
		check(OITMode == ETressFXOITMode::KBuffer || OITMode == ETressFXOITMode::None);
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
	}

public:

	FRWShaderParameter RWGBufferB;

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
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bNeedsVelocity ? 1 : 0);
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}

	FTressFXDepthsAlphaPS() {}

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
		Ar << RWFragmentDepthsTexture;
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
		FRHITexture* InAccumInvAlphaSRV,
		FRHITexture* InFragmentColorsTextureSRV,
		TRefCountPtr<FRHIUnorderedAccessView> SceneColorUAV,
		FIntPoint TargetSize
	);

	void UnsetParameters(FRHICommandList& RHICmdList);

public:

	FShaderResourceParameter FragmentColorsTexture;
	FShaderResourceParameter AccumInvAlpha;
	FShaderParameter TextureSize;
	FShaderResourceParameter SceneColorTex;

};


///////////////////////////////////////////////////////////////////////////////
// TressFXFKBufferResolvePS
//////////////////////////////////////////////////////////////////////////////

class FTressFXFKBufferResolvePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXFKBufferResolvePS);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFXPPLLResolve.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("ResolvePPLL");
	}

	FTressFXFKBufferResolvePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		FragmentListHead.Bind(Initializer.ParameterMap, TEXT("FragmentListHead"));
		LinkedListSRV.Bind(Initializer.ParameterMap, TEXT("LinkedListSRV"));
	}


	FTressFXFKBufferResolvePS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FragmentListHead;
		Ar << LinkedListSRV;
		return bShaderHasOutdatedParameters;
	}


	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FRHIShaderResourceView* InLinkedListSRV, FRHITexture* InHeadListSRV);


public:
	FShaderResourceParameter FragmentListHead;
	FShaderResourceParameter LinkedListSRV;

};

///////////////////////////////////////////////////////////////////////////////
// TressFXFKBufferResolveCS
//////////////////////////////////////////////////////////////////////////////


class FTressFXFKBufferResolveCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXFKBufferResolveCS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	/** Default constructor. */
	FTressFXFKBufferResolveCS() {}

public:
	FShaderResourceParameter SceneColorTex;
	FShaderResourceParameter FragmentListHead;
	FShaderResourceParameter LinkedListSRV;
	FShaderParameter TextureSize;

	static const uint8 ThreadSizeX = 32;
	static const uint8 ThreadSizeY = 32;

	/** Initialization constructor. */
	FTressFXFKBufferResolveCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneColorTex.Bind(Initializer.ParameterMap, TEXT("SceneColorTex"));
		FragmentListHead.Bind(Initializer.ParameterMap, TEXT("FragmentListHead"));
		LinkedListSRV.Bind(Initializer.ParameterMap, TEXT("LinkedListSRV"));
		TextureSize.Bind(Initializer.ParameterMap, TEXT("TextureSize"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << LinkedListSRV << FragmentListHead << SceneColorTex << TextureSize;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		FRHIShaderResourceView* InLinkedListSRV,
		FRHITexture* InHeadListSRV,
		TRefCountPtr<FRHIUnorderedAccessView> SceneColorUAV,
		FIntPoint TargetSize
	);

	void UnsetParameters(FRHICommandList& RHICmdList);
};
