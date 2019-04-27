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

#define MAX_TFX_KBUFFER_SIZE 16
#define MIN_TFX_KBUFFER_SIZE 2
#define TFX_PPLL_NULL 0xffffffff

struct FPPLL_Struct
{
	uint32 Depth;
	uint32 Color;
	uint32 Next;
	uint32 DummyPad;
};

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
	template<bool bCalcVelocity, ETressFXRenderType::Type TFXRenderType>
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
//TressFXDepthsAlphaPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

class TressFXDepthsAlphaPassMeshProcessor : public FMeshPassProcessor
{
public:

	TressFXDepthsAlphaPassMeshProcessor(
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
		ERasterizerCullMode MeshCullMode
	);

	FMeshPassProcessorRenderState PassDrawRenderState;

};

//////////////////////////////////////////////////////////////////////////
//FTressFXFillColorPassMeshProcessor
/////////////////////////////////////////////////////////////////////////

class FTressFXFillColorPassMeshProcessor : public FMeshPassProcessor
{
public:

	FTressFXFillColorPassMeshProcessor(
		const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext
	);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:

	template<ETressFXRenderType::Type RenderType>
	void Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode
	);

	template<int32 KBufferSize>
	void ProcessKBuffer(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode
	);

	template<int32 NodeCount, bool bUseROV>
	void ProcessAOIT(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode
	);

	void ProcessShortcut(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode
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
