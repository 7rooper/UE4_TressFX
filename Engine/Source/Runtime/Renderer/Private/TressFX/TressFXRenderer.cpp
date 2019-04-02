//
//#include "TressFXRenderer.h"
//#include "ShaderParameters.h"
//#include "RHIStaticStates.h"
//#include "Shader.h"
//#include "StaticBoundShaderState.h"
//#include "SceneUtils.h"
//#include "PostProcess/RenderTargetPool.h"
//#include "PostProcess/SceneRenderTargets.h"
//#include "GlobalShader.h"
//#include "MaterialShaderType.h"
//#include "DrawingPolicy.h"
//#include "MeshMaterialShader.h"
//#include "ShaderBaseClasses.h"
//#include "SceneRendering.h"
//#include "ScreenRendering.h"
//#include "TressFX/TressFXSceneProxy.h"
//#include "UniformBuffer.h"
//#include "ClearQuad.h"
//
//#define TOTEXT(a) TEXT(#a)
//#define BIND_PARAM(p) p.Bind(Initializer.ParameterMap, TOTEXT(p))
//#pragma  optimize ("",off)
//struct FPPLL_Struct
//{
//	uint32		depth;
//	uint32		data;
//	uint32		color;
//	uint32		uNext;
//};
//
//
//class FTressFXPPLLManager
//{
//
//
//public:
//
//
//
//public:
//
//
//	TRefCountPtr<IPooledRenderTarget> PPLLHeads;
//
//	FRWBufferStructured PPLLNodes;
//
//	int32 NodePoolSize;
//
//};
//
//
//class FDepthTestEnabledDSSInitializerRHI : public FDepthStencilStateInitializerRHI
//{
//
//public:
//	FDepthTestEnabledDSSInitializerRHI() :FDepthStencilStateInitializerRHI(true, CF_LessEqual,
//		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
//		false, CF_Always, SO_Keep, SO_Keep, SO_Keep, 0xff, 0xff)
//	{}
//
//};
//
//class FDepthTestEnabledNoDepthWritesStencilWriteIncrementDSSInitializerRHI : public FDepthStencilStateInitializerRHI
//{
//
//public:
//	FDepthTestEnabledNoDepthWritesStencilWriteIncrementDSSInitializerRHI() : FDepthStencilStateInitializerRHI(
//		false, CF_Less,
//		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
//		true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement, 0xff, 0xff, DWM_All)
//	{}
//
//};
//
//class FDepthTestDisabledStencilTestLessDSS : public FDepthStencilStateInitializerRHI
//{
//public:
//	FDepthTestDisabledStencilTestLessDSS() : FDepthStencilStateInitializerRHI(
//		false, CF_LessEqual,
//		true, CF_Less, SO_Keep, SO_Keep, SO_Keep,
//		true, CF_Less, SO_Keep, SO_Keep, SO_Keep, 0xff, 0x00)
//	{}
//
//};
//
//
//struct FBlendStateBlendToBgRenderTarget : public FBlendStateInitializerRHI::FRenderTarget
//{
//
//	FBlendStateBlendToBgRenderTarget() : FBlendStateInitializerRHI::FRenderTarget
//	(BO_Add, BF_One, BF_SourceAlpha,
//		BO_Add, BF_Zero, BF_Zero)
//	{}
//
//};
//
//struct FColorWritesOff : public FBlendStateBlendToBgRenderTarget
//{
//
//	FColorWritesOff() : FBlendStateBlendToBgRenderTarget()
//	{
//		ColorBlendOp = BO_Add;
//		ColorSrcBlend = BF_One;
//		ColorDestBlend = BF_Zero;
//		ColorWriteMask = CW_NONE;
//		AlphaBlendOp = BO_Add;
//		AlphaDestBlend = BF_Zero;
//		AlphaSrcBlend = BF_One;
//
//	}
//
//};
//
////TGlobalResource<FTressFXPPLLManager> TressFXPPLLManager;
//namespace TressFXRenderer
//{
//
//
//	TSharedRef<FTressFXPPLLManager> TressFXPPLLManager(new FTressFXPPLLManager);
//}
//
//class FTressFXBaseVS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFXBaseVS, Global);
//
//public:
//
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//	{
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("/Engine/Private/TressFXStrands");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("VS_RenderHair_AA");
//	}
//
//	FTressFXBaseVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_bThinTip);
//		BIND_PARAM(g_bThinTip);
//		BIND_PARAM(g_Ratio);
//		BIND_PARAM(g_txHairColor);
//		BIND_PARAM(g_samLinearWrap);
//		BIND_PARAM(g_GuideHairVertexPositions);
//		BIND_PARAM(g_GuideHairVertexTangents);
//		BIND_PARAM(g_HairStrandTexCd);
//		//BIND_PARAM(g_NumVerticesPerStrand);
//		BIND_PARAM(g_FiberRadius);
//		BIND_PARAM(tressfxShadeParameters);
//	}
//
//
//	FTressFXBaseVS() {}
//
//
//	// FShader interface.
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//		Ar << g_bThinTip << g_Ratio << g_txHairColor
//			<< g_samLinearWrap
//			<< g_GuideHairVertexPositions
//			<< g_GuideHairVertexTangents
//			<< g_HairStrandTexCd
//			//<< g_NumVerticesPerStrand
//			<< g_FiberRadius
//			<< tressfxShadeParameters;
//		return bShaderHasOutdatedParameters;
//	}
//
//	void SetShaderParms(FRHICommandList& RHICmdList, FTressFXSceneProxy* Proxy, const FViewInfo& view)
//	{
//		FVertexShaderRHIParamRef VertexShaderRHI = GetVertexShader();
//		FSamplerStateRHIParamRef SamplerStatePoint = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap>::GetRHI();
//		//SetTextureParameter(RHICmdList, VertexShaderRHI, VectorFieldTexture, VectorFieldTextureSampler, SamplerStatePoint, VertexFactory->VectorFieldTextureRHI);
//		SetTextureParameter(RHICmdList, GetVertexShader(), g_txHairColor, g_samLinearWrap, SamplerStatePoint, Proxy->Resource->Texture);
//		SetUniformBufferParameter(RHICmdList, GetVertexShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), view.ViewUniformBuffer);
//
//
//		SetUniformBufferParameter(RHICmdList, GetVertexShader(), tressfxShadeParameters, Proxy->Resource->TressFXRenderResources.TressFXShadeParametersBuffer);
//		SetSRVParameter(RHICmdList, GetVertexShader(), g_GuideHairVertexPositions, Proxy->Resource->TressFXRenderResources.GuideHairVertexPositions.SRV);
//		SetSRVParameter(RHICmdList, GetVertexShader(), g_GuideHairVertexTangents, Proxy->Resource->TressFXRenderResources.GuideHairVertexTangents.SRV);
//		SetSRVParameter(RHICmdList, GetVertexShader(), g_HairStrandTexCd, Proxy->Resource->TressFXRenderResources.HairStrandTexCd.SRV);
//		SetShaderValue(RHICmdList, GetVertexShader(), g_bThinTip, true);
//		SetShaderValue(RHICmdList, GetVertexShader(), g_FiberRadius, 0.21f);
//	}
//
//private:
//
//	FShaderParameter g_bThinTip;
//	FShaderParameter g_Ratio;
//	FShaderResourceParameter g_txHairColor;
//	FShaderResourceParameter g_samLinearWrap;
//
//	FShaderResourceParameter g_GuideHairVertexPositions;
//	FShaderResourceParameter g_GuideHairVertexTangents;
//	FShaderResourceParameter g_HairStrandTexCd;
//
//	//FShaderParameter g_NumVerticesPerStrand;
//	FShaderParameter g_FiberRadius;
//	FShaderUniformBufferParameter tressfxShadeParameters;
//};
//
//IMPLEMENT_SHADER_TYPE(, FTressFXBaseVS, TEXT("/Engine/Private/TressFXDepthOnly.usf"), TEXT("VS_RenderHair_AA"), SF_Vertex);
//
//
//
//class FTressFXBasePS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFXBasePS, Global);
//
//public:
//
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//	{
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//	}
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXDepthOnly");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("main_PS");
//	}
//
//	FTressFXBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//
//	}
//
//
//	FTressFXBasePS() {}
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FTressFXBasePS, TEXT("/Engine/Private/TressFXDepthOnly.usf"), TEXT("main_PS"), SF_Pixel);
//
//class FTressFXPPLL_VS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFXPPLL_VS, Global);
//
//public:
//
//
//
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//	{
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("/Engine/Private/TressFXPPLL.usf");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("VS_RenderHair_AA");
//	}
//
//	FTressFXPPLL_VS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_bThinTip);
//		BIND_PARAM(g_bThinTip);
//		BIND_PARAM(g_Ratio);
//		BIND_PARAM(g_txHairColor);
//		BIND_PARAM(g_samLinearWrap);
//		BIND_PARAM(g_GuideHairVertexPositions);
//		BIND_PARAM(g_GuideHairVertexTangents);
//		BIND_PARAM(g_HairStrandTexCd);
//		//BIND_PARAM(g_NumVerticesPerStrand);
//		BIND_PARAM(tressfxShadeParameters);
//		BIND_PARAM(g_FiberRadius);
//	}
//
//
//	FTressFXPPLL_VS() {}
//
//
//	// FShader interface.
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//		Ar << g_bThinTip << g_Ratio << g_txHairColor
//			<< g_samLinearWrap
//			<< g_GuideHairVertexPositions
//			<< g_GuideHairVertexTangents
//			<< g_HairStrandTexCd
//			//<< g_NumVerticesPerStrand
//			<< g_FiberRadius
//			<< tressfxShadeParameters;
//		return bShaderHasOutdatedParameters;
//	}
//
//	void SetShaderParms(FRHICommandList& RHICmdList, FTressFXSceneProxy* Proxy, const FViewInfo& view)
//	{
//		FVertexShaderRHIParamRef VertexShaderRHI = GetVertexShader();
//		FSamplerStateRHIParamRef SamplerStatePoint = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap>::GetRHI();
//		//SetTextureParameter(RHICmdList, VertexShaderRHI, VectorFieldTexture, VectorFieldTextureSampler, SamplerStatePoint, VertexFactory->VectorFieldTextureRHI);
//		SetTextureParameter(RHICmdList, GetVertexShader(), g_txHairColor, g_samLinearWrap, SamplerStatePoint, Proxy->Resource->Texture);
//		SetUniformBufferParameter(RHICmdList, GetVertexShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), view.ViewUniformBuffer);
//		SetUniformBufferParameter(RHICmdList, GetVertexShader(), tressfxShadeParameters, Proxy->Resource->TressFXRenderResources.TressFXShadeParametersBuffer);
//		SetSRVParameter(RHICmdList, GetVertexShader(), g_GuideHairVertexPositions, Proxy->Resource->TressFXRenderResources.GuideHairVertexPositions.SRV);
//		SetSRVParameter(RHICmdList, GetVertexShader(), g_GuideHairVertexTangents, Proxy->Resource->TressFXRenderResources.GuideHairVertexTangents.SRV);
//		SetSRVParameter(RHICmdList, GetVertexShader(), g_HairStrandTexCd, Proxy->Resource->TressFXRenderResources.HairStrandTexCd.SRV);
//		SetShaderValue(RHICmdList, GetVertexShader(), g_bThinTip, true);
//		SetShaderValue(RHICmdList, GetVertexShader(), g_FiberRadius, 0.21f);
//		SetShaderValue(RHICmdList, GetVertexShader(), g_Ratio, 0.5);
//		//SetShaderValue(RHICmdList, GetVertexShader(), g_NumVerticesPerStrand, Proxy->NumVertsPerStrand);
//
//
//	}
//
//private:
//
//	FShaderParameter g_bThinTip;
//	FShaderParameter g_Ratio;
//	FShaderResourceParameter g_txHairColor;
//	FShaderResourceParameter g_samLinearWrap;
//
//	FShaderResourceParameter g_GuideHairVertexPositions;
//	FShaderResourceParameter g_GuideHairVertexTangents;
//	FShaderResourceParameter g_HairStrandTexCd;
//
//	//FShaderParameter g_NumVerticesPerStrand;
//	FShaderParameter g_FiberRadius;
//	FShaderUniformBufferParameter tressfxShadeParameters;
//
//
//
//};
//
//
//IMPLEMENT_SHADER_TYPE(, FTressFXPPLL_VS, TEXT("/Engine/Private/TressFXPPLL.usf"), TOTEXT(VS_RenderHair_AA), SF_Vertex);
//
//
//class FTressFXPPLL_PS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFXPPLL_PS, Global);
//
//public:
//
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//	{
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("/Engine/Private/TressFXPPLL.usf");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("main");
//	}
//
//	FTressFXPPLL_PS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(tRWFragmentListHead);
//		BIND_PARAM(LinkedListUAV);
//		BIND_PARAM(nNodePoolSize);
//	}
//
//
//	FTressFXPPLL_PS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//		Ar << tRWFragmentListHead << LinkedListUAV << nNodePoolSize;
//		return bShaderHasOutdatedParameters;
//	}
//
//	void SetShaderParams(FRHICommandList& RHICmdList, const FViewInfo& view)
//	{
//		SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), view.ViewUniformBuffer);
//		SetShaderValue(RHICmdList, GetPixelShader(), nNodePoolSize, TressFXRenderer::TressFXPPLLManager->NodePoolSize);
//
//	}
//
//public:
//
//
//	FRWShaderParameter tRWFragmentListHead;
//
//	FRWShaderParameter LinkedListUAV;
//
//	FShaderParameter nNodePoolSize;
//};
//
//
//IMPLEMENT_SHADER_TYPE(, FTressFXPPLL_PS, TEXT("/Engine/Private/TressFXPPLL.usf"), TEXT("main"), SF_Pixel);
//
//
//class FTressFXPPLL_Resolve_PS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FTressFXPPLL_Resolve_PS, Global);
//
//public:
//
//
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//	{
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("/Engine/Private/TressFXPPLL_Resolve_PS.usf");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("ResolveLinkedListPS");
//	}
//
//	FTressFXPPLL_Resolve_PS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(tFragmentListHead);
//		BIND_PARAM(LinkedListSRV);
//	}
//
//
//	FTressFXPPLL_Resolve_PS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << tFragmentListHead << LinkedListSRV;
//
//		return bShaderHasOutdatedParameters;
//	}
//
//	void SetShaderParams(FRHICommandList& RHICmdList, const FViewInfo& view, FShaderResourceViewRHIParamRef ParamLinkedListSRV, FTextureRHIParamRef HeadListSRV)
//	{
//		SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), view.ViewUniformBuffer);
//		SetTextureParameter(RHICmdList, GetPixelShader(), tFragmentListHead, HeadListSRV);
//		SetSRVParameter(RHICmdList, GetPixelShader(), LinkedListSRV, ParamLinkedListSRV);
//
//	}
//
//
//public:
//
//	FShaderResourceParameter tFragmentListHead;
//	FShaderResourceParameter LinkedListSRV;
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FTressFXPPLL_Resolve_PS, TEXT("/Engine/Private/TressFXPPLL_Resolve_PS.usf"), TEXT("ResolveLinkedListPS"), SF_Pixel);
//
//
//#pragma region "TressFX Simulation"
//
//
///*
//struct Transforms
//{
//	FMatrix tfm;
//	FVector4 quat;
//};
//
//struct HairToTriangleMapping
//{
//	uint32 meshIndex;          // index to the mesh
//	uint32 triangleIndex;      // index to triangle in the skinned mesh that contains this hair
//	FVector barycentricCoord; // barycentric coordinate of the hair within the triangle
//	uint32 reserved;           // for future use
//};
//
//struct BoneSkinningData
//{
//	FVector4 boneIndex;  // x, y, z and w component are four bone indices per strand
//	FVector4 boneWeight; // x, y, z and w component are four bone weights per strand
//};
//
//class FIntegrationAndGlobalShapeConstraintsCS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FIntegrationAndGlobalShapeConstraintsCS, Global);
//
//public:
//
//
//	static bool ShouldCache(EShaderPlatform Platform)
//	{
//		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
//		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXSimulation");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("IntegrationAndGlobalShapeConstraints");
//	}
//
//	FIntegrationAndGlobalShapeConstraintsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_HairVertexPositions);
//		BIND_PARAM(g_HairVertexPositionsPrev);
//		BIND_PARAM(g_HairVertexPositionsPrevPrev);
//		BIND_PARAM(g_HairVertexTangents);
//		BIND_PARAM(g_BoneSkinningData);
//		BIND_PARAM(g_InitialHairPositions);
//
//	}
//
//
//	FIntegrationAndGlobalShapeConstraintsCS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << g_HairVertexPositions << g_HairVertexPositionsPrev << g_HairVertexPositionsPrevPrev
//			<< g_HairVertexTangents << g_BoneSkinningData << g_InitialHairPositions;
//
//		return bShaderHasOutdatedParameters;
//	}
//
//
//public:
//
//	//StructuredBuffer<float4> g_InitialHairPositions;
//	//StructuredBuffer<BoneSkinningData> g_BoneSkinningData;
//
//	FRWShaderParameter g_HairVertexPositions;
//	FRWShaderParameter g_HairVertexPositionsPrev;
//	FRWShaderParameter g_HairVertexPositionsPrevPrev;
//	FRWShaderParameter g_HairVertexTangents;
//
//	FShaderResourceParameter g_BoneSkinningData;
//	FShaderResourceParameter g_InitialHairPositions;
//
//
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FIntegrationAndGlobalShapeConstraintsCS, TEXT("TressFXSimulation"), TEXT("IntegrationAndGlobalShapeConstraints"), SF_Compute);
//
//
//class FVelocityShockPropagationCS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FVelocityShockPropagationCS, Global);
//
//public:
//
//
//	static bool ShouldCache(EShaderPlatform Platform)
//	{
//		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
//		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXSimulation");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("VelocityShockPropagation");
//	}
//
//	FVelocityShockPropagationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_HairVertexPositions);
//		BIND_PARAM(g_HairVertexPositionsPrev);
//		BIND_PARAM(g_HairVertexPositionsPrevPrev);
//	}
//
//
//	FVelocityShockPropagationCS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << g_HairVertexPositions << g_HairVertexPositionsPrev << g_HairVertexPositionsPrevPrev;
//
//		return bShaderHasOutdatedParameters;
//	}
//
//
//public:
//
//
//	FRWShaderParameter g_HairVertexPositions;
//	FRWShaderParameter g_HairVertexPositionsPrev;
//	FRWShaderParameter g_HairVertexPositionsPrevPrev;
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FVelocityShockPropagationCS, TEXT("TressFXSimulation"), TEXT("VelocityShockPropagation"), SF_Compute);
//
//class FLocalShapeConstraintsCS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FLocalShapeConstraintsCS, Global);
//
//public:
//
//
//	static bool ShouldCache(EShaderPlatform Platform)
//	{
//		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
//		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXSimulation");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("LocalShapeConstraints");
//	}
//
//	FLocalShapeConstraintsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_GlobalRotations);
//		BIND_PARAM(g_HairRefVecsInLocalFrame);
//		BIND_PARAM(g_HairVertexPositions);
//		BIND_PARAM(g_HairVertexTangents);
//	}
//
//
//	FLocalShapeConstraintsCS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << g_GlobalRotations << g_HairRefVecsInLocalFrame << g_HairVertexPositions << g_HairVertexTangents;
//
//
//		return bShaderHasOutdatedParameters;
//	}
//
//
//public:
//
//	FShaderResourceParameter g_GlobalRotations;
//	FShaderResourceParameter g_HairRefVecsInLocalFrame;
//	FRWShaderParameter g_HairVertexPositions;
//	FRWShaderParameter g_HairVertexTangents;
//
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FLocalShapeConstraintsCS, TEXT("TressFXSimulation"), TEXT("LocalShapeConstraints"), SF_Compute);
//
//
//class FLocalShapeConstraintsWithIterationCS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FLocalShapeConstraintsWithIterationCS, Global);
//
//public:
//
//
//	static bool ShouldCache(EShaderPlatform Platform)
//	{
//		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
//		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXSimulation");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("LocalShapeConstraintsWithIteration");
//	}
//
//	FLocalShapeConstraintsWithIterationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_GlobalRotations);
//		BIND_PARAM(g_HairRefVecsInLocalFrame);
//		BIND_PARAM(g_HairVertexPositions);
//		BIND_PARAM(g_HairVertexTangents);
//	}
//
//
//	FLocalShapeConstraintsWithIterationCS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << g_GlobalRotations << g_HairRefVecsInLocalFrame << g_HairVertexPositions << g_HairVertexTangents;
//
//		return bShaderHasOutdatedParameters;
//	}
//
//
//public:
//	//StructuredBuffer<float4> g_GlobalRotations;
//	//StructuredBuffer<float4> g_HairRefVecsInLocalFrame;
//	//RWStructuredBuffer<float4> g_HairVertexPositions;
//	//RWStructuredBuffer<float4> g_HairVertexTangents;
//
//	FShaderResourceParameter g_GlobalRotations;
//	FShaderResourceParameter g_HairRefVecsInLocalFrame;
//	FRWShaderParameter g_HairVertexPositions;
//	FRWShaderParameter g_HairVertexTangents;
//};
//
//IMPLEMENT_SHADER_TYPE(, FLocalShapeConstraintsWithIterationCS, TEXT("TressFXSimulation"), TEXT("LocalShapeConstraintsWithIteration"), SF_Compute);
//
//class FLengthConstriantsWindAndCollisionCS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FLengthConstriantsWindAndCollisionCS, Global);
//
//public:
//
//
//	static bool ShouldCache(EShaderPlatform Platform)
//	{
//		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
//		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXSimulation");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("LengthConstriantsWindAndCollision");
//	}
//
//	FLengthConstriantsWindAndCollisionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_HairVertexPositions);
//		BIND_PARAM(g_HairVertexTangents);
//		BIND_PARAM(g_HairRestLengthSRV);
//	}
//
//
//	FLengthConstriantsWindAndCollisionCS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << g_HairVertexPositions << g_HairVertexTangents << g_HairRestLengthSRV;
//
//		return bShaderHasOutdatedParameters;
//	}
//
//
//public:
//
//	FRWShaderParameter g_HairVertexPositions;
//	FRWShaderParameter g_HairVertexTangents;
//	FShaderResourceParameter g_HairRestLengthSRV;
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FLengthConstriantsWindAndCollisionCS, TEXT("TressFXSimulation"), TEXT("LengthConstriantsWindAndCollision"), SF_Compute);
//
//class FUpdateFollowHairVerticesCS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FUpdateFollowHairVerticesCS, Global);
//
//public:
//
//
//	static bool ShouldCache(EShaderPlatform Platform)
//	{
//		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
//		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXSimulation");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("UpdateFollowHairVertices");
//	}
//
//	FUpdateFollowHairVerticesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_FollowHairRootOffset);
//		BIND_PARAM(g_HairVertexTangents);
//		BIND_PARAM(g_HairVertexPositions);
//	}
//
//
//	FUpdateFollowHairVerticesCS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << g_HairVertexPositions << g_HairVertexTangents << g_FollowHairRootOffset;
//
//		return bShaderHasOutdatedParameters;
//	}
//
//
//public:
//
//	//RWStructuredBuffer<float4> g_HairVertexPositions;
//	//RWStructuredBuffer<float4> g_HairVertexTangents;
//	//StructuredBuffer<float4> g_FollowHairRootOffset;
//
//	FRWShaderParameter g_HairVertexPositions;
//	FRWShaderParameter g_HairVertexTangents;
//	FShaderResourceParameter g_FollowHairRootOffset;
//
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FUpdateFollowHairVerticesCS, TEXT("TressFXSimulation"), TEXT("UpdateFollowHairVertices"), SF_Compute);
//
//class FPrepareFollowHairBeforeTurningIntoGuideCS : public FGlobalShader
//{
//
//	DECLARE_SHADER_TYPE(FPrepareFollowHairBeforeTurningIntoGuideCS, Global);
//
//public:
//
//
//	static bool ShouldCache(EShaderPlatform Platform)
//	{
//		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
//	}
//
//	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
//	{
//		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
//		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
//	}
//
//	static const TCHAR* GetSourceFilename()
//	{
//		return TEXT("TressFXSimulation");
//	}
//
//	static const TCHAR* GetFunctionName()
//	{
//		return TEXT("PrepareFollowHairBeforeTurningIntoGuide");
//	}
//
//	FPrepareFollowHairBeforeTurningIntoGuideCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FGlobalShader(Initializer)
//	{
//		BIND_PARAM(g_HairVertexPositions);
//		BIND_PARAM(g_HairVertexPositionsPrev);
//		BIND_PARAM(tressfxSimParameters);
//	}
//
//
//	FPrepareFollowHairBeforeTurningIntoGuideCS() {}
//
//	virtual bool Serialize(FArchive& Ar) override
//	{
//		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//
//		Ar << g_HairVertexPositions << g_HairVertexPositionsPrev << tressfxSimParameters;
//
//		return bShaderHasOutdatedParameters;
//	}
//
//
//public:
//
//	FRWShaderParameter g_HairVertexPositions;
//	FRWShaderParameter g_HairVertexPositionsPrev;
//	FShaderUniformBufferParameter tressfxSimParameters;
//
//};
//
//IMPLEMENT_SHADER_TYPE(, FPrepareFollowHairBeforeTurningIntoGuideCS, TEXT("TressFXSimulation"), TEXT("PrepareFollowHairBeforeTurningIntoGuide"), SF_Compute);
//*/
//#pragma endregion
//
//
//class FTressFXRenderTargets
//{
//public:
//
//	TRefCountPtr<IPooledRenderTarget> Target;
//	TRefCountPtr<IPooledRenderTarget> KPassTarget;
//	TRefCountPtr<IPooledRenderTarget> Depth;
//};
//
//#define TRESSFX_PPLL_NULL_PTR 0xffffffff
//
//
//extern TGlobalResource<FEmptyVertexDeclaration> GEmptyVertexDeclaration;
//
//extern void DrawRectangle(
//	FRHICommandList& RHICmdList,
//	float X,
//	float Y,
//	float SizeX,
//	float SizeY,
//	float U,
//	float V,
//	float SizeU,
//	float SizeV,
//	FIntPoint TargetSize,
//	FIntPoint TextureSize,
//	FShader* VertexShader,
//	EDrawRectangleFlags Flags,
//	uint32 InstanceCount
//);
//#if 0
//namespace TressFXRenderer
//{
//
//
//	TSharedRef<FTressFXRenderTargets> TressFXRenderTargets(new FTressFXRenderTargets);
//
//
//	void SetupViews(TArray<FViewInfo>& Views)
//	{
//		for (auto& View : Views)
//		{
//			check(View.VisibleTressFX.Num() == 0);
//
//			for (auto* PrimitiveInfo : View.VisibleDynamicPrimitives)
//			{
//				auto& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveInfo->GetIndex()];
//				if (ViewRelevance.bTressFX)
//				{
//					View.VisibleTressFX.Add(PrimitiveInfo);
//				}
//			}
//		}
//	}
//
//	bool ViewsHasTressFX(const TArray<FViewInfo>& Views)
//	{
//		for (auto& View : Views)
//		{
//			if (View.VisibleTressFX.Num())
//				return true;
//		}
//
//		return false;
//	}
//
//	void Draw(FRHICommandList& RHICmdList, FSceneRenderTargets& sceneContext, const FViewInfo& view)
//	{
//		if (!view.ViewState)
//			return;
//#if 0
//		static const FTextureRHIParamRef ClearRenderTargets[MaxSimultaneousRenderTargets] = { nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr };
//
//		/*
//		{
//
//
//			FRHIRenderTargetView RenderTarget(TressFXRenderTargets->Target->GetRenderTargetItem().TargetableTexture);
//			//RenderTarget.LoadAction = ERenderTargetLoadAction::EClear;
//			FRHISetRenderTargetsInfo Info(1, &RenderTarget, FRHIDepthRenderTargetView(sceneContext.GetSceneDepthSurface()));
//			RHICmdList.SetRenderTargetsAndClear(Info);
//
//			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
//			RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());
//			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
//			//RHICmdList.SetBlendState(TStaticBlendState<)
//			{
//				TShaderMapRef<FTressFXBaseVS> VertexShader(view.ShaderMap);
//				TShaderMapRef<FTressFXBasePS> PixelShader(view.ShaderMap);
//				static FGlobalBoundShaderState BoundShaderState;
//				SetGlobalBoundShaderState(RHICmdList, view.GetFeatureLevel(), BoundShaderState, GEmptyVertexDeclaration.VertexDeclarationRHI, *VertexShader,nullptr);
//
//				for (auto InProxy : view.VisibleTressFX)
//				{
//					auto TFXProxy = static_cast<FTressFXSceneProxy*>(InProxy->Proxy);
//
//					if (!TFXProxy || !TFXProxy->Texture.IsValid())
//						continue;
//
//					VertexShader->SetShaderParms(RHICmdList, TFXProxy, view);
//
//					RHICmdList.DrawIndexedPrimitive(TFXProxy->IndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, TFXProxy->GuideHairVertexPositions.Vertices.Num(), 0, TFXProxy->GuideHairVertexPositions.Vertices.Num() / 3, 1);
//				}
//			}
//		}
//		*/
//
//		{
//			FRHIDepthRenderTargetView DepthRenderView(TressFXRenderTargets->Depth->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
//			FRHIRenderTargetView TestTarget(TressFXRenderTargets->Target->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::ELoad);
//			RHICmdList.SetRenderTargets(1, &TestTarget, &DepthRenderView, 0, 0);
//			//RHICmdList.ClearDepthStencilTexture(TressFXRenderTargets->Depth->GetRenderTargetItem().TargetableTexture, EClearDepthStencil::DepthStencil, 0, 0, FIntRect());
//		//	RHICmdList.Re
//			//ClearColorTexture(TressFXRenderTargets->Target->GetRenderTargetItem().TargetableTexture, FLinearColor::Black, FIntRect());
//			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
//			const auto Size = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
//			RHICmdList.SetScissorRect(true, 0, 0, Size.X, Size.Y);
//			//FDepthStencilStateRHIRef DepthTestEnabledNoDepthWritesStencilWriteIncrementDSS = RHICreateDepthStencilState(FDepthTestEnabledNoDepthWritesStencilWriteIncrementDSSInitializerRHI());
//			RHICmdList.SetDepthStencilState(
//				TStaticDepthStencilState<
//				true,
//				CF_GreaterEqual,
//				true,
//				CF_Always,
//				SO_Keep,
//				SO_Keep,
//				SO_SaturatedIncrement,
//				true,
//				CF_Always,
//				SO_Keep,
//				SO_Keep,
//				SO_SaturatedIncrement,
//				0xFF,
//				0xFF,
//				DWM_Zero>::GetRHI());
//
//			//RHICmdList.SetDepthStencilState(DepthTestEnabledNoDepthWritesStencilWriteIncrementDSS);
//
//			FBlendStateRHIRef ColorWritesOff = RHICreateBlendState(FColorWritesOff());
//
//			//RHICmdList.SetBlendState(ColorWritesOff, FLinearColor::Transparent);
//
//			RHICmdList.SetBlendState(TStaticBlendState<>::CreateRHI());
//
//			TShaderMapRef<FTressFXPPLL_VS> VertexShader(view.ShaderMap);
//			TShaderMapRef<FTressFXPPLL_PS> PixelShader(view.ShaderMap);
//
//
//			FRHISetRenderTargetsInfo Info;
//
//
//
//
//
//
//			//Info.UnorderedAccessView[0] = TressFXPPLLManager->PPLLHeads->GetRenderTargetItem().UAV;
//			//Info.UnorderedAccessView[1] = TressFXPPLLManager->PPLLNodes.UAV;
//			//Info.ColorRenderTarget[0] = TestTarget;
//			//Info.NumUAVs = 2;
//			//Info.NumColorRenderTargets = 1;
//			//RHICmdList.SetRenderTargetsAndClear(Info);
//
//
//
//
//
//			// Hook up the geometry volume UAVs
//
//			//Uavs[2] = LightPropagationVolume->GetVplListBufferUav();
//			//Uavs[3] = LightPropagationVolume->GetVplListHeadBufferUav();
//			//SetRenderTargets(RHICmdList, ARRAY_COUNT(ClearRenderTargets), ClearRenderTargets, nullptr, 0, nullptr);
//			FUnorderedAccessViewRHIParamRef Uavs[MaxSimultaneousUAVs] = { TressFXPPLLManager->PPLLHeads->GetRenderTargetItem().UAV, TressFXPPLLManager->PPLLNodes.UAV,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr };
//			RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, Uavs, 2);
//			uint32 UAVClearVal[4] = { TRESSFX_PPLL_NULL_PTR ,TRESSFX_PPLL_NULL_PTR,TRESSFX_PPLL_NULL_PTR,TRESSFX_PPLL_NULL_PTR };
//			FSceneRenderTargetItem UAVItem;
//			ClearUAV(RHICmdList, Uavs[0], view.GetFeatureLevel(), UAVClearVal);
//			RHICmdList.ClearUAV(Uavs[1], { TRESSFX_PPLL_NULL_PTR,TRESSFX_PPLL_NULL_PTR,TRESSFX_PPLL_NULL_PTR,TRESSFX_PPLL_NULL_PTR });
//			static FUAVCountInitializerRHI ZeroUAVCountInitializerRHI(0, 0);
//			SetRenderTargets(RHICmdList, 1, &TestTarget.Texture, TressFXRenderTargets->Depth->GetRenderTargetItem().TargetableTexture, 2, Uavs, false, &ZeroUAVCountInitializerRHI);
//
//
//			static FGlobalBoundShaderState BoundShaderState;
//			SetGlobalBoundShaderState(RHICmdList, view.GetFeatureLevel(), BoundShaderState, GEmptyVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
//
//			PixelShader->SetShaderParams(RHICmdList, view);
//
//			for (auto InProxy : view.VisibleTressFX)
//			{
//				auto TFXProxy = static_cast<FTressFXSceneProxy*>(InProxy->Proxy);
//
//				if (!TFXProxy || !TFXProxy->Resource->Texture.IsValid())
//					continue;
//
//				VertexShader->SetShaderParms(RHICmdList, TFXProxy, view);
//
//				RHICmdList.DrawIndexedInstanced(TFXProxy->Resource->IndexBuffer.IndexBufferRHI, TFXProxy->Resource->mtotalIndices, PT_TriangleList, 0, 0, 1, 0);
//				//RHICmdList.DrawIndexedPrimitive(TFXProxy->IndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 
//				//								TFXProxy->GuideHairVertexPositions.Vertices.Num(), 0, 
//				//								TFXProxy->GuideHairVertexPositions.Vertices.Num() / 3, 1);
//			}
//		}
//		{
//			{
//
//				FUnorderedAccessViewRHIParamRef UavsToReadable[2];
//				UavsToReadable[0] = TressFXPPLLManager->PPLLHeads->GetRenderTargetItem().UAV;
//				UavsToReadable[1] = TressFXPPLLManager->PPLLNodes.UAV;
//				RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UavsToReadable, ARRAY_COUNT(UavsToReadable));
//
//				// Unset render targets
//				FTextureRHIParamRef RenderTargets[1] = { NULL };
//				FUnorderedAccessViewRHIParamRef Uavs[2] = { NULL };
//
//				SetRenderTargets(RHICmdList, ARRAY_COUNT(ClearRenderTargets) - ARRAY_COUNT(Uavs), ClearRenderTargets, nullptr, ARRAY_COUNT(Uavs), Uavs);
//			}
//
//		}
//		{
//			TShaderMapRef<FScreenVS>					ScreenVertexShader(view.ShaderMap);
//			TShaderMapRef<FTressFXPPLL_Resolve_PS>		PixelShader(view.ShaderMap);
//			static FGlobalBoundShaderState BoundState;
//
//
//			FTextureRHIParamRef RenderTargets[1] = { sceneContext.GetSceneColorSurface() };
//
//
//			SetRenderTargets(RHICmdList, 1, RenderTargets, nullptr, 0, nullptr);
//			//RHICmdList.ClearColorTexture(TressFXRenderTargets->KPassTarget->GetRenderTargetItem().TargetableTexture, FLinearColor::Black, FIntRect());
//
//			const auto Size = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
//			RHICmdList.SetViewport(0, 0, 0, Size.X, Size.Y, 1);
//			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
//			RHICmdList.SetScissorRect(true, 0, 0, Size.X, Size.Y);
//			//TStaticDepthStencilState<false,
//			//
//			//RHICmdList.SetDepthStencilState(RHICreateDepthStencilState(FDepthTestEnabledNoDepthWritesStencilWriteIncrementDSSInitializerRHI()));
//
//			RHICmdList.SetDepthStencilState(
//				TStaticDepthStencilState <
//				false, CF_Less,
//				false, CF_Less, SO_Keep, SO_Keep, SO_Keep,
//				false, CF_Less, SO_Keep, SO_Keep, SO_Keep,
//				0xff, 0x00,
//				DWM_All, true>::GetRHI()
//			);
//
//			//	RHICmdList.SetBlendState(TStaticBlendState<>::CreateRHI());
//
//			RHICmdList.SetBlendState(RHICreateBlendState(FBlendStateBlendToBgRenderTarget()), FLinearColor::Transparent, 0x000000ff);
//			SetGlobalBoundShaderState(RHICmdList, view.GetFeatureLevel(), BoundState, GScreenVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);
//			/*
//			RHICmdList.SetDepthStencilState(
//				TStaticDepthStencilState<
//				false,
//				CF_LessEqual,
//				true,
//				CF_Less,
//				SO_Keep,
//				SO_Keep,
//				SO_Keep,
//				true,
//				CF_Less,
//				SO_Keep,
//				SO_Keep,
//				SO_Keep,
//				0xFF,
//				0x00>::GetRHI());
//			RHICmdList.SetBlendState(TStaticBlendState <
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero,
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero,
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero,
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero,
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero,
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero,
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero,
//				CW_RGBA,
//				BO_Add, BF_One, BF_SourceAlpha,
//				BO_Add, BF_Zero, BF_Zero >
//				::GetRHI(), FLinearColor::Black);
//				*/
//			ScreenVertexShader->SetParameters(RHICmdList, view);
//			PixelShader->SetShaderParams(RHICmdList, view, TressFXPPLLManager->PPLLNodes.SRV, TressFXPPLLManager->PPLLHeads->GetRenderTargetItem().ShaderResourceTexture);
//
//
//			DrawRectangle(
//				RHICmdList,
//				0, 0,
//				Size.X, Size.Y,
//				0, 0,
//				Size.X, Size.Y,
//				Size,
//				Size,
//				*ScreenVertexShader,
//				EDRF_Default,
//				1
//			);
//		}
//#endif
//
//	}
//
//
//
//
//	bool FindFreeElementInPool(FRHICommandList& RHICmdList, const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget> &Out, const TCHAR* InDebugName)
//	{
//		// There is bug. When a render target is created from a existing pointer, AllocationLevelInKB is not decreased. This cause assertion failure in FRenderTargetPool::GetStats(). So we have to release it first.
//		if (Out != nullptr)
//		{
//			if (!Out->GetDesc().Compare(Desc, true))
//			{
//				GRenderTargetPool.FreeUnusedResource(Out);
//				Out = nullptr;
//			}
//		}
//
//		const bool bReuse = GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Out, InDebugName);
//
//		// Release useless resolved render resource. Because of the reason mentioned above, we do in only in the macro.
//#if UE_BUILD_SHIPPING || UE_BUILD_TEST
//		if (Out->GetDesc().NumSamples > 1)
//			Out->GetRenderTargetItem().ShaderResourceTexture = nullptr;
//#endif
//
//		return bReuse;
//	}
//	void AllocPPLLBuffer(FRHICommandList& RHICmdList, const FIntPoint& Size)
//	{
//		// Get MSAA level
//		int SampleCount = 1;
//
//		// GBuffers
//		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Size, PF_R32_UINT, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
//		Desc.NumSamples = SampleCount;
//		FindFreeElementInPool(RHICmdList, Desc, TressFXPPLLManager->PPLLHeads, TEXT("PPLLHeads"));
//		TressFXPPLLManager->PPLLNodes.Release();
//		TressFXPPLLManager->PPLLNodes.Initialize(sizeof(FPPLL_Struct), Size.X*Size.Y, BUF_UnorderedAccess | BUF_ShaderResource, true);
//		TressFXPPLLManager->NodePoolSize = Size.X*Size.Y * 4;
//
//	}
//	void AllocRenderTargets(FRHICommandList& RHICmdList, const FIntPoint& Size)
//	{
//		// Get MSAA level
//		int SampleCount = 1;
//		{
//			// GBuffers
//			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Size, PF_A32B32G32R32F, FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));
//			Desc.NumSamples = SampleCount;
//			FindFreeElementInPool(RHICmdList, Desc, TressFXRenderTargets->Target, TEXT("TressFX"));
//		}
//		{
//			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Size, PF_R8G8B8A8, FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));
//			Desc.NumSamples = SampleCount;
//			FindFreeElementInPool(RHICmdList, Desc, TressFXRenderTargets->KPassTarget, TEXT("TressFXKpass"));
//		}
//		FPooledRenderTargetDesc DepthDesc(FPooledRenderTargetDesc::Create2DDesc(Size, PF_DepthStencil, FClearValueBinding::DepthOne, TexCreate_None, TexCreate_DepthStencilTargetable, true));
//		FindFreeElementInPool(RHICmdList, DepthDesc, TressFXRenderTargets->Depth, TEXT("TressFXDepth"));
//		AllocPPLLBuffer(RHICmdList, Size);
//
//	}
//
//
//
//
//	FTextureRHIParamRef GetTressFXRenderTargetView()
//	{
//		if (TressFXRenderTargets->Target.IsValid())
//		{
//			return TressFXRenderTargets->Target->GetRenderTargetItem().TargetableTexture;
//		}
//
//		return nullptr;
//
//
//	}
//}
//#endif
//#pragma  optimize ("",on)