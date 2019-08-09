#pragma once
#include "CoreMinimal.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "GlobalShader.h"
#include "RHIDefinitions.h"
#include "HitProxies.h"
#include "RHI.h"
#include "TressFXShaders.h"
#include "MeshPassProcessor.h"

DECLARE_LOG_CATEGORY_EXTERN(TressFXRenderingLog, Log, All);

#ifndef SHORTCUT_INITIAL_DEPTH
	#define SHORTCUT_INITIAL_DEPTH 0x0
#endif

#define MAX_TFX_KBUFFER_SIZE 16
#define MIN_TFX_KBUFFER_SIZE 2
#define TFX_PPLL_NULL 0xffffffff

class FTressFXShaderElementData : public FMeshMaterialShaderElementData
{
public:
	FTressFXShaderElementData(ETressFXPass::Type InTFXPass, const FSceneView* InViewIfDynamicMeshCommand) :
		TFXPass(InTFXPass)
	{
		if (InViewIfDynamicMeshCommand)
		{
			auto ViewSize = InViewIfDynamicMeshCommand->UnscaledViewRect.Size();
			FragmentBufferSize = FVector4(ViewSize.X, ViewSize.Y, ViewSize.X*ViewSize.Y, 0);
			ViewRect = InViewIfDynamicMeshCommand->UnscaledViewRect;
		}
	}
	ETressFXPass::Type TFXPass;
	FVector4 FragmentBufferSize;
	FIntRect ViewRect;

};

struct ETressFXRenderType
{
	enum Type
	{
		Opaque,
		ShortCut,
		KBuffer,
		///////
		Num,
		Max = (Num - 1)
	};

};


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

