#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "GlobalShader.h"


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
//  FTressFXCopyHairDepthPS
////////////////////////////////////////////////////////////////////////////////

class FTressFXCopyHairDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXCopyHairDepthPS);

	FTressFXCopyHairDepthPS()
	{}

	FTressFXCopyHairDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer): FGlobalShader(Initializer)
	{
		DepthTexture.Bind(Initializer.ParameterMap, TEXT("DepthTexture"));
		StencilTexture.Bind(Initializer.ParameterMap, TEXT("StencilTexture"));
	}

	const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/TressFXCopyVelocityDepth.usf");
	}

	const TCHAR* GetFunctionName()
	{
		return TEXT("CopyHairDepthPs");
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
		Ar << DepthTexture << StencilTexture;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FShaderResourceViewRHIRef StencilSRV, TRefCountPtr<IPooledRenderTarget> HairSceneDepth)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		SetTextureParameter(RHICmdList, GetPixelShader(), DepthTexture, HairSceneDepth->GetRenderTargetItem().TargetableTexture);
		SetSRVParameter(RHICmdList, GetPixelShader(), StencilTexture, StencilSRV);
	}

	FShaderResourceParameter DepthTexture;
	FShaderResourceParameter StencilTexture;
};


/////////////////////////////////////////////////////////////////////////////////
//  FTressFXCopyDepthPS - Simple Depth Copy Shader
////////////////////////////////////////////////////////////////////////////////

class FTressFXCopyDepthPS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FTressFXCopyDepthPS, Global)

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
//  TTressFX_ShortCutVS - Vertex Shader for shortcut
////////////////////////////////////////////////////////////////////////////////

class TTressFX_ShortCutVSShaderElementData : public FMeshMaterialShaderElementData
{
public:
	TTressFX_ShortCutVSShaderElementData(FVector4 InFragmentBufferSize)
		: FragmentBufferSize(InFragmentBufferSize)
	{
	}

	FVector4 FragmentBufferSize;
};


template <bool bCalcVelocity>
class TTressFX_ShortCutVS : public FMeshMaterialShader
{

	DECLARE_SHADER_TYPE(TTressFX_ShortCutVS, MeshMaterial)

public:
	TTressFX_ShortCutVS() {}

	TTressFX_ShortCutVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
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

	//void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatch& Mesh, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
	//{
	//	FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	//}

	//void SetParameters(
	//	FRHICommandList& RHICmdList,
	//	const FMaterialRenderProxy* MaterialRenderProxy,
	//	const FMaterial& MaterialResource,
	//	const FSceneView& View,
	//	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	//	const bool bIsInstancedStereo,
	//	const bool bIsInstancedStereoEmulated,
	//	const FMeshPassProcessorRenderState& DrawRenderState
	//)
	//{
	//	FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, MaterialResource, View, ViewUniformBuffer, DrawRenderState.GetPassUniformBuffer());
	//	auto ViewSize = View.UnscaledViewRect.Size();
	//	FVector4 FragmentBufferSize(ViewSize.X, ViewSize.Y, ViewSize.X*ViewSize.Y, 0);
	//	SetShaderValue(RHICmdList, GetVertexShader(), vFragmentBufferSize, FragmentBufferSize);
	//}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const TTressFX_ShortCutVSShaderElementData& ShaderElementData,
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
//  FTressFX_VelocityDepthPS
////////////////////////////////////////////////////////////////////////////////

class FTressFX_VelocityDepthPSShaderElementData : public FMeshMaterialShaderElementData
{
public:
	FTressFX_VelocityDepthPSShaderElementData(FIntRect InViewRect)
		: ViewRect(InViewRect)
	{
	}

	FIntRect ViewRect;
};


template <bool bCalcVelocity>
class FTressFX_VelocityDepthPS : public FMeshMaterialShader
{

	DECLARE_SHADER_TYPE(FTressFX_VelocityDepthPS, MeshMaterial)

public:

	FTressFX_VelocityDepthPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		g_vViewport.Bind(Initializer.ParameterMap, TEXT("g_vViewport"));
	}


	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bCalcVelocity ? 1 : 0 );
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FTressFX_VelocityDepthPS() {}

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
		Ar << g_vViewport;
		return result;
	}

	//void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
	//{
	//	FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	//}

	//void SetParameters(
	//	FRHICommandList& RHICmdList,
	//	const FMaterialRenderProxy* MaterialRenderProxy,
	//	const FMaterial& MaterialResource,
	//	const FSceneView& View,
	//	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	//	const bool bIsInstancedStereo,
	//	const bool bIsInstancedStereoEmulated,
	//	const FMeshPassProcessorRenderState& DrawRenderState
	//)
	//{
	//	FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, MaterialResource, View, ViewUniformBuffer, DrawRenderState.GetPassUniformBuffer());
	//	FIntRect ViewRect = View.UnscaledViewRect;
	//	SetShaderValue(RHICmdList, GetPixelShader(), g_vViewport, FVector4(0, 0, ViewRect.Width(), ViewRect.Height()));
	//}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FTressFX_VelocityDepthPSShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		//JAKETODO, just use view from uniform buffer in shader, this isnt needed
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
		ShaderBindings.Add(g_vViewport, FVector4(0, 0, ShaderElementData.ViewRect.Width(), ShaderElementData.ViewRect.Height()));
	}

public:
	FShaderParameter g_vViewport;
};


/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_DepthsAlphaPS - Pixel shader for First pass of shortcut
////////////////////////////////////////////////////////////////////////////////


class FTressFX_DepthsAlphaPSShaderElementData : public FMeshMaterialShaderElementData
{
public:
	FTressFX_DepthsAlphaPSShaderElementData(FIntRect InViewRect)
		: ViewRect(InViewRect)
	{
	}

	FIntRect ViewRect;
};


template<bool bNeedsVelocity>
class FTressFX_DepthsAlphaPS : public FMeshMaterialShader
{

	DECLARE_SHADER_TYPE(FTressFX_DepthsAlphaPS, MeshMaterial)

public:

	FTressFX_DepthsAlphaPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		RWFragmentDepthsTexture.Bind(Initializer.ParameterMap, TEXT("RWFragmentDepthsTexture"));
		g_vViewport.Bind(Initializer.ParameterMap, TEXT("g_vViewport"));
	}


	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_VELOCITY"), bNeedsVelocity ? 1 : 0);
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FTressFX_DepthsAlphaPS() {}

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
		Ar << RWFragmentDepthsTexture << g_vViewport;
		return result;
	}

	//void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
	//{
	//	FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	//}

	//void SetParameters(
	//	FRHICommandList& RHICmdList,
	//	const FMaterialRenderProxy* MaterialRenderProxy,
	//	const FMaterial& MaterialResource,
	//	const FSceneView& View,
	//	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	//	const bool bIsInstancedStereo,
	//	const bool bIsInstancedStereoEmulated,
	//	const FMeshPassProcessorRenderState& DrawRenderState
	//)
	//{
	//	FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, MaterialResource, View, ViewUniformBuffer, DrawRenderState.GetPassUniformBuffer());
	//	FIntRect ViewRect = View.UnscaledViewRect;
	//	SetShaderValue(RHICmdList, GetPixelShader(), g_vViewport, FVector4(0, 0, ViewRect.Width(), ViewRect.Height()));
	//}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FTressFX_VelocityDepthPSShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		//JAKETODO, just use view from uniform buffer in shader, this isnt needed
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
		ShaderBindings.Add(g_vViewport, FVector4(0, 0, ShaderElementData.ViewRect.Width(), ShaderElementData.ViewRect.Height()));
	}

public:

	FRWShaderParameter RWFragmentDepthsTexture;
	FShaderParameter g_vViewport;

};

/////////////////////////////////////////////////////////////////////////////////
//  FTressFX_FillColorPS - Pixel shader for Third pass of shortcut, also will probably use for PPPL first pass
////////////////////////////////////////////////////////////////////////////////


//class FTressFX_FillColorPS : public FMeshMaterialShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFX_FillColorPS, MeshMaterial)
//
//public:
//
//	FTressFX_FillColorPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer);
//	FTressFX_FillColorPS() {}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
//
//	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType);
//
//
//	virtual bool Serialize(FArchive& Ar) override;
//
//	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState);
//
//	void SetParameters(
//		FRHICommandList& RHICmdList,
//		const FMaterialRenderProxy* MaterialRenderProxy,
//		const FMaterial& MaterialResource,
//		const FViewInfo& View,
//		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
//		const bool bIsInstancedStereo,
//		const bool bIsInstancedStereoEmulated,
//		const FDrawingPolicyRenderState& DrawRenderState
//	);
//
//public:
//
//	FShaderParameter g_vViewport;
//	FShaderUniformBufferParameter tressfxShadeParameters;
//};


/////////////////////////////////////////////////////////////////////////////////
//  JAKETODO
////////////////////////////////////////////////////////////////////////////////

//class FTressFXPPLLGather2_PS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFXPPLLGather2_PS, Global);
//
//public:
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
//	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
//
//	static const TCHAR* GetSourceFilename();
//	static const TCHAR* GetFunctionName();
//
//	FTressFXPPLLGather2_PS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
//
//
//	FTressFXPPLLGather2_PS() {}
//
//	virtual bool Serialize(FArchive& Ar) override;
//
//	void SetShaderParams(FRHICommandList& RHICmdList, const FViewInfo& view, FShaderResourceViewRHIParamRef ParamLinkedListSRV, FTextureRHIParamRef HeadListSRV);
//
//
//public:
//
//	FShaderResourceParameter tFragmentListHead;
//	FShaderResourceParameter LinkedListSRV;
//
//	FShaderParameter g_vViewport;
//	FShaderParameter g_mInvViewProj;
//	FShaderParameter g_vEye;
//
//};
//
///////////////////////////////////////////////////////////////////////////////////
////  Resolve velocity values from tressfx to ue4
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
		//DepthTexture.Bind(Initializer.ParameterMap, TEXT("DepthTexture"));
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VelocityTexture;
		//Ar << DepthTexture;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget> TressFXVelocityRT, TRefCountPtr<IPooledRenderTarget> TressFXDepth)
	{
		//FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);
		SetTextureParameter(RHICmdList, GetPixelShader(), VelocityTexture, TressFXVelocityRT->GetRenderTargetItem().TargetableTexture);
		//SetTextureParameter(RHICmdList, GetPixelShader(), DepthTexture, TressFXDepth->GetRenderTargetItem().TargetableTexture);
	}

	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter DepthTexture;
};

///////////////////////////////////////////////////////////////////////////////////
////  JAKETODO
//////////////////////////////////////////////////////////////////////////////////
//
//class FTressFX_PPLLBuildVS : public FMeshMaterialShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFX_PPLLBuildVS, MeshMaterial)
//
//protected:
//
//	FTressFX_PPLLBuildVS() :FMeshMaterialShader() {}
//
//	FTressFX_PPLLBuildVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer);
//
//public:
//
//	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType);
//
//	virtual bool Serialize(FArchive& Ar) override;
//
//	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState);
//	void SetParameters(
//		FRHICommandList& RHICmdList,
//		const FMaterialRenderProxy* MaterialRenderProxy,
//		const FMaterial& MaterialResource,
//		const FSceneView& View,
//		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
//		const bool bIsInstancedStereo,
//		const bool bIsInstancedStereoEmulated,
//		const FDrawingPolicyRenderState& DrawRenderState
//	);
//
//};
//
///////////////////////////////////////////////////////////////////////////////////
////  JAKETODO
//////////////////////////////////////////////////////////////////////////////////
//
//class FTressFX_PPLLBuildPS : public FMeshMaterialShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFX_PPLLBuildPS, MeshMaterial)
//
//protected:
//
//	FTressFX_PPLLBuildPS() {}
//	FTressFX_PPLLBuildPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer);
//
//public:
//
//	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType);
//
//	virtual bool Serialize(FArchive& Ar) override;
//
//	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState);
//
//	void SetParameters(
//		FRHICommandList& RHICmdList,
//		const FMaterialRenderProxy* MaterialRenderProxy,
//		const FMaterial& MaterialResource,
//		const FSceneView& View,
//		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
//		const bool bIsInstancedStereo,
//		const bool bIsInstancedStereoEmulated,
//		const FDrawingPolicyRenderState& DrawRenderState
//	);
//
//private:
//
//	FShaderParameter tRWFragmentListHead;
//	FShaderParameter LinkedListUAV;
//	FShaderParameter nNodePoolSize;
//	FShaderUniformBufferParameter tressfxShadeParameters;
//
//
//};


