
#include "TressFXShortCut.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "ScreenRendering.h"
#include "TressFX/TressFXSceneProxy.h"
#include "UniformBuffer.h"
#include "TressFX/TressFXSimulation.h"
#include "BasePassRendering.h"
#include "TressFX/TressFXTypes.h"
#include "Engine/TressFXAsset.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

//
//bool FTressFXShortCut_FillColorsPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//{
//	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//}
//
//void FTressFXShortCut_FillColorsPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters & Parameters, FShaderCompilerEnvironment & OutEnvironment)
//{
//	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//	FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
//}
//
//const TCHAR* FTressFXShortCut_FillColorsPS::GetSourceFilename()
//{
//	return TEXT("TressFXShortCut_FillColorsPS");
//}
//
//const TCHAR* FTressFXShortCut_FillColorsPS::GetFunctionName()
//{
//	return TEXT("main");
//}
//
//FTressFXShortCut_FillColorsPS::FTressFXShortCut_FillColorsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
//{
//	g_vViewport.Bind(Initializer.ParameterMap, TEXT("g_vViewport"));
//	g_mInvViewProj.Bind(Initializer.ParameterMap, TEXT("g_mInvViewProj"));
//}
//
//bool FTressFXShortCut_FillColorsPS::Serialize(FArchive& Ar)
//{
//	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
//	Ar << g_vViewport << g_mInvViewProj;
//	return bShaderHasOutdatedParameters;
//}
//
//void FTressFXShortCut_FillColorsPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext, FTressFXSceneProxy* Proxy)
//{
//	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
//
//	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
//
//	FIntRect ViewRect = View.UnscaledViewRect;
//	SetShaderValue(RHICmdList, GetPixelShader(), g_mInvViewProj, View.ViewMatrices.GetInvTranslatedViewProjectionMatrix());
//	SetShaderValue(RHICmdList, GetPixelShader(), g_vViewport, FVector4(0, 0, ViewRect.Width(), ViewRect.Height()));
//	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FViewUniformShaderParameters>(), View.ViewUniformBuffer);
//	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FTressFXShadeParametersUniformBuffer>(), Proxy->TressFXHairObject->ShadeParametersUniformBuffer);
//	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FPrimitiveUniformShaderParameters>(), Proxy->GetUniformBuffer());
//	SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FForwardLightData>(), View.ForwardLightingResources->ForwardLightDataUniformBuffer);
//}
//
//IMPLEMENT_SHADER_TYPE(, FTressFXShortCut_FillColorsPS, TEXT("/Engine/Private/TressFXShortCut_FillColorsPS.usf"), TEXT("main"), SF_Pixel);

void FTressFXShortCut_ResolveColorPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters & Parameters, FShaderCompilerEnvironment & OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
}


bool FTressFXShortCut_ResolveColorPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);

}

const TCHAR* FTressFXShortCut_ResolveColorPS::GetSourceFilename()
{
	return TEXT("/Engine/Private/TressFXShortCut_ResolveColorPS.usf");
}

const TCHAR* FTressFXShortCut_ResolveColorPS::GetFunctionName()
{
	return TEXT("main");
}

FTressFXShortCut_ResolveColorPS::FTressFXShortCut_ResolveColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	FragmentColorsTexture.Bind(Initializer.ParameterMap, TEXT("FragmentColorsTexture"));
	tAccumInvAlpha.Bind(Initializer.ParameterMap, TEXT("tAccumInvAlpha"));
	vFragmentBufferSize.Bind(Initializer.ParameterMap, TEXT("vFragmentBufferSize"));
}

bool FTressFXShortCut_ResolveColorPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << FragmentColorsTexture << tAccumInvAlpha << vFragmentBufferSize;
	return bShaderHasOutdatedParameters;
}

void FTressFXShortCut_ResolveColorPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

	SetTextureParameter(RHICmdList, GetPixelShader(), FragmentColorsTexture, SceneContext.FragmentColorsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SetTextureParameter(RHICmdList, GetPixelShader(), tAccumInvAlpha, SceneContext.AccumInvAlpha->GetRenderTargetItem().ShaderResourceTexture);

	FIntPoint BufferSize = View.ViewRect.Size();
	SetShaderValue(RHICmdList, GetPixelShader(), vFragmentBufferSize, FVector4(BufferSize.X, BufferSize.Y, 0, 0));

}

IMPLEMENT_SHADER_TYPE(, FTressFXShortCut_ResolveColorPS, TEXT("/Engine/Private/TressFXShortCut_ResolveColorPS.usf"), TEXT("main"), SF_Pixel);



void FTressFXShortCut_ResolveDepthPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
}

bool FTressFXShortCut_ResolveDepthPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

const TCHAR* FTressFXShortCut_ResolveDepthPS::GetSourceFilename()
{
	return TEXT("/Engine/Private/TressFXShortCut_ResolveDepthPS.usf");
}

const TCHAR* FTressFXShortCut_ResolveDepthPS::GetFunctionName()
{
	return TEXT("main");
}

FTressFXShortCut_ResolveDepthPS::FTressFXShortCut_ResolveDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	FragmentDepthsTexture.Bind(Initializer.ParameterMap, TEXT("FragmentDepthsTexture"));
	SceneTextureShaderParameters.Bind(Initializer);
}

void FTressFXShortCut_ResolveDepthPS::SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext)
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
	SetTextureParameter(RHICmdList, ShaderRHI, FragmentDepthsTexture, SceneContext.FragmentDepthsTexture->GetRenderTargetItem().ShaderResourceTexture);
	SceneTextureShaderParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
}

bool FTressFXShortCut_ResolveDepthPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << FragmentDepthsTexture << SceneTextureShaderParameters;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FTressFXShortCut_ResolveDepthPS, TEXT("/Engine/Private/TressFXShortCut_ResolveDepthPS.usf"), TEXT("main"), SF_Pixel);



extern void DrawRectangle(
	FRHICommandList& RHICmdList,
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	FShader* VertexShader,
	EDrawRectangleFlags Flags,
	uint32 InstanceCount
);





//void CopyDepth(FRHICommandList& RHICmdList, const FViewInfo& View, FSceneRenderTargets& SceneContext, FTextureRHIRef Dest)
//{
//
//
//
//	//FRHIDepthRenderTargetView DepthView(Dest, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop);
//	SetRenderTarget(RHICmdList, nullptr, Dest, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite);
//	//DrawClearQuad(RHICmdList, View.GetFeatureLevel(), false, FLinearColor::White, true, 0, true, 0);
//
//
//	//SetRenderTargets(RHICmdList,0, nullptr, .Texture, 0, nullptr);
//
//	FGraphicsPipelineStateInitializer GraphicsPSOInit;
//
//	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
//
//
//	GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<false>(FM_Solid, CM_None);
//
//	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();
//
//	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
//
//
//	TShaderMapRef<FScreenVS> VertexShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
//	TShaderMapRef<FCopyDepthPS> PixelShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
//
//	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
//	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
//	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
//	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
//
//
//	//static FGlobalBoundShaderState BoundShaderState;
//	//SetGlobalBoundShaderState(RHICmdList, ERHIFeatureLevel::SM5, BoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
//
//	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
//
//	VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
//	PixelShader->SetParameters(RHICmdList, View);
//
//	//SetTextureParameter(RHICmdList, PixelShader->GetPixelShader(), PixelShader->SceneDepthTexture, FSceneRenderTargets::Get(RHICmdList).GetSceneDepthTexture());
//
//	// Draw
//	const auto Size = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
//	//RHICmdList.SetViewport(0, 0, 0, Size.X, Size.Y, 1);
//
//	DrawRectangle(
//		RHICmdList,
//		0, 0,
//		Size.X, Size.Y,
//		0, 0,
//		Size.X, Size.Y,
//		Size,
//		Size,
//		*VertexShader,
//		EDrawRectangleFlags::EDRF_Default,
//		1
//	);
//}

namespace TressFXRenderer
{

	void DrawShortCut(FRHICommandList& RHICmdList, FSceneRenderTargets& sceneContext, const FViewInfo& view)
	{


		return;

//		SCOPED_DRAW_EVENT(RHICmdList, TressFXShortcutDraw);
//		if (!view.ViewState)
//		{
//			return;
//		}
//
//
//
//		{
//			/*
//			for (auto InProxy : view.VisibleTressFX)
//			{
//				auto TFXProxy = static_cast<FTressFXSceneProxy*>(InProxy->Proxy);
//
//				if (!TFXProxy || !TFXProxy->Texture.IsValid())
//					continue;
//
//				SimulateTressFX(RHICmdList, TFXProxy);
//			}
//		*/
//
//		//RHICmdList.()
//
//		}
//
//		//CopyDepth(RHICmdList, view, sceneContext, sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture);
//#pragma region "shortcut pass 1"
//
//		{
//
//
//			//RHICmdList.SetScissorRect(true, view.ViewRect.Min.X, view.ViewRect.Min.Y, view.ViewRect.Max.X, view.ViewRect.Max.Y);
//
//
//
//			SCOPED_DRAW_EVENT(RHICmdList, DrawPassOne)
//				FRHIRenderTargetView RTV[MaxSimultaneousRenderTargets];
//
//			RTV[0] = FRHIRenderTargetView(sceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::EClear);
//			TShaderMapRef<FTressFXShortCutBaseVS>			VertexShader(view.ShaderMap);
//			TShaderMapRef<FTressFXShortCut_DepthsAlphaPS>	PixelShader(view.ShaderMap);
//
//			//FRHIRenderTargetView InvAlphaRenderTarget(sceneContext.ShortCutInvAlphaTexture->GetRenderTargetItem().TargetableTexture);
//			FUnorderedAccessViewRHIParamRef UAVs[1] = { sceneContext.FragmentDepthsTexture->GetRenderTargetItem().UAV };
//			static const uint32 ClearValue[4] = { SHORTCUT_INITIAL_DEPTH ,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH,SHORTCUT_INITIAL_DEPTH };
//			ClearUAV(RHICmdList, sceneContext.FragmentDepthsTexture->GetRenderTargetItem(), ClearValue);
//
//
//			SetRenderTargets(RHICmdList, 1, &RTV->Texture, sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture, 0, nullptr);
//
//			//RHICmdList.ClearColorTexture(RTV[0].Texture, FLinearColor::White, FIntRect());
//
//			//SetRenderTargets(RHICmdList, 1, &InvAlphaRenderTarget.Texture, sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture, 0, nullptr);
//
//			//RHICmdList.ClearColorTexture(InvAlphaRenderTarget.Texture, FLinearColor::White, FIntRect());
//			FUAVCountInitializerRHI UAVCountInitializer(0, 0, 0, 0, 0, 0, 0, 0);
//
//
//			//RHICmdList.ClearDepthStencilTexture(sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture, EClearDepthStencil::DepthStencil, 1.f, 0, FIntRect());
//			SetRenderTargets(RHICmdList, 1, &RTV[0].Texture, sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture, 1, UAVs, false, &UAVCountInitializer);
//
//			FGraphicsPipelineStateInitializer PSOInitializer;
//
//			RHICmdList.ApplyCachedRenderTargets(PSOInitializer);
//
//			FBlendStateInitializerRHI::FRenderTarget DepthWritesToColor_BS;
//
//			DepthWritesToColor_BS.ColorBlendOp = BO_Add;
//			DepthWritesToColor_BS.ColorSrcBlend = BF_Zero;
//			DepthWritesToColor_BS.ColorDestBlend = BF_SourceColor;
//			DepthWritesToColor_BS.AlphaSrcBlend = BF_Zero;
//			DepthWritesToColor_BS.AlphaDestBlend = BF_SourceAlpha;
//			DepthWritesToColor_BS.AlphaBlendOp = BO_Add;
//			DepthWritesToColor_BS.ColorWriteMask = EColorWriteMask::CW_RED;
//
//			PSOInitializer.BlendState = RHICreateBlendState(DepthWritesToColor_BS);//, FLinearColor::Transparent, 0x000000ff;
//			PSOInitializer.BlendFactor = FLinearColor::Transparent;
//			PSOInitializer.SampleMask = 0x0000000ff;
//
//			PSOInitializer.DepthStencilState = TStaticDepthStencilState<
//				true, CF_GreaterEqual,
//				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedDecrement,
//				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedDecrement,
//				0xff, 0xff, DWM_Zero>::CreateRHI();
//
//			PSOInitializer.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW, false, true, true>::CreateRHI();
//
//
//			/*RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
//				true, CF_GreaterEqual,
//				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedDecrement,
//				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedDecrement,
//				0xff, 0xff, DWM_Zero>::CreateRHI(), 0x00);*/
//
//
//				//RHICmdList.SetRasterizerState(TStaticRasterizerState
//				//	<FM_Solid, CM_CW, false, true,true>::CreateRHI());
//
//			PSOInitializer.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
//			PSOInitializer.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
//			PSOInitializer.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
//			PSOInitializer.PrimitiveType = PT_TriangleList;
//
//			SetGraphicsPipelineState(RHICmdList, PSOInitializer);
//			//PSOInitializer.BoundShaderState = Create
//			//static FGlobalBoundShaderState BoundShaderState;
//			//SetGlobalBoundShaderState(RHICmdList, view.GetFeatureLevel(), BoundShaderState, GEmptyVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
//
//
//
//			for (auto InProxy : view.VisibleTressFX)
//			{
//				auto TFXProxy = static_cast<FTressFXSceneProxy*>(InProxy->Proxy);
//
//				if (!TFXProxy || !TFXProxy->TressFXHairObject || !TFXProxy->TressFXHairObject->Texture.IsValid() || !TFXProxy->bInitialized)
//					continue;
//
//				SCOPED_DRAW_EVENTF(RHICmdList, DrawPassOneAsset, TEXT("DrawPassOneAsset  %s"), *TFXProxy->TFXComponent->Asset->GetName());
//				PixelShader->SetParameters(RHICmdList, view, sceneContext, TFXProxy);
//				VertexShader->SetShaderParms(RHICmdList, TFXProxy, view);
//
//				RHICmdList.DrawIndexedInstanced(TFXProxy->TressFXHairObject->IndexBuffer.IndexBufferRHI, TFXProxy->TressFXHairObject->mtotalIndices, PT_TriangleList, 0, 0, 1, 0);
//			}
//
//		}
//#pragma endregion
//#pragma  region "pass 2"
//
//		{
//			FTextureRHIParamRef RTVs[1] = { nullptr };
//			FUnorderedAccessViewRHIParamRef UAVs[2] = { nullptr,nullptr };
//			SetRenderTargets(RHICmdList, 1, RTVs, nullptr, 2, UAVs);
//		}
//
//		{
//			SCOPED_DRAW_EVENT(RHICmdList, DrawPassPassTwo)
//				TShaderMapRef<FScreenVS>							ScreenVertexShader(view.ShaderMap);
//			TShaderMapRef<FTressFXShortCut_ResolveDepthPS>		PixelShader(view.ShaderMap);
//			static FGlobalBoundShaderState BoundState;
//
//			SetRenderTargets(RHICmdList, 0, nullptr, sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture, 0, nullptr); // TODO: Add uav transion 
//
//			FGraphicsPipelineStateInitializer PSOInitializer;
//
//			RHICmdList.ApplyCachedRenderTargets(PSOInitializer);
//
//			PSOInitializer.DepthStencilState = TStaticDepthStencilState<
//				true, CF_GreaterEqual
//			>::GetRHI();
//
//			PSOInitializer.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
//			PSOInitializer.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
//			PSOInitializer.PrimitiveType = PT_TriangleList;
//
//			SetGraphicsPipelineState(RHICmdList, PSOInitializer);
//
//			//SetGlobalBoundShaderState(RHICmdList, view.GetFeatureLevel(), BoundState, GScreenVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);
//
//
//			ScreenVertexShader->SetParameters(RHICmdList, view.ViewUniformBuffer);
//			PixelShader->SetParameters(RHICmdList, view, sceneContext);
//
//			const auto Size = view.ViewRect.Size(); //FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
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
//
//		}
//
//#pragma endregion
//#pragma region "pass 3"
//		{
//			SCOPED_DRAW_EVENT(RHICmdList, DrawPassThree)
//				FUnorderedAccessViewRHIParamRef UAVs[7] = { nullptr,nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
//			FUAVCountInitializerRHI UAVCounts(0, 0, 0, 0, 0, 0, 0, 0);
//			FRHIRenderTargetView RTV(sceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::EClear);
//			SetRenderTargets(RHICmdList, 1, &RTV.Texture,
//				sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture,
//				7, UAVs, false, &UAVCounts);
//			DrawClearQuad(RHICmdList, true, FLinearColor::Transparent, false, 0.f, false, 0);
//			//RHICmdList.ClearColorTexture(sceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture, FLinearColor::Transparent,FIntRect());
//
//
//			FGraphicsPipelineStateInitializer PSOInitializer;
//
//			RHICmdList.ApplyCachedRenderTargets(PSOInitializer);
//
//			TShaderMapRef<FTressFXShortCutBaseVS> VertexShader(view.ShaderMap);
//			TShaderMapRef<FTressFXShortCut_FillColorsPS> PixelShader(view.ShaderMap);
//			FBlendStateInitializerRHI::FRenderTarget Sum_BS;
//
//			Sum_BS.ColorBlendOp = BO_Add;
//			Sum_BS.ColorSrcBlend = BF_One;
//			Sum_BS.ColorDestBlend = BF_One;
//			Sum_BS.AlphaSrcBlend = BF_One;
//			Sum_BS.AlphaDestBlend = BF_One;
//			Sum_BS.AlphaBlendOp = BO_Add;
//			Sum_BS.ColorWriteMask = EColorWriteMask::CW_RGBA;
//
//
//
//			PSOInitializer.BlendState = RHICreateBlendState(Sum_BS);
//
//			PSOInitializer.BlendFactor = FLinearColor::Transparent;
//			PSOInitializer.SampleMask = 0xffffffff;
//
//			PSOInitializer.DepthStencilState =
//				TStaticDepthStencilState<
//				true, CF_GreaterEqual,
//				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedDecrement,
//				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedDecrement,
//				0xff, 0xff, DWM_Zero
//				>::GetRHI();// , 0x00;
//
//			PSOInitializer.StencilRef = 0x00;
//
//			PSOInitializer.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW, false, true, true>::GetRHI();
//
//			PSOInitializer.PrimitiveType = PT_TriangleList;
//
//			PSOInitializer.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
//			PSOInitializer.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
//
//			SetGraphicsPipelineState(RHICmdList, PSOInitializer);
//			//static FGlobalBoundShaderState BoundShaderState;
//			//SetGlobalBoundShaderState(RHICmdList, view.GetFeatureLevel(), BoundShaderState, GEmptyVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
//
//
//
//			for (auto InProxy : view.VisibleTressFX)
//			{
//				auto TFXProxy = static_cast<FTressFXSceneProxy*>(InProxy->Proxy);
//
//				if (!TFXProxy || !TFXProxy->TressFXHairObject->Texture.IsValid() || !TFXProxy->bInitialized)
//					continue;
//				SCOPED_DRAW_EVENTF(RHICmdList, DrawPassThreeAsset, TEXT("DrawPassOneAsset  %s"), *TFXProxy->TFXComponent->Asset->GetName());
//				PixelShader->SetParameters(RHICmdList, view, sceneContext, TFXProxy);
//				VertexShader->SetShaderParms(RHICmdList, TFXProxy, view);
//
//				RHICmdList.DrawIndexedInstanced(TFXProxy->TressFXHairObject->IndexBuffer.IndexBufferRHI, TFXProxy->TressFXHairObject->mtotalIndices, PT_TriangleList, 0, 0, 1, 0);
//			}
//
//		}
//
//		//return;
//#pragma endregion
//#pragma region "pass 4"
//
//
//		{
//			SCOPED_DRAW_EVENT(RHICmdList, DrawPassFour)
//				FTextureRHIParamRef Resources[2] = { sceneContext.AccumInvAlpha->GetRenderTargetItem().TargetableTexture,sceneContext.FragmentColorsTexture->GetRenderTargetItem().TargetableTexture };
//			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, Resources, 2);
//
//			TShaderMapRef<FScreenVS>							ScreenVertexShader(view.ShaderMap);
//			TShaderMapRef<FTressFXShortCut_ResolveColorPS>		PixelShader(view.ShaderMap);
//			static FGlobalBoundShaderState BoundState;
//
//			FRHIRenderTargetView RTV(sceneContext.GetSceneColorTexture(), ERenderTargetLoadAction::ELoad);
//
//			SetRenderTargets(RHICmdList, 1, &RTV.Texture, sceneContext.ShortCutSceneDepth->GetRenderTargetItem().TargetableTexture, 0, nullptr);
//			FGraphicsPipelineStateInitializer PSOInitializer;
//
//			RHICmdList.ApplyCachedRenderTargets(PSOInitializer);
//
//			PSOInitializer.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, false, false>::GetRHI();
//
//			FBlendStateInitializerRHI::FRenderTarget ResolveColor_BS;
//
//			ResolveColor_BS.AlphaBlendOp = BO_Add;
//			ResolveColor_BS.AlphaDestBlend = BF_Zero;
//			ResolveColor_BS.AlphaSrcBlend = BF_Zero;
//			ResolveColor_BS.ColorSrcBlend = BF_One;
//			ResolveColor_BS.ColorDestBlend = BF_SourceAlpha;
//			ResolveColor_BS.ColorBlendOp = BO_Add;
//			ResolveColor_BS.ColorWriteMask = CW_RGBA;
//
//			PSOInitializer.BlendState = RHICreateBlendState(ResolveColor_BS);// , FLinearColor::Transparent, 0xff);
//
//			//SetGraphicsPipelineState(RHICmdList,)
//
//
//
//
//
//			PSOInitializer.DepthStencilState =
//				TStaticDepthStencilState<
//				false, CF_GreaterEqual,
//				true, CF_Less, SO_Keep, SO_Keep, SO_Keep,
//				true, CF_Less, SO_Keep, SO_Keep, SO_Keep,
//				0xff, 0x00, DWM_All, true>::GetRHI();
//
//			PSOInitializer.PrimitiveType = PT_TriangleList;
//
//			PSOInitializer.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
//			PSOInitializer.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
//
//			//SetGlobalBoundShaderState(RHICmdList, view.GetFeatureLevel(), BoundState, GScreenVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);
//
//			ScreenVertexShader->SetParameters(RHICmdList, view.ViewUniformBuffer);
//			PixelShader->SetParameters(RHICmdList, view, sceneContext);
//
//			const auto Size = view.ViewRect.Size();//FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
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
//
//		}

#pragma endregion

	}

}


