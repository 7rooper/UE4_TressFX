// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthRendering.cpp: Depth rendering implementation.
=============================================================================*/

#include "DepthRendering.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "SceneRendering.h"
#include "StaticMeshDrawList.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "GPUSkinCache.h"
#include "MeshPassProcessor.inl"

static TAutoConsoleVariable<int32> CVarRHICmdPrePassDeferredContexts(
	TEXT("r.RHICmdPrePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize prepass command list execution."));
static TAutoConsoleVariable<int32> CVarParallelPrePass(
	TEXT("r.ParallelPrePass"),
	1,
	TEXT("Toggles parallel zprepass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksPrePass(
	TEXT("r.RHICmdFlushRenderThreadTasksPrePass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the pre pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksPrePass is > 0 we will flush."));

const TCHAR* GetDepthDrawingModeString(EDepthDrawingMode Mode)
{
	switch (Mode)
	{
	case DDM_None:
		return TEXT("DDM_None");
	case DDM_NonMaskedOnly:
		return TEXT("DDM_NonMaskedOnly");
	case DDM_AllOccluders:
		return TEXT("DDM_AllOccluders");
	case DDM_AllOpaque:
		return TEXT("DDM_AllOpaque");
	default:
		check(0);
	}

	return TEXT("");
}

DECLARE_GPU_STAT(Prepass);

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVS<true>,TEXT("/Engine/Private/PositionOnlyDepthVertexShader.usf"),TEXT("Main"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVS<false>,TEXT("/Engine/Private/DepthOnlyVertexShader.usf"),TEXT("Main"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyHS,TEXT("/Engine/Private/DepthOnlyVertexShader.usf"),TEXT("MainHull"),SF_Hull);	
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyDS,TEXT("/Engine/Private/DepthOnlyVertexShader.usf"),TEXT("MainDomain"),SF_Domain);

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyPS,TEXT("/Engine/Private/DepthOnlyPixelShader.usf"),TEXT("Main"),SF_Pixel);

IMPLEMENT_SHADERPIPELINE_TYPE_VS(DepthNoPixelPipeline, TDepthOnlyVS<false>, true);
IMPLEMENT_SHADERPIPELINE_TYPE_VS(DepthPosOnlyNoPixelPipeline, TDepthOnlyVS<true>, true);
IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(DepthPipeline, TDepthOnlyVS<false>, FDepthOnlyPS, true);


bool UseShaderPipelines()
{
	static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderPipelines"));
	return CVar && CVar->GetValueOnAnyThread() != 0;
}

template <bool bPositionOnly>
void GetDepthPassShaders(
	const FMaterial& Material,
	FVertexFactoryType* VertexFactoryType,
	ERHIFeatureLevel::Type FeatureLevel,
	FDepthOnlyHS*& HullShader,
	FDepthOnlyDS*& DomainShader,
	TDepthOnlyVS<bPositionOnly>*& VertexShader,
	FDepthOnlyPS*& PixelShader,
	FShaderPipeline*& ShaderPipeline,
	bool bUsesMobileColorValue)
{
	if (bPositionOnly && !bUsesMobileColorValue)
	{
		ShaderPipeline = UseShaderPipelines() ? Material.GetShaderPipeline(&DepthPosOnlyNoPixelPipeline, VertexFactoryType) : nullptr;
		VertexShader = ShaderPipeline
			? ShaderPipeline->GetShader<TDepthOnlyVS<bPositionOnly> >()
			: Material.GetShader<TDepthOnlyVS<bPositionOnly> >(VertexFactoryType);
	}
	else
	{
		const bool bNeedsPixelShader = bUsesMobileColorValue || !Material.WritesEveryPixel() || Material.MaterialUsesPixelDepthOffset() || Material.IsTranslucencyWritingCustomDepth();

		const EMaterialTessellationMode TessellationMode = Material.GetTessellationMode();
		if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[FeatureLevel])
			&& VertexFactoryType->SupportsTessellationShaders() 
			&& TessellationMode != MTM_NoTessellation)
		{
			ShaderPipeline = nullptr;
			VertexShader = Material.GetShader<TDepthOnlyVS<bPositionOnly> >(VertexFactoryType);
			HullShader = Material.GetShader<FDepthOnlyHS>(VertexFactoryType);
			DomainShader = Material.GetShader<FDepthOnlyDS>(VertexFactoryType);
			if (bNeedsPixelShader)
			{
				PixelShader = Material.GetShader<FDepthOnlyPS>(VertexFactoryType);
			}
		}
		else
		{
			HullShader = nullptr;
			DomainShader = nullptr;
			bool bUseShaderPipelines = UseShaderPipelines();
			if (bNeedsPixelShader)
			{
				ShaderPipeline = bUseShaderPipelines ? Material.GetShaderPipeline(&DepthPipeline, VertexFactoryType, false) : nullptr;
			}
			else
			{
				ShaderPipeline = bUseShaderPipelines ? Material.GetShaderPipeline(&DepthNoPixelPipeline, VertexFactoryType, false) : nullptr;
			}

			if (ShaderPipeline)
			{
				VertexShader = ShaderPipeline->GetShader<TDepthOnlyVS<bPositionOnly> >();
				if (bNeedsPixelShader)
				{
					PixelShader = ShaderPipeline->GetShader<FDepthOnlyPS>();
				}
			}
			else
			{
				VertexShader = Material.GetShader<TDepthOnlyVS<bPositionOnly> >(VertexFactoryType);
				if (bNeedsPixelShader)
				{
					PixelShader = Material.GetShader<FDepthOnlyPS>(VertexFactoryType);
				}
			}
		}
	}
}

#define IMPLEMENT_GetDepthPassShaders( bPositionOnly ) \
	template void GetDepthPassShaders< bPositionOnly >( \
		const FMaterial& Material, \
		FVertexFactoryType* VertexFactoryType, \
		ERHIFeatureLevel::Type FeatureLevel, \
		FDepthOnlyHS*& HullShader, \
		FDepthOnlyDS*& DomainShader, \
		TDepthOnlyVS<bPositionOnly>*& VertexShader, \
		FDepthOnlyPS*& PixelShader, \
		FShaderPipeline*& ShaderPipeline, \
		bool bUsesMobileColorValue \
	);

IMPLEMENT_GetDepthPassShaders( true );
IMPLEMENT_GetDepthPassShaders( false );

FDepthDrawingPolicy::FDepthDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
	ERHIFeatureLevel::Type InFeatureLevel,
	float InMobileColorValue) 
	: FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings)
{
	const bool bUsesMobileColorValue = (InMobileColorValue != 0.0f);
	MobileColorValue = InMobileColorValue;
	
	bNeedsPixelShader = bUsesMobileColorValue || (!InMaterialResource.WritesEveryPixel() || InMaterialResource.MaterialUsesPixelDepthOffset() || InMaterialResource.IsTranslucencyWritingCustomDepth());

	if (!bNeedsPixelShader)
	{
		PixelShader = nullptr;
	}

	const EMaterialTessellationMode TessellationMode = InMaterialResource.GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& TessellationMode != MTM_NoTessellation)
	{
		ShaderPipeline = nullptr;
		VertexShader = InMaterialResource.GetShader<TDepthOnlyVS<false> >(VertexFactory->GetType());
		HullShader = InMaterialResource.GetShader<FDepthOnlyHS>(VertexFactory->GetType());
		DomainShader = InMaterialResource.GetShader<FDepthOnlyDS>(VertexFactory->GetType());
		if (bNeedsPixelShader)
		{
			PixelShader = InMaterialResource.GetShader<FDepthOnlyPS>(InVertexFactory->GetType());
		}
	}
	else
	{
		HullShader = nullptr;
		DomainShader = nullptr;
		bool bUseShaderPipelines = UseShaderPipelines();
		if (bNeedsPixelShader)
		{
			ShaderPipeline = bUseShaderPipelines ? InMaterialResource.GetShaderPipeline(&DepthPipeline, InVertexFactory->GetType(), false) : nullptr;
		}
		else
		{
			ShaderPipeline = bUseShaderPipelines ? InMaterialResource.GetShaderPipeline(&DepthNoPixelPipeline, InVertexFactory->GetType(), false) : nullptr;
		}

		if (ShaderPipeline)
		{
			VertexShader = ShaderPipeline->GetShader<TDepthOnlyVS<false> >();
			if (bNeedsPixelShader)
			{
				PixelShader = ShaderPipeline->GetShader<FDepthOnlyPS>();
			}
		}
		else
		{
			VertexShader = InMaterialResource.GetShader<TDepthOnlyVS<false> >(VertexFactory->GetType());
			if (bNeedsPixelShader)
			{
				PixelShader = InMaterialResource.GetShader<FDepthOnlyPS>(InVertexFactory->GetType());
			}
		}
	}
	BaseVertexShader = VertexShader;
}

void SetDepthPassDitheredLODTransitionState(const FSceneView* SceneView, const FMeshBatch& RESTRICT Mesh, int32 StaticMeshId, FDrawingPolicyRenderState& DrawRenderState)
{
	if (SceneView && StaticMeshId >= 0 && Mesh.bDitheredLODTransition)
	{
		checkSlow(SceneView->bIsViewInfo);
		const FViewInfo* ViewInfo = (FViewInfo*)SceneView;

		if (ViewInfo->bAllowStencilDither)
		{
			if (ViewInfo->StaticMeshFadeOutDitheredLODMap[StaticMeshId])
			{
				DrawRenderState.SetDepthStencilState(
					TStaticDepthStencilState<true, CF_DepthNearOrEqual,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK
					>::GetRHI());
				DrawRenderState.SetStencilRef(STENCIL_SANDBOX_MASK);
			}
			else if (ViewInfo->StaticMeshFadeInDitheredLODMap[StaticMeshId])
			{
				DrawRenderState.SetDepthStencilState(
					TStaticDepthStencilState<true, CF_DepthNearOrEqual,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK
					>::GetRHI());
			}
		}
	}
}

void ApplyDitheredLODTransitionStateInternal(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither)
{
	DrawRenderState.SetDitheredLODTransitionAlpha(0.0f);
	FDepthStencilStateRHIParamRef DepthStencilState = nullptr;
	uint32 StencilRef = 0;

	if (InAllowStencilDither)
	{
		DepthStencilState = TStaticDepthStencilState<>::GetRHI();
	}

	if (Mesh.bDitheredLODTransition)
	{
		if (ViewInfo.StaticMeshFadeOutDitheredLODMap[Mesh.Id])
		{
			if (InAllowStencilDither)
			{
				DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK
					>::GetRHI();
				StencilRef = STENCIL_SANDBOX_MASK;
			}
			else
			{
				DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition());
			}
		}
		else if (ViewInfo.StaticMeshFadeInDitheredLODMap[Mesh.Id])
		{
			if (InAllowStencilDither)
			{
				DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK
					>::GetRHI();
			}
			else
			{
				DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition() - 1.0f);
			}
		}
	}

	if (DepthStencilState)
	{
		DrawRenderState.SetDepthStencilState(DepthStencilState);
		DrawRenderState.SetStencilRef(StencilRef);
	}
}

void FDepthDrawingPolicy::ApplyDitheredLODTransitionState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither)
{
	ApplyDitheredLODTransitionStateInternal(DrawRenderState, ViewInfo, Mesh, InAllowStencilDither);
}

void FPositionOnlyDepthDrawingPolicy::ApplyDitheredLODTransitionState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither)
{
	ApplyDitheredLODTransitionStateInternal(DrawRenderState, ViewInfo, Mesh, InAllowStencilDither);
}


void FDepthDrawingPolicy::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
{
	VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
}

void FDepthDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const FDepthDrawingPolicy::ContextDataType PolicyContext) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, DrawRenderState, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated);
	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View,DrawRenderState.GetViewUniformBuffer(),DrawRenderState.GetPassUniformBuffer());
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View,DrawRenderState.GetViewUniformBuffer(),DrawRenderState.GetPassUniformBuffer());
	}

	if (bNeedsPixelShader)
	{
		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View, DrawRenderState, MobileColorValue);
	}

	// Set the shared mesh resources.
	FMeshDrawingPolicy::SetSharedState(RHICmdList, DrawRenderState, View, PolicyContext);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @return new bound shader state object
*/
FBoundShaderStateInput FDepthDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		bNeedsPixelShader ? PixelShader->GetPixelShader() : NULL,
		NULL);
}

void FDepthDrawingPolicy::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	const FDrawingPolicyRenderState& DrawRenderState,
	const ElementDataType& ElementData,
	const ContextDataType PolicyContext
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	SetPrimitiveIdStream(RHICmdList, View, PrimitiveSceneProxy, BatchElement.PrimitiveIdMode, BatchElement.DynamicPrimitiveShaderDataIndex);
	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}

	if (bNeedsPixelShader)
	{
		PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
}

int32 CompareDrawingPolicy(const FDepthDrawingPolicy& A,const FDepthDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
	COMPAREDRAWINGPOLICYMEMBERS(bNeedsPixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	COMPAREDRAWINGPOLICYMEMBERS(MobileColorValue);
	return 0;
}

FPositionOnlyDepthDrawingPolicy::FPositionOnlyDepthDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings
	) 
	: FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings)
{
	ShaderPipeline = UseShaderPipelines() ? InMaterialResource.GetShaderPipeline(&DepthPosOnlyNoPixelPipeline, VertexFactory->GetType()) : nullptr;
	VertexShader = ShaderPipeline
		? ShaderPipeline->GetShader<TDepthOnlyVS<true> >()
		: InMaterialResource.GetShader<TDepthOnlyVS<true> >(InVertexFactory->GetType());
	bUsePositionOnlyVS = true;
	BaseVertexShader = VertexShader;
}

void FPositionOnlyDepthDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, FPositionOnlyDepthDrawingPolicy::ContextDataType PolicyContext) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, DrawRenderState, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated);

	// Set the shared mesh resources.
	VertexFactory->SetPositionStream(RHICmdList);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @return new bound shader state object
*/
FBoundShaderStateInput FPositionOnlyDepthDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
{
	FVertexDeclarationRHIParamRef VertexDeclaration;
	VertexDeclaration = VertexFactory->GetPositionDeclaration();

	checkSlow(MaterialRenderProxy->GetMaterial(InFeatureLevel)->GetBlendMode() == BLEND_Opaque);
	return FBoundShaderStateInput(VertexDeclaration, VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), FPixelShaderRHIRef(), FGeometryShaderRHIRef());
}

void FPositionOnlyDepthDrawingPolicy::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	const FDrawingPolicyRenderState& DrawRenderState,
	const ElementDataType& ElementData,
	const ContextDataType PolicyContext
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
	VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
	SetPrimitiveIdStream(RHICmdList, View, PrimitiveSceneProxy, BatchElement.PrimitiveIdMode, BatchElement.DynamicPrimitiveShaderDataIndex);
}

void FPositionOnlyDepthDrawingPolicy::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
{
	VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
}

int32 CompareDrawingPolicy(const FPositionOnlyDepthDrawingPolicy& A, const FPositionOnlyDepthDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}

void FDepthDrawingPolicyFactory::AddStaticMesh(FScene* Scene, FStaticMesh* StaticMesh)
{
	const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	const EBlendMode BlendMode = Material->GetBlendMode();
	const auto FeatureLevel = Scene->GetFeatureLevel();

	FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(*StaticMesh);
	OverrideSettings.MeshOverrideFlags |= Material->IsTwoSided() ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;

	if (!Material->WritesEveryPixel() || Material->MaterialUsesPixelDepthOffset())
	{	
		FDepthDrawingPolicy DrawingPolicy(StaticMesh->VertexFactory,
			MaterialRenderProxy,
			*Material,
			OverrideSettings,
			FeatureLevel,
			0.0f // MobileColorValue
			);

		// only draw if required
		Scene->MaskedDepthDrawList.AddMesh(
			StaticMesh,
			FDepthDrawingPolicy::ElementDataType(),
			DrawingPolicy,
			FeatureLevel
			);
	}
	else
	{
		if (StaticMesh->VertexFactory->SupportsPositionOnlyStream() 
			&& !Material->MaterialModifiesMeshPosition_RenderThread())
		{
			OverrideSettings.MeshOverrideFlags |= Material->IsWireframe() ? EDrawingPolicyOverrideFlags::Wireframe : EDrawingPolicyOverrideFlags::None;

			const FMaterialRenderProxy* DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			FPositionOnlyDepthDrawingPolicy DrawingPolicy(StaticMesh->VertexFactory,
				DefaultProxy,
				*DefaultProxy->GetMaterial(Scene->GetFeatureLevel()),
				OverrideSettings
				);

			// Add the static mesh to the position-only depth draw list.
			Scene->PositionOnlyDepthDrawList.AddMesh(
				StaticMesh,
				FPositionOnlyDepthDrawingPolicy::ElementDataType(),
				DrawingPolicy,
				FeatureLevel
				);
		}
		else
		{
			if (!Material->MaterialModifiesMeshPosition_RenderThread())
			{
				// Override with the default material for everything but opaque two sided materials
				MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			}

			FDepthDrawingPolicy DrawingPolicy(StaticMesh->VertexFactory,
				MaterialRenderProxy,
				*MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel()),
				OverrideSettings,
				FeatureLevel,
				0.0f // MobileColorValue
				);

			// Add the static mesh to the opaque depth-only draw list.
			Scene->DepthDrawList.AddMesh(
				StaticMesh,
				FDepthDrawingPolicy::ElementDataType(),
				DrawingPolicy,
				FeatureLevel
				);
		}
	}
}

bool FDepthDrawingPolicyFactory::DrawMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	const uint64& BatchElementMask,
	const FDrawingPolicyRenderState& DrawRenderState,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId, 
	const bool bIsInstancedStereo, 
	const bool bIsInstancedStereoEmulated
	)
{
	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
	bool bDirty = false;

	//Do a per-FMeshBatch check on top of the proxy check in RenderPrePass to handle the case where a proxy that is relevant 
	//to the depth only pass has to submit multiple FMeshElements but only some of them should be used as occluders.
	if ((Mesh.bUseAsOccluder || !DrawingContext.bRespectUseAsOccluderFlag || DrawingContext.DepthDrawingMode == DDM_AllOpaque)
		&& ShouldIncludeDomainInMeshPass(Material->GetMaterialDomain()))
	{
		const EBlendMode BlendMode = Material->GetBlendMode();
		const bool bUsesMobileColorValue = (DrawingContext.MobileColorValue != 0.0f);

		// Check to see if the primitive is currently fading in or out using the screen door effect.  If it is,
		// then we can't assume the object is opaque as it may be forcibly masked.
		const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View.State );

		FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(Mesh);
		OverrideSettings.MeshOverrideFlags |= Material->IsTwoSided() ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;

		if ( BlendMode == BLEND_Opaque 
			&& Mesh.VertexFactory->SupportsPositionOnlyStream() 
			&& !Material->MaterialModifiesMeshPosition_RenderThread()
			&& Material->WritesEveryPixel()
			&& !bUsesMobileColorValue
			)
		{
			//render opaque primitives that support a separate position-only vertex buffer
			const FMaterialRenderProxy* DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

			OverrideSettings.MeshOverrideFlags |= Material->IsWireframe() ? EDrawingPolicyOverrideFlags::Wireframe : EDrawingPolicyOverrideFlags::None;

			FPositionOnlyDepthDrawingPolicy DrawingPolicy(
				Mesh.VertexFactory, 
				DefaultProxy, 
				*DefaultProxy->GetMaterial(View.GetFeatureLevel()), 
				OverrideSettings
				);

			FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
			DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
			CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()), DrawingPolicy.GetMaterialRenderProxy());
			DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, FPositionOnlyDepthDrawingPolicy::ContextDataType(bIsInstancedStereo, bIsInstancedStereoEmulated));

			int32 BatchElementIndex = 0;
			uint64 Mask = BatchElementMask;
			do
			{
				if(Mask & 1)
				{
					// We draw instanced static meshes twice when rendering with instanced stereo. Once for each eye.
					const bool bIsInstancedMesh = Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
					const uint32 InstancedStereoDrawCount = (bIsInstancedStereo && bIsInstancedMesh) ? 2 : 1;
					for (uint32 DrawCountIter = 0; DrawCountIter < InstancedStereoDrawCount; ++DrawCountIter)
					{
						DrawingPolicy.SetInstancedEyeIndex(RHICmdList, DrawCountIter);

						TDrawEvent<FRHICommandList> MeshEvent;
						BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent, EnumHasAnyFlags(EShowMaterialDrawEventTypes(GShowMaterialDrawEventTypes), EShowMaterialDrawEventTypes::DepthPositionOnly));

						DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal, FPositionOnlyDepthDrawingPolicy::ElementDataType(), FPositionOnlyDepthDrawingPolicy::ContextDataType());
						DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex, bIsInstancedStereo);
					}
				}
				Mask >>= 1;
				BatchElementIndex++;
			} while(Mask);

			bDirty = true;
		}
		else if (!IsTranslucentBlendMode(BlendMode) || Material->IsTranslucencyWritingCustomDepth())
		{
			const bool bMaterialMasked = !Material->WritesEveryPixel() || Material->IsTranslucencyWritingCustomDepth();

			bool bDraw = true;

			switch(DrawingContext.DepthDrawingMode)
			{
			case DDM_AllOpaque:
				break;
			case DDM_AllOccluders: 
				break;
			case DDM_NonMaskedOnly: 
				bDraw = !bMaterialMasked;
				break;
			default:
				check(!"Unrecognized DepthDrawingMode");
			}

			if(bDraw)
			{
				if (!bMaterialMasked && !Material->MaterialModifiesMeshPosition_RenderThread())
				{
					// Override with the default material for opaque materials that are not two sided
					MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
				}

				FDepthDrawingPolicy DrawingPolicy(
					Mesh.VertexFactory, 
					MaterialRenderProxy, 
					*MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), 
					OverrideSettings,
					View.GetFeatureLevel(),
					DrawingContext.MobileColorValue
					);

				FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
				DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
				CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()), DrawingPolicy.GetMaterialRenderProxy());
				DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, FDepthDrawingPolicy::ContextDataType(bIsInstancedStereo, bIsInstancedStereoEmulated));

				int32 BatchElementIndex = 0;
				uint64 Mask = BatchElementMask;
				do
				{
					if(Mask & 1)
					{
						// We draw instanced static meshes twice when rendering with instanced stereo. Once for each eye.
						const bool bIsInstancedMesh = Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
						const uint32 InstancedStereoDrawCount = (bIsInstancedStereo && bIsInstancedMesh) ? 2 : 1;
						for (uint32 DrawCountIter = 0; DrawCountIter < InstancedStereoDrawCount; ++DrawCountIter)
						{
							DrawingPolicy.SetInstancedEyeIndex(RHICmdList, DrawCountIter);

							TDrawEvent<FRHICommandList> MeshEvent;
							BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent, EnumHasAnyFlags(EShowMaterialDrawEventTypes(GShowMaterialDrawEventTypes), EShowMaterialDrawEventTypes::Depth));

							DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal, FMeshDrawingPolicy::ElementDataType(), FDepthDrawingPolicy::ContextDataType());
							DrawingPolicy.DrawMesh(RHICmdList, View, Mesh, BatchElementIndex, bIsInstancedStereo);
						}
					}
					Mask >>= 1;
					BatchElementIndex++;
				} while(Mask);

				bDirty = true;
			}
		}
	}

	return bDirty;
}

bool FDepthDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId, 
	const bool bIsInstancedStereo, 
	const bool bIsInstancedStereoEmulated
	)
{
	return DrawMesh(
		RHICmdList, 
		View,
		DrawingContext,
		Mesh,
		Mesh.Elements.Num()==1 ? 1 : (1<<Mesh.Elements.Num())-1,	// 1 bit set for each mesh element
		DrawRenderState,
		bPreFog,
		PrimitiveSceneProxy,
		HitProxyId, 
		bIsInstancedStereo, 
		bIsInstancedStereoEmulated
		);
}

bool FDepthDrawingPolicyFactory::DrawStaticMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FStaticMesh& StaticMesh,
	const uint64& BatchElementMask,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId, 
	const bool bIsInstancedStereo,
	const bool bIsInstancedStereoEmulated
	)
{
	bool bDirty = false;

	const FMaterial* Material = StaticMesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
	const EMaterialShadingModel ShadingModel = Material->GetShadingModel();
	bDirty |= DrawMesh(
		RHICmdList, 
		View,
		DrawingContext,
		StaticMesh,
		BatchElementMask,
		DrawRenderState,
		bPreFog,
		PrimitiveSceneProxy,
		HitProxyId, 
		bIsInstancedStereo, 
		bIsInstancedStereoEmulated
		);

	return bDirty;
}

bool FDeferredShadingSceneRenderer::RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState)
{
	FDepthDrawingPolicyFactory::ContextType Context(EarlyZPassMode, true);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() && MeshBatchAndRelevance.GetRenderInMainPass())
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = MeshBatchAndRelevance.PrimitiveSceneProxy;
			bool bShouldUseAsOccluder = true;

			if (EarlyZPassMode < DDM_AllOpaque)
			{
				extern float GMinScreenRadiusForDepthPrepass;
				//@todo - move these proxy properties into FMeshBatchAndRelevance so we don't have to dereference the proxy in order to reject a mesh
				const float LODFactorDistanceSquared = (PrimitiveSceneProxy->GetBounds().Origin - View.ViewMatrices.GetViewOrigin()).SizeSquared() * FMath::Square(View.LODDistanceFactor);

				// Only render primitives marked as occluders
				bShouldUseAsOccluder = PrimitiveSceneProxy->ShouldUseAsOccluder()
					// Only render static objects unless movable are requested
					&& (!PrimitiveSceneProxy->IsMovable() || bEarlyZPassMovable)
					&& (FMath::Square(PrimitiveSceneProxy->GetBounds().SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared);
			}

			if (bShouldUseAsOccluder)
			{
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, DrawRenderState, PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
			}
		}
	}

	return true;
}

static void SetupPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View, const FSceneRenderer* SceneRenderer, const bool bIsEditorPrimitivePass = false)
{
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	if (!View.IsInstancedStereoPass() || bIsEditorPrimitivePass)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	}
	else
	{
		if (View.bIsMultiViewEnabled)
		{
			const uint32 LeftMinX = SceneRenderer->Views[0].ViewRect.Min.X;
			const uint32 LeftMaxX = SceneRenderer->Views[0].ViewRect.Max.X;
			const uint32 RightMinX = SceneRenderer->Views[1].ViewRect.Min.X;
			const uint32 RightMaxX = SceneRenderer->Views[1].ViewRect.Max.X;
			
			const uint32 LeftMaxY = SceneRenderer->Views[0].ViewRect.Max.Y;
			const uint32 RightMaxY = SceneRenderer->Views[1].ViewRect.Max.Y;
			
			RHICmdList.SetStereoViewport(LeftMinX, RightMinX, 0, 0, 0.0f, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, 1.0f);
		}
		else
		{
			RHICmdList.SetViewport(0, 0, 0, SceneRenderer->InstancedStereoWidth, View.ViewRect.Max.Y, 1);
		}
	}
}

static void RenderHiddenAreaMaskView(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View)
{
	const auto FeatureLevel = GMaxRHIFeatureLevel;
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);

	extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	VertexShader->SetDepthParameter(RHICmdList, 1.0f);

	if (GEngine->XRSystem->GetHMDDevice())
	{
		GEngine->XRSystem->GetHMDDevice()->DrawHiddenAreaMesh_RenderThread(RHICmdList, View.StereoPass);
	}
}

bool FDeferredShadingSceneRenderer::RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState)
{
	bool bDirty = false;

	SetupPrePassView(RHICmdList, View, this);

	if (UseMeshDrawCommandPipeline())
	{
		SubmitMeshDrawCommandsForView(View, EMeshPass::DepthPass, nullptr, RHICmdList);

		return true;
	}

	// Draw the static occluder primitives using a depth drawing policy.
	{
		// Draw opaque occluders which support a separate position-only
		// vertex buffer to minimize vertex fetch bandwidth, which is
		// often the bottleneck during the depth only pass.
		SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
		bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisible(RHICmdList, View, DrawRenderState, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
	}
	{
		// Draw opaque occluders, using double speed z where supported.
		SCOPED_DRAW_EVENT(RHICmdList, Opaque);
		bDirty |= Scene->DepthDrawList.DrawVisible(RHICmdList, View, DrawRenderState, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
	}

	if (EarlyZPassMode >= DDM_AllOccluders)
	{
		// Draw opaque occluders with masked materials
		SCOPED_DRAW_EVENT(RHICmdList, Masked);
		bDirty |= Scene->MaskedDepthDrawList.DrawVisible(RHICmdList, View, DrawRenderState, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, Dynamic);
		bDirty |= RenderPrePassViewDynamic(RHICmdList, View, DrawRenderState);
	}

	return bDirty;
}

class FRenderPrepassDynamicDataThreadTask : public FRenderTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;

public:

	FRenderPrepassDynamicDataThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const FDrawingPolicyRenderState& InDrawRenderState
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, DrawRenderState(InDrawRenderState)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderPrepassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(RHICmdList.IsInsideRenderPass());
		ThisRenderer.RenderPrePassViewDynamic(RHICmdList, View, DrawRenderState);
		RHICmdList.EndRenderPass();
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

DECLARE_CYCLE_STAT(TEXT("Prepass"), STAT_CLP_Prepass, STATGROUP_ParallelCommandListMarkers);

static int32 GStartPrepassParallelTranslatesImmediately = 0;
static FAutoConsoleVariableRef CVarStartPrepassParallelTranslatesImmediately(
	TEXT("r.StartPrepassParallelTranslatesImmediately"),
	GStartPrepassParallelTranslatesImmediately,
	TEXT("Experimental option, allows the parallel translate tasks for the prepass to start immediately. Makes the most sense with r.DoInitViewsLightingAfterPrepass = 1."),
	ECVF_RenderThreadSafe
);

class FPrePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	FPrePassParallelCommandListSet(const FViewInfo& InView, const FSceneRenderer* InSceneRenderer, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext, const FDrawingPolicyRenderState& InDrawRenderState)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Prepass), InView, InSceneRenderer, InParentCmdList, bInParallelExecute, bInCreateSceneContext, InDrawRenderState)
	{
		// Do not copy-paste. this is a very unusual FParallelCommandListSet because it is a prepass and we want to do some work after starting some tasks
	}

	virtual ~FPrePassParallelCommandListSet()
	{
		// Do not copy-paste. this is a very unusual FParallelCommandListSet because it is a prepass and we want to do some work after starting some tasks
		Dispatch(true);
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		FSceneRenderTargets::Get(CmdList).BeginRenderingPrePass(CmdList, false);
		SetupPrePassView(CmdList, View, SceneRenderer);
	}
};

bool FDeferredShadingSceneRenderer::RenderPrePassViewParallel(const FViewInfo& View, FRHICommandListImmediate& ParentCmdList, const FDrawingPolicyRenderState& DrawRenderState, TFunctionRef<void()> AfterTasksAreStarted, bool bDoPrePre)
{
	bool bDepthWasCleared = false;

	check(ParentCmdList.IsOutsideRenderPass());

	{
		FPrePassParallelCommandListSet ParallelCommandListSet(View, this, ParentCmdList,
			CVarRHICmdPrePassDeferredContexts.GetValueOnRenderThread() > 0, 
			CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
			DrawRenderState);

		if (UseMeshDrawCommandPipeline())
		{
			SubmitMeshDrawCommandsForView(View, EMeshPass::DepthPass, &ParallelCommandListSet, ParentCmdList);

			if (bDoPrePre)
			{
				AfterTasksAreStarted();
				bDepthWasCleared = PreRenderPrePass(ParentCmdList);
			}
		}
		else
		{
			// Draw the static occluder primitives using a depth drawing policy.
			// Draw opaque occluders which support a separate position-only
			// vertex buffer to minimize vertex fetch bandwidth, which is
			// often the bottleneck during the depth only pass.
			Scene->PositionOnlyDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);

			// Draw opaque occluders, using double speed z where supported.
			Scene->DepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);

			// Draw opaque occluders with masked materials
			if (EarlyZPassMode >= DDM_AllOccluders)
			{
				Scene->MaskedDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);
			}

			if (!GStartPrepassParallelTranslatesImmediately)
			{
				// we do this step here (awkwardly) so that the above tasks can be in flight while we get the particles (which must be dynamic) setup.
				if (bDoPrePre)
				{
					AfterTasksAreStarted();
					bDepthWasCleared = PreRenderPrePass(ParentCmdList);
				}
				// Dynamic
				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderPrepassDynamicDataThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::GetRenderThread())
					.ConstructAndDispatchWhenReady(*this, *CmdList, View, ParallelCommandListSet.DrawRenderState);

				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
			}
			else
			{
				if (bDoPrePre)
				{
					bDepthWasCleared = PreRenderPrePass(ParentCmdList);
				}
			}
		}
	}

	if (GStartPrepassParallelTranslatesImmediately && !UseMeshDrawCommandPipeline())
	{
		// we do this step here (awkwardly) so that the above tasks can be in flight while we get the particles (which must be dynamic) setup.
		if (bDoPrePre)
		{
			AfterTasksAreStarted();
		}
		FPrePassParallelCommandListSet ParallelCommandListSet(View, this, ParentCmdList,
			CVarRHICmdPrePassDeferredContexts.GetValueOnRenderThread() > 0,
			CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
			DrawRenderState);

		// Dynamic
		FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

		FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderPrepassDynamicDataThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::GetRenderThread())
			.ConstructAndDispatchWhenReady(*this, *CmdList, View, ParallelCommandListSet.DrawRenderState);

		ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
	}

	return bDepthWasCleared;
}

/** A pixel shader used to fill the stencil buffer with the current dithered transition mask. */
class FDitheredTransitionStencilPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDitheredTransitionStencilPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	FDitheredTransitionStencilPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
	{
		DitheredTransitionFactorParameter.Bind(Initializer.ParameterMap, TEXT("DitheredTransitionFactor"));
	}

	FDitheredTransitionStencilPS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

		const float DitherFactor = View.GetTemporalLODTransition();
		SetShaderValue(RHICmdList, GetPixelShader(), DitheredTransitionFactorParameter, DitherFactor);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DitheredTransitionFactorParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter DitheredTransitionFactorParameter;
};

IMPLEMENT_SHADER_TYPE(, FDitheredTransitionStencilPS, TEXT("/Engine/Private/DitheredTransitionStencil.usf"), TEXT("Main"), SF_Pixel);

/** Possibly do the FX prerender and setup the prepass*/
bool FDeferredShadingSceneRenderer::PreRenderPrePass(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_GPU_MASK(RHICmdList, FRHIGPUMask::All()); // Required otherwise emulatestereo gets broken.

	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PrePass));
	// RenderPrePassHMD clears the depth buffer. If this changes we must change RenderPrePass to maintain the correct behavior!
	bool bDepthWasCleared = RenderPrePassHMD(RHICmdList);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SceneContext.BeginRenderingPrePass(RHICmdList, !bDepthWasCleared);
	bDepthWasCleared = true;

	// Dithered transition stencil mask fill
	if (bDitheredLODTransitionsUseStencil)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always,
			true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK>::GetRHI();

		SCOPED_DRAW_EVENT(RHICmdList, DitheredStencilPrePass);
		FIntPoint BufferSizeXY = SceneContext.GetBufferSizeXY();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			// Set shaders, states
			TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
			TShaderMapRef<FDitheredTransitionStencilPS> PixelShader(View.ShaderMap);

			extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			RHICmdList.SetStencilRef(STENCIL_SANDBOX_MASK);

			PixelShader->SetParameters(RHICmdList, View);

			DrawRectangle(
				RHICmdList,
				0, 0,
				BufferSizeXY.X, BufferSizeXY.Y,
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				BufferSizeXY,
				BufferSizeXY,
				*ScreenVertexShader,
				EDRF_UseTriangleOptimization);
		}
	}
	// Need to close the renderpass here since we may call BeginRenderingPrePass later
	RHICmdList.EndRenderPass();

	return bDepthWasCleared;
}

void FDeferredShadingSceneRenderer::RenderPrePassEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, FDepthDrawingPolicyFactory::ContextType Context) 
{
	SetupPrePassView(RHICmdList, View, this, true);

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_World);
	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);

	bool bDirty = false;
	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(ShaderPlatform);
		const FScene* LocalScene = Scene;

		if (UseMeshDrawCommandPipeline())
		{
			DrawDynamicMeshPass(View, RHICmdList,
				[&View, &DrawRenderState, LocalScene, &Context](FDynamicPassMeshDrawListContext& DynamicMeshPassContext)
				{
					FDepthPassMeshProcessor PassMeshProcessor(
						LocalScene,
						&View,
						DrawRenderState,
						Context.bRespectUseAsOccluderFlag,
						Context.DepthDrawingMode,
						false,
						DynamicMeshPassContext);

					const uint64 DefaultBatchElementMask = ~0ull;
					
					for (int32 MeshIndex = 0; MeshIndex < View.ViewMeshElements.Num(); MeshIndex++)
					{
						const FMeshBatch& MeshBatch = View.ViewMeshElements[MeshIndex];
						PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
					}
				});
		}
		else
		{
			// Draw the base pass for the view's batched mesh elements.
			bDirty |= DrawViewElements<FDepthDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, Context, SDPG_World, true) || bDirty;
		}

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.BatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, false) || bDirty;

		if (UseMeshDrawCommandPipeline())
		{
			DrawDynamicMeshPass(View, RHICmdList,
				[&View, &DrawRenderState, LocalScene, &Context](FDynamicPassMeshDrawListContext& DynamicMeshPassContext)
				{
					FDepthPassMeshProcessor PassMeshProcessor(
						LocalScene,
						&View,
						DrawRenderState,
						Context.bRespectUseAsOccluderFlag,
						Context.DepthDrawingMode,
						false,
						DynamicMeshPassContext);

					const uint64 DefaultBatchElementMask = ~0ull;
					
					for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
					{
						const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
						PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
					}
				});
		}
		else
		{
			// Draw foreground objects last
			bDirty |= DrawViewElements<FDepthDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, Context, SDPG_Foreground, true) || bDirty;
		}

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.TopBatchedViewElements.Draw(RHICmdList, DrawRenderState, FeatureLevel, bNeedToSwitchVerticalAxis, View, false) || bDirty;
	}
}

void SetupDepthPassState(FDrawingPolicyRenderState& DrawRenderState)
{
	// Disable color writes, enable depth tests and writes.
	DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
}

void CreateDepthPassUniformBuffer(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View,
	TUniformBufferRef<FSceneTexturesUniformParameters>& DepthPassUniformBuffer)
{
	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);

	FSceneTexturesUniformParameters SceneTextureParameters;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SetupSceneTextureUniformParameters(SceneContext, View.FeatureLevel, ESceneTextureSetupMode::None, SceneTextureParameters);

	FScene* Scene = View.Family->Scene->GetRenderScene();

	if (Scene)
	{
		Scene->UniformBuffers.DepthPassUniformBuffer.UpdateUniformBufferImmediate(SceneTextureParameters);
		DepthPassUniformBuffer = Scene->UniformBuffers.DepthPassUniformBuffer;
	}
	else
	{
		DepthPassUniformBuffer = TUniformBufferRef<FSceneTexturesUniformParameters>::CreateUniformBufferImmediate(SceneTextureParameters, UniformBuffer_SingleFrame);
	}
}

bool FDeferredShadingSceneRenderer::RenderPrePass(FRHICommandListImmediate& RHICmdList, TFunctionRef<void()> AfterTasksAreStarted)
{
	check(RHICmdList.IsOutsideRenderPass());

	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderPrePass, FColor::Emerald);
	bool bDepthWasCleared = false;

	extern const TCHAR* GetDepthPassReason(bool bDitheredLODTransitionsUseStencil, EShaderPlatform ShaderPlatform);
	SCOPED_DRAW_EVENTF(RHICmdList, PrePass, TEXT("PrePass %s %s"), GetDepthDrawingModeString(EarlyZPassMode), GetDepthPassReason(bDitheredLODTransitionsUseStencil, ShaderPlatform));

	SCOPE_CYCLE_COUNTER(STAT_DepthDrawTime);
	SCOPED_GPU_STAT(RHICmdList, Prepass);

	bool bDirty = false;
	bool bDidPrePre = false;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	bool bParallel = GRHICommandList.UseParallelAlgorithms() && CVarParallelPrePass.GetValueOnRenderThread();

	if (!bParallel)
	{
		// nothing to be gained by delaying this.
		AfterTasksAreStarted();
		// Note: the depth buffer will be cleared under PreRenderPrePass.
		bDepthWasCleared = PreRenderPrePass(RHICmdList);
		bDidPrePre = true;

		// PreRenderPrePass will end up clearing the depth buffer so do not clear it again.
		SceneContext.BeginRenderingPrePass(RHICmdList, false);
	}
	else
	{
		SceneContext.GetSceneDepthSurface(); // this probably isn't needed, but if there was some lazy allocation of the depth surface going on, we want it allocated now before we go wide. We may not have called BeginRenderingPrePass yet if bDoFXPrerender is true
	}

	// Draw a depth pass to avoid overdraw in the other passes.
	if(EarlyZPassMode != DDM_None)
	{
		const bool bWaitForTasks = bParallel && (CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0);
		FScopedCommandListWaitForTasks Flusher(bWaitForTasks, RHICmdList);

		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
			const FViewInfo& View = Views[ViewIndex];
			SCOPED_GPU_MASK(RHICmdList, !View.IsInstancedStereoPass() ? View.GPUMask : (Views[0].GPUMask | Views[1].GPUMask));

			TUniformBufferRef<FSceneTexturesUniformParameters> PassUniformBuffer;
			CreateDepthPassUniformBuffer(RHICmdList, View, PassUniformBuffer);

			FDrawingPolicyRenderState DrawRenderState(View, PassUniformBuffer);

			if (View.ShouldRenderView())
			{
				Scene->UniformBuffers.UpdateViewUniformBuffer(View);

				SetupDepthPassState(DrawRenderState);

				if (bParallel)
				{
					check(RHICmdList.IsOutsideRenderPass());
					bDepthWasCleared = RenderPrePassViewParallel(View, RHICmdList, DrawRenderState, AfterTasksAreStarted, !bDidPrePre) || bDepthWasCleared;
					bDirty = true; // assume dirty since we are not going to wait
					bDidPrePre = true;
				}
				else
				{
					bDirty |= RenderPrePassView(RHICmdList, View, DrawRenderState);
				}
			}

			// Parallel rendering has self contained renderpasses so we need a new one for editor primitives.
			if (bParallel)
			{
				SceneContext.BeginRenderingPrePass(RHICmdList, false);
			}
			RenderPrePassEditorPrimitives(RHICmdList, View, DrawRenderState, FDepthDrawingPolicyFactory::ContextType(EarlyZPassMode, true));
			if (bParallel)
			{
				RHICmdList.EndRenderPass();
			}
		}
	}
	if (!bDidPrePre)
	{
		// Only parallel rendering with all views marked as not-to-be-rendered will get here.
		// For some reason we haven't done this yet. Best do it now for consistency with the old code.
		AfterTasksAreStarted();
		bDepthWasCleared = PreRenderPrePass(RHICmdList);
		bDidPrePre = true;
	}

	if (bParallel)
	{
		// In parallel mode there will be no renderpass here. Need to restart.
		SceneContext.BeginRenderingPrePass(RHICmdList, false);
	}

	// Dithered transition stencil mask clear, accounting for all active viewports
	if (bDitheredLODTransitionsUseStencil)
	{
		if (Views.Num() > 1)
		{
			FIntRect FullViewRect = Views[0].ViewRect;
			for (int32 ViewIndex = 1; ViewIndex < Views.Num(); ++ViewIndex)
			{
				FullViewRect.Union(Views[ViewIndex].ViewRect);
			}
			RHICmdList.SetViewport(FullViewRect.Min.X, FullViewRect.Min.Y, 0, FullViewRect.Max.X, FullViewRect.Max.Y, 1);
		}
		DrawClearQuad(RHICmdList, false, FLinearColor::Transparent, false, 0, true, 0);
	}

	// Now we are finally finished.
	SceneContext.FinishRenderingPrePass(RHICmdList);

	return bDepthWasCleared;
}

/**
 * Returns true if there's a hidden area mask available
 */
static FORCEINLINE bool HasHiddenAreaMask()
{
	static const auto* const HiddenAreaMaskCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.HiddenAreaMask"));
	return (HiddenAreaMaskCVar != nullptr &&
		HiddenAreaMaskCVar->GetValueOnRenderThread() == 1 &&
		GEngine &&
		GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() &&
		GEngine->XRSystem->GetHMDDevice()->HasHiddenAreaMesh());
}

bool FDeferredShadingSceneRenderer::RenderPrePassHMD(FRHICommandListImmediate& RHICmdList)
{
	// Early out before we change any state if there's not a mask to render
	if (!HasHiddenAreaMask())
	{
		return false;
	}

	// This is the only place the depth buffer is cleared. If this changes we MUST change RenderPrePass and others to maintain the behavior.
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingPrePass(RHICmdList, true);


	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.BlendState = TStaticBlendState<CW_NONE>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		if (View.StereoPass != eSSP_FULL)
		{
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			RenderHiddenAreaMaskView(RHICmdList, GraphicsPSOInit, View);
		}
	}

	SceneContext.FinishRenderingPrePass(RHICmdList);

	return true;
}

template<bool bPositionOnly>
void FDepthPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		TDepthOnlyVS<bPositionOnly>,
		FDepthOnlyHS,
		FDepthOnlyDS,
		FDepthOnlyPS> DepthPassShaders;

	FShaderPipeline* ShaderPipeline = nullptr;

	GetDepthPassShaders<bPositionOnly>(
		MaterialResource,
		VertexFactory->GetType(),
		FeatureLevel,
		DepthPassShaders.HullShader,
		DepthPassShaders.DomainShader,
		DepthPassShaders.VertexShader,
		DepthPassShaders.PixelShader,
		ShaderPipeline,
		false
		);

	FDrawingPolicyRenderState DrawRenderState(PassDrawRenderState);

	SetDepthPassDitheredLODTransitionState(ViewIfDynamicMeshCommand, MeshBatch, StaticMeshId, DrawRenderState);

	FDepthOnlyShaderElementData ShaderElementData(0.0f);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, true);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		DepthPassShaders,
		MeshFillMode,
		MeshCullMode,
		1,
		FMeshDrawCommandSortKey::Default,
		bPositionOnly ? EMeshPassFeatures::PositionOnly : EMeshPassFeatures::Default,
		ShaderElementData);
}

void FDepthPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	bool bDraw = MeshBatch.bUseForDepthPass;

	// Filter by occluder flags and settings if required.
	if (bDraw && bRespectUseAsOccluderFlag && !MeshBatch.bUseAsOccluder && EarlyZPassMode < DDM_AllOpaque)
	{
		if (PrimitiveSceneProxy)
		{
			// Only render primitives marked as occluders.
			bDraw = PrimitiveSceneProxy->ShouldUseAsOccluder()
				// Only render static objects unless movable are requested.
				&& (!PrimitiveSceneProxy->IsMovable() || bEarlyZPassMovable);

			// Filter dynamic mesh commands by screen size.
			if (ViewIfDynamicMeshCommand)
			{
				extern float GMinScreenRadiusForDepthPrepass;
				const float LODFactorDistanceSquared = (PrimitiveSceneProxy->GetBounds().Origin - ViewIfDynamicMeshCommand->ViewMatrices.GetViewOrigin()).SizeSquared() * FMath::Square(ViewIfDynamicMeshCommand->LODDistanceFactor);
				bDraw = bDraw && FMath::Square(PrimitiveSceneProxy->GetBounds().SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared;
			}
		}
		else
		{
			bDraw = false;
		}
	}

	if (bDraw)
	{
		// Determine the mesh's material and blend mode.
		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, FallbackMaterialRenderProxyPtr);

		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		const EBlendMode BlendMode = Material.GetBlendMode();
		const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MeshBatch, Material);
		const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MeshBatch, Material);
		const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);

		if (!bIsTranslucent
			&& (!PrimitiveSceneProxy || PrimitiveSceneProxy->ShouldRenderInMainPass())
			&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()))
		{
			if (BlendMode == BLEND_Opaque
				&& MeshBatch.VertexFactory->SupportsPositionOnlyStream()
				&& !Material.MaterialModifiesMeshPosition_RenderThread()
				&& Material.WritesEveryPixel())
			{
				const FMaterialRenderProxy& DefaultProxy = *UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
				const FMaterial& DefaultMaterial = *DefaultProxy.GetMaterial(FeatureLevel);
				Process<true>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, DefaultProxy, DefaultMaterial, MeshFillMode, MeshCullMode);
			}
			else
			{
				const bool bMaterialMasked = !Material.WritesEveryPixel() || Material.IsTranslucencyWritingCustomDepth();

				if (!bMaterialMasked || EarlyZPassMode != DDM_NonMaskedOnly)
				{
					const FMaterialRenderProxy* EffectiveMaterialRenderProxy = &MaterialRenderProxy;
					const FMaterial* EffectiveMaterial = &Material;

					if (!bMaterialMasked && !Material.MaterialModifiesMeshPosition_RenderThread())
					{
						// Override with the default material for opaque materials that are not two sided
						EffectiveMaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
						EffectiveMaterial = EffectiveMaterialRenderProxy->GetMaterial(FeatureLevel);
					}

					Process<false>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, *EffectiveMaterialRenderProxy, *EffectiveMaterial, MeshFillMode, MeshCullMode);
				}
			}
		}
	}
}

FDepthPassMeshProcessor::FDepthPassMeshProcessor(const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FDrawingPolicyRenderState& InPassDrawRenderState,
	const bool InbRespectUseAsOccluderFlag,
	const EDepthDrawingMode InEarlyZPassMode,
	const bool InbEarlyZPassMovable,
	FMeshPassDrawListContext& InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	, bRespectUseAsOccluderFlag(InbRespectUseAsOccluderFlag)
	, EarlyZPassMode(InEarlyZPassMode)
	, bEarlyZPassMovable(InbEarlyZPassMovable)
{
	PassDrawRenderState = InPassDrawRenderState;
	PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	PassDrawRenderState.SetPassUniformBuffer(Scene->UniformBuffers.DepthPassUniformBuffer);
}

FMeshPassProcessor* CreateDepthPassProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext)
{
	extern void GetEarlyZPassMode(EShaderPlatform ShaderPlatform, EDepthDrawingMode& EarlyZPassMode, bool& bEarlyZPassMovable);

	EDepthDrawingMode EarlyZPassMode;
	bool bEarlyZPassMovable;
	GetEarlyZPassMode(Scene->GetShaderPlatform(), EarlyZPassMode, bEarlyZPassMovable);

	FDrawingPolicyRenderState DepthPassState;
	SetupDepthPassState(DepthPassState);
	return new(FMemStack::Get()) FDepthPassMeshProcessor(Scene, InViewIfDynamicMeshCommand, DepthPassState, true, EarlyZPassMode, bEarlyZPassMovable, InDrawListContext);
}

FRegisterPassProcessorCreateFunction RegisterDepthPass(&CreateDepthPassProcessor, EShadingPath::Deferred, EMeshPass::DepthPass, EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);