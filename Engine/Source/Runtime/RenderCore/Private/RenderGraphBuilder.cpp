// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RenderGraphBuilder.h"
#include "RenderCore.h"
#include "RenderTargetPool.h"
#include "RenderGraphResourcePool.h"
#include "VisualizeTexture.h"


#if RENDER_GRAPH_DEBUGGING

static int32 GRenderGraphImmediateMode = 0;
static FAutoConsoleVariableRef CVarImmediateMode(
	TEXT("r.RDG.ImmediateMode"),
	GRenderGraphImmediateMode,
	TEXT("Executes passes as they get created. Useful to have a callstack of the wiring code when crashing in the pass' lambda."),
	ECVF_RenderThreadSafe);

static int32 GRenderGraphEmitWarnings = 1;
static FAutoConsoleVariableRef CVarEmitWarnings(
	TEXT("r.RDG.EmitWarnings"),
	GRenderGraphEmitWarnings,
	TEXT("Allow to output warnings for inefficiencies found during wiring and execution of the passes."),
	ECVF_RenderThreadSafe);

#else

static const int32 GRenderGraphImmediateMode = 0;
static const int32 GRenderGraphEmitWarnings = 0;

#endif


void InitRenderGraph()
{
#if RENDER_GRAPH_DEBUGGING && WITH_ENGINE
	if (FParse::Param(FCommandLine::Get(), TEXT("rdgimmediate")))
	{
		GRenderGraphImmediateMode = 1;
	}
#endif
}


static bool IsBoundAsReadable(const FRDGTexture* Texture, FShaderParameterStructRef ParameterStruct)
{
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
		uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_TEXTURE:
		{
			const FRDGTexture* InputTexture = *ParameterStruct.GetMemberPtrAtOffset<const FRDGTexture*>(Offset);
			if (Texture == InputTexture)
			{
				return true;
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_SRV:
		{
			const FRDGTextureSRV* InputSRV = *ParameterStruct.GetMemberPtrAtOffset<const FRDGTextureSRV*>(Offset);
			if (InputSRV && Texture == InputSRV->Desc.Texture)
			{
				return true;
			}
		}
		break;
		default:
			break;
		}
	}

	return false;
}


#if RENDER_GRAPH_DRAW_EVENTS == 2

FRDGEventName::FRDGEventName(const TCHAR* EventFormat, ...)
{
	if (GetEmitDrawEvents())
	{
		va_list ptr;
		va_start(ptr, EventFormat);
		TCHAR TempStr[256];
		// Build the string in the temp buffer
		FCString::GetVarArgs(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr) - 1, EventFormat, ptr);

		EventName = TempStr;
	}
}

#endif


void FRDGBuilder::Execute()
{
	#if RENDER_GRAPH_DEBUGGING
	{
		checkf(CurrentScope == nullptr, TEXT("Render graph needs to have all scopes ended to execute."));

		checkf(!bHasExecuted, TEXT("Render graph execution should only happen once."));
	}
	#endif

	if (!GRenderGraphImmediateMode)
	{
		WalkGraphDependencies();

		QUICK_SCOPE_CYCLE_COUNTER(STAT_FRDGBuilder_Execute);
		for (const FRenderGraphPass* Pass : Passes)
		{
			ExecutePass(Pass);
		}
	}

	// Pops remaining scopes
	if ((RENDER_GRAPH_DRAW_EVENTS != 0) && GetEmitDrawEvents())
	{
		for (int32 i = 0; i < kMaxScopeCount; i++)
		{
			if (!ScopesStack[i])
				break;
			RHICmdList.PopEvent();
		}
	}

	ProcessDeferredInternalResourceQueries();

	DestructPasses();

	#if RENDER_GRAPH_DEBUGGING
	{
		bHasExecuted = true;
	}
	#endif
}

void FRDGBuilder::DebugPass(const FRenderGraphPass* Pass)
{
#if RENDER_GRAPH_DEBUGGING
	// Verify all the settings of the pass make sense.
	ValidatePass(Pass);

	// Executes the pass immediatly as they get added mode, to have callstack of wiring code when crashing within the pass.
	if (GRenderGraphImmediateMode)
	{
		ExecutePass(Pass);
	}
#endif

#if WITH_ENGINE && SUPPORTS_VISUALIZE_TEXTURE
	// If visualizing a texture, look for any output of the pass. This must be done after the
	// GRenderGraphImmediateMode's ExecutePass() because this will actually create a capturing
	// pass if needed that would have to be executed right away as well.
	if (GVisualizeTexture.bEnabled)
	{
		CaptureAnyInterestingPassOutput(Pass);
	}
#endif
}

void FRDGBuilder::ValidatePass(const FRenderGraphPass* Pass) const
{
#if RENDER_GRAPH_DEBUGGING
	FRenderTargetBindingSlots* RESTRICT RenderTargets = nullptr;
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	bool bIsCompute = Pass->IsCompute();
	bool bCanUseUAVs = bIsCompute;
	bool bRequiresRenderTargetSlots = !bIsCompute;

	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
		uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_UAV:
		case UBMT_GRAPH_TRACKED_BUFFER_UAV:
		{
			FRDGResource* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGResource*>(Offset);
			if (UAV && !bCanUseUAVs && GRenderGraphEmitWarnings)
			{
				UE_LOG(LogRendererCore, Warning, TEXT("UAV can only been bound to compute shaders, therefore UAV %s is certainly useless for passs %s."), UAV->Name, Pass->GetName());
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			if (!RenderTargets)
			{
				RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);
			}
			else if (GRenderGraphEmitWarnings)
			{
				UE_LOG(LogRendererCore, Warning, TEXT("Pass %s have duplicated render target binding slots."), Pass->GetName());
			}
		}
		break;
		default:
			break;
		}
	}

	if (RenderTargets)
	{
		checkf(bRequiresRenderTargetSlots, TEXT("Render pass %s does not need render target binging slots"), Pass->GetName());

		const bool bGeneratingMips = (Pass->GetFlags() & ERenderGraphPassFlags::GenerateMips) == ERenderGraphPassFlags::GenerateMips;
		bool bFoundRTBound = false;

		uint32 NumRenderTargets = 0;
		for (int i = 0; i < RenderTargets->Output.Num(); i++)
		{
			const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
			if (!RenderTarget.GetTexture())
			{
				NumRenderTargets = i;
				break;
			}
			bFoundRTBound = bFoundRTBound || IsBoundAsReadable(RenderTarget.GetTexture(), ParameterStruct);
		}
		for (int i = NumRenderTargets; i < RenderTargets->Output.Num(); i++)
		{
			const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
			checkf(RenderTarget.GetTexture() == nullptr, TEXT("Render targets must be packed. No empty spaces in the array."));
		}
		ensureMsgf(!bGeneratingMips || bFoundRTBound, TEXT("GenerateMips enabled but no RT found as source!"));
	}
	else
	{
		checkf(!bRequiresRenderTargetSlots, TEXT("Render pass %s requires render target binging slots"), Pass->GetName());
	}
#endif
}

void FRDGBuilder::CaptureAnyInterestingPassOutput(const FRenderGraphPass* Pass)
{
#if SUPPORTS_VISUALIZE_TEXTURE
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
		uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_UAV:
		{
			FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
			if (UAV && GVisualizeTexture.ShouldCapture(UAV->Desc.Texture->Name))
			{
				GVisualizeTexture.CreateContentCapturePass(*this, UAV->Desc.Texture);
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			FRenderTargetBindingSlots* RESTRICT RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);
			if (RenderTargets->DepthStencil.Texture && 
				(RenderTargets->DepthStencil.DepthStoreAction != ERenderTargetStoreAction::ENoAction || RenderTargets->DepthStencil.StencilStoreAction != ERenderTargetStoreAction::ENoAction) &&
				GVisualizeTexture.ShouldCapture(RenderTargets->DepthStencil.Texture->Name))
			{
				GVisualizeTexture.CreateContentCapturePass(*this, RenderTargets->DepthStencil.Texture);
			}
			for (int i = 0; i < RenderTargets->Output.Num(); i++)
			{
				const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
				if (RenderTarget.GetTexture() &&
					RenderTarget.GetStoreAction() != ERenderTargetStoreAction::ENoAction &&
					GVisualizeTexture.ShouldCapture(RenderTarget.GetTexture()->Name))
				{
					GVisualizeTexture.CreateContentCapturePass(*this, RenderTarget.GetTexture());
				}
				else
				{
					break;
				}
			}
		}
		break;
		default:
			break;
		}
	}
#endif // SUPPORTS_VISUALIZE_TEXTURE
}

void FRDGBuilder::WalkGraphDependencies()
{
	for (const FRenderGraphPass* Pass : Passes)
	{
		FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

		/** Increments all the FRDGResource::ReferenceCount. */
		for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
		{
			EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
			uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

			switch (Type)
			{
			case UBMT_GRAPH_TRACKED_TEXTURE:
			case UBMT_GRAPH_TRACKED_BUFFER:
			{
				FRDGResource* RESTRICT Resource = *ParameterStruct.GetMemberPtrAtOffset<FRDGResource*>(Offset);
				if (Resource)
				{
					Resource->ReferenceCount++;
				}
			}
			break;
			case UBMT_GRAPH_TRACKED_SRV:
			{
				FRDGTextureSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureSRV*>(Offset);
				if (SRV)
				{
					SRV->Desc.Texture->ReferenceCount++;
				}
			}
			break;
			case UBMT_GRAPH_TRACKED_UAV:
			{
				FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
				if (UAV)
				{
					UAV->Desc.Texture->ReferenceCount++;
				}
			}
			break;
			case UBMT_GRAPH_TRACKED_BUFFER_SRV:
			{
				FRDGBufferSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGBufferSRV*>(Offset);
				if (SRV)
				{
					SRV->Desc.Buffer->ReferenceCount++;
				}
			}
			break;
			case UBMT_GRAPH_TRACKED_BUFFER_UAV:
			{
				FRDGBufferUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGBufferUAV*>(Offset);
				if (UAV)
				{
					UAV->Desc.Buffer->ReferenceCount++;
				}
			}
			break;
			case UBMT_RENDER_TARGET_BINDING_SLOTS:
			{
				FRenderTargetBindingSlots* RESTRICT RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);

				for (int i = 0; i < RenderTargets->Output.Num(); i++)
				{
					const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
					if (RenderTarget.GetTexture())
					{
						RenderTarget.GetTexture()->ReferenceCount++;
					}
					else
					{
						break;
					}
				}

				const FDepthStencilBinding& DepthStencil = RenderTargets->DepthStencil;
				if (DepthStencil.Texture)
				{
					DepthStencil.Texture->ReferenceCount++;
				}
			}
			break;
			default:
				break;
			}
		}
	} // for (const FRenderGraphPass* Pass : Passes)

	// Add additional dependencies from deferred queries.
	for (const auto& Query : DeferredInternalTextureQueries)
	{
		Query.Texture->ReferenceCount++;
	}

	// Release external texture that have ReferenceCount == 0 and yet are already allocated.
	for (auto Pair : AllocatedTextures)
	{
		if (Pair.Key->ReferenceCount == 0)
		{
			Pair.Value = nullptr;
			Pair.Key->PooledRenderTarget = nullptr;
		}
	}
}

void FRDGBuilder::AllocateRHITextureIfNeeded(const FRDGTexture* Texture, bool bComputePass)
{
	check(Texture);

	if (Texture->PooledRenderTarget)
	{
		return;
	}

	check(Texture->ReferenceCount > 0 || GRenderGraphImmediateMode);

	// TODO(RDG): should avoid bDoWritableBarrier = true
	TRefCountPtr<IPooledRenderTarget>& PooledRenderTarget = AllocatedTextures.FindOrAdd(Texture);
	GRenderTargetPool.FindFreeElement(RHICmdList, Texture->Desc, PooledRenderTarget, Texture->Name, /* bDoWritableBarrier = */ true);

	Texture->PooledRenderTarget = PooledRenderTarget;
	Texture->CachedRHI.Texture = PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
}

void FRDGBuilder::AllocateRHITextureSRVIfNeeded(const FRDGTextureSRV* SRV, bool bComputePass)
{
	check(SRV);

	if (SRV->CachedRHI.SRV)
	{
		return;
	}

	AllocateRHITextureIfNeeded(SRV->Desc.Texture, bComputePass);

	SRV->CachedRHI.SRV = SRV->Desc.Texture->PooledRenderTarget->GetRenderTargetItem().MipSRVs[SRV->Desc.MipLevel];
}

void FRDGBuilder::AllocateRHITextureUAVIfNeeded(const FRDGTextureUAV* UAV, bool bComputePass)
{
	check(UAV);

	if (UAV->CachedRHI.UAV)
	{
		return;
	}

	AllocateRHITextureIfNeeded(UAV->Desc.Texture, bComputePass);

	UAV->CachedRHI.UAV = UAV->Desc.Texture->PooledRenderTarget->GetRenderTargetItem().MipUAVs[UAV->Desc.MipLevel];
}

void FRDGBuilder::AllocateRHIBufferIfNeeded(const FRDGBuffer* Buffer, bool bComputePass)
{
	check(Buffer);

	if (Buffer->PooledBuffer)
	{
		return;
	}

	check(Buffer->ReferenceCount > 0 || GRenderGraphImmediateMode);

	TRefCountPtr<FPooledRDGBuffer>& AllocatedBuffer = AllocatedBuffers.FindOrAdd(Buffer);
	GRenderGraphResourcePool.FindFreeBuffer(RHICmdList, Buffer->Desc, AllocatedBuffer, Buffer->Name);

	Buffer->PooledBuffer = AllocatedBuffer;
}

void FRDGBuilder::AllocateRHIBufferSRVIfNeeded(const FRDGBufferSRV* SRV, bool bComputePass)
{
	check(SRV);

	if (SRV->CachedRHI.SRV)
	{
		return;
	}

	AllocateRHIBufferIfNeeded(SRV->Desc.Buffer, bComputePass);

	if (SRV->Desc.Buffer->PooledBuffer->SRVs.Contains(SRV->Desc))
	{
		SRV->CachedRHI.SRV = SRV->Desc.Buffer->PooledBuffer->SRVs[SRV->Desc];
		return;
	}

	FShaderResourceViewRHIRef RHIShaderResourceView;

	if (SRV->Desc.Buffer->Desc.UnderlyingType == FRDGBufferDesc::EUnderlyingType::VertexBuffer)
	{
		RHIShaderResourceView = RHICreateShaderResourceView(SRV->Desc.Buffer->PooledBuffer->VertexBuffer, SRV->Desc.BytesPerElement, SRV->Desc.Format);
	}
	else if (SRV->Desc.Buffer->Desc.UnderlyingType == FRDGBufferDesc::EUnderlyingType::StructuredBuffer)
	{
		RHIShaderResourceView = RHICreateShaderResourceView(SRV->Desc.Buffer->PooledBuffer->StructuredBuffer);
	}
	else
	{
		check(0);
	}

	SRV->CachedRHI.SRV = RHIShaderResourceView;
	SRV->Desc.Buffer->PooledBuffer->SRVs.Add(SRV->Desc, RHIShaderResourceView);
}

void FRDGBuilder::AllocateRHIBufferUAVIfNeeded(const FRDGBufferUAV* UAV, bool bComputePass)
{
	check(UAV);

	if (UAV->CachedRHI.UAV)
	{
		return;
	}

	AllocateRHIBufferIfNeeded(UAV->Desc.Buffer, bComputePass);

	if (UAV->Desc.Buffer->PooledBuffer->UAVs.Contains(UAV->Desc))
	{
		UAV->CachedRHI.UAV = UAV->Desc.Buffer->PooledBuffer->UAVs[UAV->Desc];
		return;
	}

	// Hack to make sure only one UAVs is arround.
	UAV->Desc.Buffer->PooledBuffer->UAVs.Empty();

	FUnorderedAccessViewRHIRef RHIUnorderedAccessView;

	if (UAV->Desc.Buffer->Desc.UnderlyingType == FRDGBufferDesc::EUnderlyingType::VertexBuffer)
	{
		RHIUnorderedAccessView = RHICreateUnorderedAccessView(UAV->Desc.Buffer->PooledBuffer->VertexBuffer, UAV->Desc.Format);
	}
	else if (UAV->Desc.Buffer->Desc.UnderlyingType == FRDGBufferDesc::EUnderlyingType::StructuredBuffer)
	{
		RHIUnorderedAccessView = RHICreateUnorderedAccessView(UAV->Desc.Buffer->PooledBuffer->StructuredBuffer, UAV->Desc.bSupportsAtomicCounter, UAV->Desc.bSupportsAppendBuffer);
	}
	else
	{
		check(0);
	}

	UAV->CachedRHI.UAV = RHIUnorderedAccessView;
	UAV->Desc.Buffer->PooledBuffer->UAVs.Add(UAV->Desc, RHIUnorderedAccessView);
}

static EResourceTransitionPipeline CalcTransitionPipeline(bool bCurrentCompute, bool bTargetCompute)
{
	// TODO(RDG) convert table to math
	uint32 Bits;
	Bits  = (uint32)bCurrentCompute;
	Bits |= (uint32)bTargetCompute << 1;

	EResourceTransitionPipeline Table[] = {
		EResourceTransitionPipeline::EGfxToGfx,
		EResourceTransitionPipeline::EComputeToGfx,
		EResourceTransitionPipeline::EGfxToCompute,
		EResourceTransitionPipeline::EComputeToCompute
	};
	
	return static_cast< EResourceTransitionPipeline >( Table[ Bits ] );
}

void FRDGBuilder::TransitionTexture( const FRDGTexture* Texture, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const
{
	const bool bRequiredWritable = TransitionAccess != EResourceTransitionAccess::EReadable;

	if( Texture->bWritable != bRequiredWritable || Texture->bCompute != bRequiredCompute )
	{
		RHICmdList.TransitionResource( TransitionAccess, Texture->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture );
		Texture->bWritable = bRequiredWritable;
		Texture->bCompute = bRequiredCompute;
	}
}

void FRDGBuilder::TransitionUAV(FUnorderedAccessViewRHIParamRef UAV, const FRDGResource* UnderlyingResource, EResourceTransitionAccess TransitionAccess, bool bRequiredCompute ) const
{
	const bool bRequiredWritable = true;

	if(UnderlyingResource->bWritable != bRequiredWritable || UnderlyingResource->bCompute != bRequiredCompute )
	{
		EResourceTransitionPipeline TransitionPipeline = CalcTransitionPipeline(UnderlyingResource->bCompute, bRequiredCompute );
		RHICmdList.TransitionResource( TransitionAccess, TransitionPipeline, UAV);
		UnderlyingResource->bWritable = bRequiredWritable;
		UnderlyingResource->bCompute = bRequiredCompute;
	}
}

void FRDGBuilder::PushDrawEventStack(const FRenderGraphPass* Pass)
{
	// Push the scope event.
	{
		// Find out how many scope events needs to be poped.
		TStaticArray<const FRDGEventScope*, kMaxScopeCount> TraversedScopes;
		int32 CommonScopeId = -1;
		int32 TraversedScopeCount = 0;
		const FRDGEventScope* PassParentScope = Pass->ParentScope;
		while (PassParentScope)
		{
			TraversedScopes[TraversedScopeCount] = PassParentScope;

			for (int32 i = 0; i < ScopesStack.Num(); i++)
			{
				if (ScopesStack[i] == PassParentScope)
				{
					CommonScopeId = i;
					break;
				}
			}

			if (CommonScopeId != -1)
			{
				break;
			}

			TraversedScopeCount++;
			PassParentScope = PassParentScope->ParentScope;
		}

		// Pop no longer used scopes
		for (int32 i = CommonScopeId + 1; CommonScopeId >= 0 && i < kMaxScopeCount; i++)
		{
			if (!ScopesStack[i])
				break;

			RHICmdList.PopEvent();
			ScopesStack[i] = nullptr;
		}

		// Push new scopes
		const FColor ScopeColor(0);
		for (int32 i = TraversedScopeCount - 1; i >= 0; i--)
		{
			RHICmdList.PushEvent(TraversedScopes[i]->Name.GetTCHAR(), ScopeColor);
			CommonScopeId++;
			ScopesStack[CommonScopeId] = TraversedScopes[i];
		}
	}

	// Push the pass's event with some color.
	{
		FColor Color(0, 0, 0);

		if (Pass->IsCompute())
		{
			// Green for compute.
			Color = FColor(128, 255, 128);
		}
		else
		{
			// Ref for rasterizer.
			Color = FColor(255, 128, 128);
		}

		RHICmdList.PushEvent(Pass->GetName(), Color);
	}
}

void FRDGBuilder::ExecutePass( const FRenderGraphPass* Pass )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRDGBuilder_ExecutePass);

	FRHIRenderPassInfo RPInfo;
	bool bHasRenderTargets = false;

	AllocateAndTransitionPassResources(Pass, &RPInfo, &bHasRenderTargets);

	if (RENDER_GRAPH_DRAW_EVENTS)
	{
		if (GetEmitDrawEvents())
		{
			PushDrawEventStack(Pass);
		}
	}

	if( !Pass->IsCompute())
	{
		check(bHasRenderTargets);
		RHICmdList.BeginRenderPass( RPInfo, Pass->GetName() );
	}
	else
	{
		UnbindRenderTargets(RHICmdList);
	}
	
	// The name of the pass just for debuging convenience when crashing in the Execute().
	const TCHAR* PassName = Pass->GetName();

	Pass->Execute(RHICmdList);

	if( bHasRenderTargets )
	{
		RHICmdList.EndRenderPass();
	}

	if (RENDER_GRAPH_DRAW_EVENTS)
	{
		if (GetEmitDrawEvents())
		{
			RHICmdList.PopEvent();
		}
	}

	if (RENDER_GRAPH_DEBUGGING)
	{
		WarnForUselessPassDependencies(Pass);
	}

	// Can't release resources with immediate mode, because don't know if whether they are gonna be used.
	if (!GRenderGraphImmediateMode)
	{
		ReleaseUnecessaryResources(Pass);
	}
}

void FRDGBuilder::AllocateAndTransitionPassResources(const FRenderGraphPass* Pass, struct FRHIRenderPassInfo* OutRPInfo, bool* bOutHasRenderTargets)
{
	bool bIsCompute = Pass->IsCompute();
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	const bool bGeneratingMips = (Pass->GetFlags() & ERenderGraphPassFlags::GenerateMips) == ERenderGraphPassFlags::GenerateMips;
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
		uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_TEXTURE:
		{
			FRDGTexture* RESTRICT Texture = *ParameterStruct.GetMemberPtrAtOffset<FRDGTexture*>(Offset);
			if (Texture)
			{
				AllocateRHITextureIfNeeded(Texture, bIsCompute);
				TransitionTexture(Texture, EResourceTransitionAccess::EReadable, bIsCompute);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_SRV:
		{
			FRDGTextureSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureSRV*>(Offset);
			if (SRV)
			{
				check(SRV->Desc.Texture);

				AllocateRHITextureSRVIfNeeded(SRV, bIsCompute);
				TransitionTexture(SRV->Desc.Texture, EResourceTransitionAccess::EReadable, bIsCompute);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_UAV:
		{
			FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
			if (UAV)
			{
				AllocateRHITextureUAVIfNeeded(UAV, bIsCompute);
				TransitionUAV(UAV->CachedRHI.UAV, UAV->Desc.Texture, EResourceTransitionAccess::EWritable, bIsCompute);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_BUFFER:
		{
			FRDGBuffer* RESTRICT Buffer = *ParameterStruct.GetMemberPtrAtOffset<FRDGBuffer*>(Offset);
			if (Buffer)
			{
				AllocateRHIBufferIfNeeded(Buffer, bIsCompute);

				// TODO(RDG): supper hacky, find the UAV and transition it. Hopefully there is one...
				check(Buffer->PooledBuffer->UAVs.Num() == 1);
				FUnorderedAccessViewRHIParamRef UAV = Buffer->PooledBuffer->UAVs.CreateIterator().Value();
				TransitionUAV(UAV, Buffer, EResourceTransitionAccess::EReadable, bIsCompute);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_BUFFER_SRV:
		{
			FRDGBufferSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGBufferSRV*>(Offset);
			if (SRV)
			{
				AllocateRHIBufferSRVIfNeeded(SRV, bIsCompute);

				// TODO(RDG): supper hacky, find the UAV and transition it. Hopefully there is one...
				check(SRV->Desc.Buffer->PooledBuffer->UAVs.Num() == 1);
				FUnorderedAccessViewRHIParamRef UAV = SRV->Desc.Buffer->PooledBuffer->UAVs.CreateIterator().Value();
				TransitionUAV(UAV, SRV->Desc.Buffer, EResourceTransitionAccess::EReadable, bIsCompute);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_BUFFER_UAV:
		{
			FRDGBufferUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGBufferUAV*>(Offset);
			if (UAV)
			{
				AllocateRHIBufferUAVIfNeeded(UAV, bIsCompute);
				TransitionUAV(UAV->CachedRHI.UAV, UAV->Desc.Buffer, EResourceTransitionAccess::EWritable, bIsCompute);
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			check(!bIsCompute);

			FRenderTargetBindingSlots* RESTRICT RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);

			uint32 NumRenderTargets = 0;
			uint32 NumDepthStencilTargets = 0;
			uint32 NumSamples = 0;

			for (int i = 0; i < RenderTargets->Output.Num(); i++)
			{
				const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
				if (RenderTarget.GetTexture())
				{
					AllocateRHITextureIfNeeded(RenderTarget.GetTexture(), false);

					// TODO(RDG): should force TargetableTexture == ShaderResourceTexture with MSAA, and instead have an explicit MSAA resolve pass.
					OutRPInfo->ColorRenderTargets[i].RenderTarget = RenderTarget.GetTexture()->PooledRenderTarget->GetRenderTargetItem().TargetableTexture;
					OutRPInfo->ColorRenderTargets[i].ResolveTarget = nullptr;
					OutRPInfo->ColorRenderTargets[i].ArraySlice = -1;
					OutRPInfo->ColorRenderTargets[i].MipIndex = RenderTarget.GetMipIndex();
					OutRPInfo->ColorRenderTargets[i].Action = MakeRenderTargetActions(RenderTarget.GetLoadAction(), RenderTarget.GetStoreAction());

					if (!bGeneratingMips)
					{
						// Implicit assurance the RHI will do the correct transitions
						TransitionTexture(RenderTarget.GetTexture(), EResourceTransitionAccess::EWritable, false);
					}

					NumSamples |= OutRPInfo->ColorRenderTargets[i].RenderTarget->GetNumSamples();
					NumRenderTargets++;
				}
				else
				{
					break;
				}
			}

			const FDepthStencilBinding& DepthStencil = RenderTargets->DepthStencil;
			if (DepthStencil.Texture)
			{
				AllocateRHITextureIfNeeded(DepthStencil.Texture, false);

				OutRPInfo->DepthStencilRenderTarget.DepthStencilTarget = DepthStencil.Texture->PooledRenderTarget->GetRenderTargetItem().TargetableTexture;
				OutRPInfo->DepthStencilRenderTarget.ResolveTarget = nullptr;
				OutRPInfo->DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(
					MakeRenderTargetActions(DepthStencil.DepthLoadAction, DepthStencil.DepthStoreAction),
					MakeRenderTargetActions(DepthStencil.StencilLoadAction, DepthStencil.StencilStoreAction));
				OutRPInfo->DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;

				TransitionTexture(DepthStencil.Texture, EResourceTransitionAccess::EWritable, false);

				NumSamples |= OutRPInfo->DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples();
				NumDepthStencilTargets++;
			}

			OutRPInfo->bIsMSAA = NumSamples > 1;

			*bOutHasRenderTargets = NumRenderTargets + NumDepthStencilTargets > 0;
		}
		break;
		default:
			break;
		}
	}

	OutRPInfo->bGeneratingMips = bGeneratingMips;
}

// static 
void FRDGBuilder::WarnForUselessPassDependencies(const FRenderGraphPass* Pass)
{
	if (!GRenderGraphEmitWarnings)
	{
		return;
	}

	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	int32 TrackedResourceCount = 0;
	int32 UsedResourceCount = 0;

	// First pass to count resources.
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
		uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

		if (Type != UBMT_GRAPH_TRACKED_TEXTURE && Type != UBMT_GRAPH_TRACKED_SRV && Type != UBMT_GRAPH_TRACKED_UAV &&
			Type != UBMT_GRAPH_TRACKED_BUFFER && Type != UBMT_GRAPH_TRACKED_BUFFER_SRV && Type != UBMT_GRAPH_TRACKED_BUFFER_UAV)
			continue;

		const FRDGResource* Resource = *ParameterStruct.GetMemberPtrAtOffset<const FRDGResource*>(Offset);

		if (!Resource)
			continue;

		TrackedResourceCount++;
		UsedResourceCount += Resource->bIsActuallyUsedByPass ? 1 : 0;
	}

	if (TrackedResourceCount != UsedResourceCount)
	{
		UE_LOG(LogRendererCore, Warning, TEXT("%i of the %i resources of the pass %s where not actually used."), TrackedResourceCount - UsedResourceCount, TrackedResourceCount, Pass->GetName());

		for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
		{
			EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
			uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

			if (Type != UBMT_GRAPH_TRACKED_TEXTURE && Type != UBMT_GRAPH_TRACKED_SRV && Type != UBMT_GRAPH_TRACKED_UAV &&
				Type != UBMT_GRAPH_TRACKED_BUFFER && Type != UBMT_GRAPH_TRACKED_BUFFER_SRV && Type != UBMT_GRAPH_TRACKED_BUFFER_UAV)
				continue;

			const FRDGResource* Resource = *ParameterStruct.GetMemberPtrAtOffset<const FRDGResource*>(Offset);

			if (!Resource)
				continue;

			if (!Resource->bIsActuallyUsedByPass)
				UE_LOG(LogRendererCore, Warning, TEXT("	%s"), Resource->Name);
		}
	}

	// Last pass to clean the bIsActuallyUsedByPass flags.
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
		uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

		if (Type != UBMT_GRAPH_TRACKED_TEXTURE && Type != UBMT_GRAPH_TRACKED_SRV && Type != UBMT_GRAPH_TRACKED_UAV &&
			Type != UBMT_GRAPH_TRACKED_BUFFER && Type != UBMT_GRAPH_TRACKED_BUFFER_SRV && Type != UBMT_GRAPH_TRACKED_BUFFER_UAV)
			continue;

		const FRDGResource* Resource = *ParameterStruct.GetMemberPtrAtOffset<const FRDGResource*>(Offset);

		if (!Resource)
			continue;

		Resource->bIsActuallyUsedByPass = false;
	}
}

void FRDGBuilder::ReleaseRHITextureIfPossible(const FRDGTexture* Texture)
{
	check(Texture->ReferenceCount > 0);
	Texture->ReferenceCount--;

	if (Texture->ReferenceCount == 0)
	{
		Texture->PooledRenderTarget = nullptr;
		AllocatedTextures.FindChecked(Texture) = nullptr;
	}
}

void FRDGBuilder::ReleaseRHIBufferIfPossible(const FRDGBuffer* Buffer)
{
	check(Buffer->ReferenceCount > 0);
	Buffer->ReferenceCount--;

	if (Buffer->ReferenceCount == 0)
	{
		Buffer->PooledBuffer = nullptr;
		AllocatedBuffers.FindChecked(Buffer) = nullptr;
	}
}

void FRDGBuilder::ReleaseUnecessaryResources(const FRenderGraphPass* Pass)
{
	FShaderParameterStructRef ParameterStruct = Pass->GetParameters();

	/** Increments all the FRDGResource::ReferenceCount. */
	// TODO(RDG): Investigate the cost of branch miss-prediction.
	for (int ResourceIndex = 0, Num = ParameterStruct.Layout->Resources.Num(); ResourceIndex < Num; ResourceIndex++)
	{
		EUniformBufferBaseType Type = ParameterStruct.Layout->Resources[ResourceIndex].MemberType;
		uint16 Offset = ParameterStruct.Layout->Resources[ResourceIndex].MemberOffset;

		switch (Type)
		{
		case UBMT_GRAPH_TRACKED_TEXTURE:
		{
			FRDGTexture* RESTRICT Texture = *ParameterStruct.GetMemberPtrAtOffset<FRDGTexture*>(Offset);
			if (Texture)
			{
				ReleaseRHITextureIfPossible(Texture);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_SRV:
		{
			FRDGTextureSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureSRV*>(Offset);
			if (SRV)
			{
				ReleaseRHITextureIfPossible(SRV->Desc.Texture);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_UAV:
		{
			FRDGTextureUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGTextureUAV*>(Offset);
			if (UAV)
			{
				ReleaseRHITextureIfPossible(UAV->Desc.Texture);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_BUFFER:
		{
			FRDGBuffer* RESTRICT Buffer = *ParameterStruct.GetMemberPtrAtOffset<FRDGBuffer*>(Offset);
			if (Buffer)
			{
				ReleaseRHIBufferIfPossible(Buffer);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_BUFFER_SRV:
		{
			FRDGBufferSRV* RESTRICT SRV = *ParameterStruct.GetMemberPtrAtOffset<FRDGBufferSRV*>(Offset);
			if (SRV)
			{
				ReleaseRHIBufferIfPossible(SRV->Desc.Buffer);
			}
		}
		break;
		case UBMT_GRAPH_TRACKED_BUFFER_UAV:
		{
			FRDGBufferUAV* RESTRICT UAV = *ParameterStruct.GetMemberPtrAtOffset<FRDGBufferUAV*>(Offset);
			if (UAV)
			{
				ReleaseRHIBufferIfPossible(UAV->Desc.Buffer);
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			FRenderTargetBindingSlots* RESTRICT RenderTargets = ParameterStruct.GetMemberPtrAtOffset<FRenderTargetBindingSlots>(Offset);

			for (int i = 0; i < RenderTargets->Output.Num(); i++)
			{
				const FRenderTargetBinding& RenderTarget = RenderTargets->Output[i];
				if (RenderTarget.GetTexture())
				{
					ReleaseRHITextureIfPossible(RenderTarget.GetTexture());
				}
				else
				{
					break;
				}
			}
			if (RenderTargets->DepthStencil.Texture)
			{
				ReleaseRHITextureIfPossible(RenderTargets->DepthStencil.Texture);
			}
		}
		break;
		default:
			break;
		}
	}
}

void FRDGBuilder::ProcessDeferredInternalResourceQueries()
{
	for (const auto& Query : DeferredInternalTextureQueries)
	{
		check(Query.Texture->PooledRenderTarget);

		if (Query.bTransitionToRead)
		{
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Query.Texture->PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture);
		}

		*Query.OutTexturePtr = AllocatedTextures.FindChecked(Query.Texture);

		// No need to manually release in immediate mode, since it is done directly when emptying AllocatedTextures in DestructPasses().
		if (!GRenderGraphImmediateMode)
		{
			ReleaseRHITextureIfPossible(Query.Texture);
		}
	}
}

void FRDGBuilder::DestructPasses()
{
	#if RENDER_GRAPH_DRAW_EVENTS == 2
	{
		// Event scopes are allocated on FMemStack, so need to call their destructor because have a FString within them.
		for (FRDGEventScope* EventScope : EventScopes)
		{
			EventScope->~FRDGEventScope();
		}
		EventScopes.Empty();
	}
	#endif

	#if RENDER_GRAPH_DEBUGGING
	{
		// Make sure all resource references have been released to ensure no leaks happen.
		for (const FRDGResource* Resource : Resources)
		{
			check(Resource->ReferenceCount == 0);
		}
		Resources.Empty();
	}
	#endif

	// Passes are allocated on FMemStack, so need to call destructor manually.
	for (FRenderGraphPass* Pass : Passes)
	{
		Pass->~FRenderGraphPass();
	}

	Passes.Empty();
	DeferredInternalTextureQueries.Empty();
	AllocatedTextures.Empty();
}