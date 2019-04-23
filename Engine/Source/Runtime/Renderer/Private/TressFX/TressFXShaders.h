#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "MeshMaterialShader.h"
#include "TressFX/TressFXSceneProxy.h"
#include "ShaderBaseClasses.h"
#include "GlobalShader.h"


struct ETressFXPass
{
	enum Type {
		DepthsAlpha,
		ResolveDepths,
		DepthsVelocity,
		ResolveVelocity,
		FillColor_Shortcut,
		FillColor_KBuffer,

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
		tAccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("tAccumInvAlpha"));
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
		Ar << DepthTexture << tAccumInvAlpha;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget> AccumInvAlpha, TRefCountPtr<IPooledRenderTarget> HairSceneDepth)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		SetTextureParameter(RHICmdList, GetPixelShader(), DepthTexture, HairSceneDepth->GetRenderTargetItem().TargetableTexture);
		SetTextureParameter(RHICmdList, GetPixelShader(), tAccumInvAlpha, AccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);
	}

	FShaderResourceParameter DepthTexture;
	FShaderResourceParameter tAccumInvAlpha;
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
		// Only the local vertex factory supports the position-only stream
		if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FTressFXVertexFactory"), FNAME_Find)) && (Material->IsUsedWithTressFX() || Material->IsSpecialEngineMaterial()))
		{
			return true;
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
		OutEnvironment.SetDefine(TEXT("TFX_PPLL"), TFXRenderType == ETressFXRenderType::KBuffer ? 1 : 0);
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FTressFXVelocityDepthPS() {}

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FTressFXVertexFactory"), FNAME_Find)) && (Material->IsUsedWithTressFX() || Material->IsSpecialEngineMaterial()))
		{
			return true;
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
			return true;
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
template<bool bWriteClosestDepth = false>
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
		SceneTextureShaderParameters.Bind(Initializer);
		if(bWriteClosestDepth)
		{
			tAccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("tAccumInvAlpha"));
		}
	}

	FTressFXShortCutResolveDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FragmentDepthsTexture << SceneTextureShaderParameters;
		if (bWriteClosestDepth) 
		{
			Ar << tAccumInvAlpha;
		}
		return bShaderHasOutdatedParameters;	
	}

public:

	FShaderResourceParameter FragmentDepthsTexture;
	FSceneTextureShaderParameters SceneTextureShaderParameters;
	FShaderResourceParameter tAccumInvAlpha;

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
		return TEXT("main");
	}

	FTressFXShortCutResolveColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		FragmentColorsTexture.Bind(Initializer.ParameterMap, TEXT("FragmentColorsTexture"));
		tAccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("tAccumInvAlpha"));
		vFragmentBufferSize.Bind(Initializer.ParameterMap, TEXT("vFragmentBufferSize"));
	}

	FTressFXShortCutResolveColorPS() {}


	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FragmentColorsTexture << tAccumInvAlpha << vFragmentBufferSize;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext);

public:

	FShaderResourceParameter FragmentColorsTexture;
	FShaderResourceParameter tAccumInvAlpha;
	FShaderParameter vFragmentBufferSize;
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


	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FShaderResourceViewRHIParamRef InLinkedListSRV, FTextureRHIParamRef InHeadListSRV);


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
		const FShaderResourceViewRHIParamRef InLinkedListSRV,
		FTextureRHIParamRef InHeadListSRV,
		FSceneRenderTargets& SceneContext,
		FIntPoint TargetSize
	);

	void UnsetParameters(FRHICommandList& RHICmdList);
};


