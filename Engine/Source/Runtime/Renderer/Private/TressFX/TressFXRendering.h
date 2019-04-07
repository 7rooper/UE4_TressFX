#pragma once
#include "CoreMinimal.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "GlobalShader.h"
#include "RHIDefinitions.h"
#include "HitProxies.h"
#include "RHI.h"
#include "MeshPassProcessor.h"
#include "TressFXShaders.h"

DECLARE_LOG_CATEGORY_EXTERN(TressFXRenderingLog, Log, All);

#ifndef SHORTCUT_INITIAL_DEPTH
	#define SHORTCUT_INITIAL_DEPTH 0x0
#endif

struct FTressFXMeshBatch
{
	const FMeshBatch* Mesh;
	const FPrimitiveSceneProxy* Proxy;
};

class FPrimitiveSceneProxy;

//////////////////////////////////////////////////////////////////////////
//FTRessFXDepthsVelocityPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

class FTressFXDepthsVelocityPassMeshProcessor : public FMeshPassProcessor
{
public:

	FTressFXDepthsVelocityPassMeshProcessor(
		const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext
	);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:
	template<bool bCalcVelocity>
	void Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		bool bNoDepth
	);

	FMeshPassProcessorRenderState PassDrawRenderState;

};


//////////////////////////////////////////////////////////////////////////
//FTressFXROVPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

class FTressFXROVPassMeshProcessor : public FMeshPassProcessor
{
public:

	FTressFXROVPassMeshProcessor(
		const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext
	);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:
	template<bool bCalcVelocity>
	void Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		bool bNoDepth
	);

	FMeshPassProcessorRenderState PassDrawRenderState;

};


/////////////////////////////////////////////////////////////////////////////////
//  FTressFXFillColorDrawingPolicy
////////////////////////////////////////////////////////////////////////////////

//class FTressFXFillColorDrawingPolicy : public FMeshDrawingPolicy
//{
//public:
//
//	struct ContextDataType : public FMeshDrawingPolicy::ContextDataType
//	{
//		explicit ContextDataType(const bool InbIsInstancedStereo) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(false) {};
//		ContextDataType(const bool InbIsInstancedStereo, const bool InbIsInstancedStereoEmulated) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(InbIsInstancedStereoEmulated) {};
//		ContextDataType() : bIsInstancedStereoEmulated(false) {};
//
//		bool bIsInstancedStereoEmulated;
//	};
//
//	FTressFXFillColorDrawingPolicy(
//		const FVertexFactory* InVertexFactory,
//		const FMaterialRenderProxy* InMaterialRenderProxy,
//		const FMaterial& InMaterialResource,
//		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
//		ERHIFeatureLevel::Type InFeatureLevel
//	);
//
//	// FMeshDrawingPolicy interface.
//
//
//	FDrawingPolicyMatchResult Matches(const FTressFXFillColorDrawingPolicy& Other, bool bForReals = false) const
//	{
//		DRAWING_POLICY_MATCH_BEGIN
//			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other, bForReals)) &&
//			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
//			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
//		DRAWING_POLICY_MATCH_END
//	}
//
//
//	void SetSharedState(FRHICommandList& RHICmdList, const FViewInfo* View, const FTressFXFillColorDrawingPolicy::ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState) const;
//
//	/**
//	* Create bound shader state using the vertex decl from the mesh draw policy
//	* as well as the shaders needed to draw the mesh
//	* @param DynamicStride - optional stride for dynamic vertex data
//	* @return new bound shader state object
//	*/
//	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const;
//
//	void SetMeshRenderState(
//		FRHICommandList& RHICmdList,
//		const FSceneView& View,
//		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
//		const FMeshBatch& Mesh,
//		int32 BatchElementIndex,
//		const FDrawingPolicyRenderState& DrawRenderState
//	) const;
//
//	friend int32 CompareDrawingPolicy(const FTressFXFillColorDrawingPolicy& A, const FTressFXFillColorDrawingPolicy& B);
//
//	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View);
//
//	FRasterizerStateRHIParamRef ComputeRasterizerState(EDrawingPolicyOverrideFlags InOverrideFlags) const;
//
//private:
//
//
//	class TTressFX_ShortCutVS<false>* VertexShader;
//	class FTressFX_FillColorPS* PixelShader;
//
//	FShaderPipeline* ShaderPipeline;
//};

/////////////////////////////////////////////////////////////////////////////////
//  FTressFXFillColorDrawingPolicyFactory
////////////////////////////////////////////////////////////////////////////////


//class FTressFXFillColorDrawingPolicyFactory
//{
//
//public:
//
//
//	struct ContextType {};
//
//	static bool DrawDynamicMesh(
//		FRHICommandList& RHICmdList,
//		const FViewInfo& View,
//		ContextType DrawingContext,
//		const FMeshBatch& Mesh,
//		bool bPreFog,
//		const FDrawingPolicyRenderState& DrawRenderState,
//		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
//		FHitProxyId HitProxyId
//	);
//
//};


//class FTressFXPPLLDrawingPolicy : public FMeshDrawingPolicy
//{
//public:
//
//	struct ContextDataType : public FMeshDrawingPolicy::ContextDataType
//	{
//		explicit ContextDataType(const bool InbIsInstancedStereo) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(false) {};
//		ContextDataType(const bool InbIsInstancedStereo, const bool InbIsInstancedStereoEmulated) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(InbIsInstancedStereoEmulated) {};
//		ContextDataType() : bIsInstancedStereoEmulated(false) {};
//
//		bool bIsInstancedStereoEmulated;
//	};
//
//	FTressFXPPLLDrawingPolicy(
//		const FVertexFactory* InVertexFactory,
//		const FMaterialRenderProxy* InMaterialRenderProxy,
//		const FMaterial& InMaterialResource,
//		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
//		ERHIFeatureLevel::Type InFeatureLevel
//	);
//
//	// FMeshDrawingPolicy interface.
//
//
//	FDrawingPolicyMatchResult Matches(const FTressFXPPLLDrawingPolicy& Other, bool bForReals = false) const
//	{
//		DRAWING_POLICY_MATCH_BEGIN
//			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other, bForReals)) &&
//			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
//			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
//		DRAWING_POLICY_MATCH_END
//	}
//
//
//	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FTressFXPPLLDrawingPolicy::ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState) const;
//
//	/**
//	* Create bound shader state using the vertex decl from the mesh draw policy
//	* as well as the shaders needed to draw the mesh
//	* @param DynamicStride - optional stride for dynamic vertex data
//	* @return new bound shader state object
//	*/
//	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const;
//
//	void SetMeshRenderState(
//		FRHICommandList& RHICmdList,
//		const FSceneView& View,
//		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
//		const FMeshBatch& Mesh,
//		int32 BatchElementIndex,
//		const FDrawingPolicyRenderState& DrawRenderState
//	) const;
//
//	friend int32 CompareDrawingPolicy(const FTressFXPPLLDrawingPolicy& A, const FTressFXPPLLDrawingPolicy& B);
//
//	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View);
//
//private:
//
//
//	class FTressFX_PPLLBuildVS* VertexShader;
//	class FTressFX_PPLLBuildPS* PixelShader;
//
//	FShaderPipeline* ShaderPipeline;
//};


//class FTressFXPPLLDrawingPolicyFactory
//{
//
//public:
//
//
//	struct ContextType {};
//
//	static bool DrawDynamicMesh(
//		FRHICommandList& RHICmdList,
//		const FViewInfo& View,
//		ContextType DrawingContext,
//		const FMeshBatch& Mesh,
//		bool bPreFog,
//		const FDrawingPolicyRenderState& DrawRenderState,
//		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
//		FHitProxyId HitProxyId
//	);
//
//};

//template<bool bNeedsVelocity>
//class FTressFXDepthsAlphaDrawingPolicy //: public FMeshDrawingPolicy
//{
//public:
//
//	struct ContextDataType : public FMeshDrawingPolicy::ContextDataType
//	{
//		explicit ContextDataType(const bool InbIsInstancedStereo) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(false) {};
//		ContextDataType(const bool InbIsInstancedStereo, const bool InbIsInstancedStereoEmulated) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(InbIsInstancedStereoEmulated) {};
//		ContextDataType() : bIsInstancedStereoEmulated(false) {};
//
//		bool bIsInstancedStereoEmulated;
//	};
//
//	FTressFXDepthsAlphaDrawingPolicy(
//		const FVertexFactory* InVertexFactory,
//		const FMaterialRenderProxy* InMaterialRenderProxy,
//		const FMaterial& InMaterialResource,
//	//	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
//		ERHIFeatureLevel::Type InFeatureLevel
//	);
//
//	// FMeshDrawingPolicy interface.
//
//
//	FDrawingPolicyMatchResult Matches(const FTressFXDepthsAlphaDrawingPolicy& Other, bool bForReals = false) const
//	{
//		DRAWING_POLICY_MATCH_BEGIN
//			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other, bForReals)) &&
//			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
//			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
//		DRAWING_POLICY_MATCH_END
//	}
//
//
//	void SetSharedState(
//		FRHICommandList& RHICmdList,
//		const FSceneView* View,
//		const FTressFXDepthsAlphaDrawingPolicy::ContextDataType PolicyContext,
//		FDrawingPolicyRenderState& DrawRenderState
//	) const
//	{
//		VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, View->ViewUniformBuffer, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated, DrawRenderState);
//		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, View->ViewUniformBuffer, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated, DrawRenderState);
//	};
//
//	/**
//	* Create bound shader state using the vertex decl from the mesh draw policy
//	* as well as the shaders needed to draw the mesh
//	* @param DynamicStride - optional stride for dynamic vertex data
//	* @return new bound shader state object
//	*/
//	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
//	{
//		return FBoundShaderStateInput(
//			FMeshDrawingPolicy::GetVertexDeclaration(),
//			VertexShader->GetVertexShader(),
//			nullptr,
//			nullptr,
//			PixelShader->GetPixelShader(),
//			NULL);
//	};
//
//	void SetMeshRenderState(
//		FRHICommandList& RHICmdList,
//		const FSceneView& View,
//		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
//		const FMeshBatch& Mesh,
//		int32 BatchElementIndex,
//		const FDrawingPolicyRenderState& DrawRenderState
//	) const
//	{
//		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
//		VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh, BatchElement, DrawRenderState);
//		PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
//	};
//
//	friend int32 CompareDrawingPolicy(const FTressFXDepthsAlphaDrawingPolicy& A, const FTressFXDepthsAlphaDrawingPolicy& B);
//
//	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View)
//	{
//		DrawRenderState.SetBlendState(TStaticBlendState<
//			//accum inv allpha
//			CW_RED
//			, BO_Add
//			, BF_Zero
//			, BF_SourceColor
//			, BO_Add
//			, BF_Zero
//			, BF_SourceAlpha
//			//velocity
//			, CW_RGBA
//			, BO_Add
//			, BF_One
//			, BF_Zero
//			, BO_Add
//			, BF_One
//			, BF_Zero
//		>::GetRHI(), FLinearColor::Transparent, 0x000000ff);
//		DrawRenderState.SetDepthStencilState(
//			TStaticDepthStencilState<
//			true, CF_GreaterEqual,
//			true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
//			true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
//			0xff, 0xff, DWM_Zero>::GetRHI());
//	};
//
//	FRasterizerStateRHIParamRef ComputeRasterizerState(EDrawingPolicyOverrideFlags InOverrideFlags) const
//	{
//		return TStaticRasterizerState<FM_Solid, CM_CW, false, true, true>::GetRHI();
//	};
//
//private:
//
//	class TTressFX_ShortCutVS<bNeedsVelocity>* VertexShader;
//	class FTressFX_DepthsAlphaPS<bNeedsVelocity>* PixelShader;
//
//	FShaderPipeline* ShaderPipeline;
//};

//
//class FTressFXDepthsAlphaDrawingPolicyFactory
//{
//
//public:
//
//
//	struct ContextType {};
//
//	static bool DrawDynamicMesh(
//		FRHICommandList& RHICmdList,
//		const FSceneView& View,
//		ContextType DrawingContext,
//		const FMeshBatch& Mesh,
//		bool bPreFog,
//		const FMeshPassProcessorRenderState& DrawRenderState,
//		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
//		FHitProxyId HitProxyId
//	);
//
//};
