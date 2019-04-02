
#include "TressFX/TressFXVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "SpeedTreeWind.h"
#include "ShaderParameterUtils.h"
#include "Rendering/ColorVertexBuffer.h"
#include "UniformBuffer.h"
#include "SceneView.h"
#include "Shader.h"
#include "MeshBatch.h"
#include "ShaderParameterUtils.h"
#include "RenderResource.h"
#include "RendererInterface.h"

void FTressFXVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{

	tressfxShadeParameters.Bind(ParameterMap, TEXT("tressfxShadeParameters"));
	HairThicknessCoeffs.Bind(ParameterMap, TEXT("g_HairThicknessCoeffs"));
	HairStrandTexCd.Bind(ParameterMap, TEXT("g_HairStrandTexCd"));
	NumVerticesPerStrand.Bind(ParameterMap, TEXT("g_NumVerticesPerStrand"));
	g_bThinTip.Bind(ParameterMap, TEXT("g_bThinTip"));
	g_GuideHairVertexPositions.Bind(ParameterMap, TEXT("g_GuideHairVertexPositions"));
	g_GuideHairVertexTangents.Bind(ParameterMap, TEXT("g_GuideHairVertexTangents"));
	g_HairVertexPositionsPrev.Bind(ParameterMap, TEXT("g_HairVertexPositionsPrev"));

}

void FTressFXVertexFactoryShaderParameters::Serialize(FArchive& Ar)
{
	//FVertexFactoryShaderParameters::Serialize(Ar);

	Ar << tressfxShadeParameters
		<< HairThicknessCoeffs
		<< HairStrandTexCd
		<< NumVerticesPerStrand
		<< g_bThinTip
		<< g_GuideHairVertexPositions
		<< g_GuideHairVertexTangents
		<< g_HairVertexPositionsPrev;

}

void FTressFXVertexFactoryShaderParameters::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const
{
	const FTressFXVertexFactory* TFXVertexFactory = ((const FTressFXVertexFactory*)VertexFactory);
	FRHIVertexShader* VertexShader = Shader->GetVertexShader();

	if (TFXVertexFactory->TressFXHairObject)
	{
		FUnorderedAccessViewRHIParamRef UAVs[] = {
			TFXVertexFactory->TressFXHairObject->PosTanCollection.Positions.UAV
			,TFXVertexFactory->TressFXHairObject->PosTanCollection.Tangents.UAV
		};

		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, UAVs, ARRAY_COUNT(UAVs));

		SetUniformBufferParameter(RHICmdList, VertexShader, Shader->GetUniformBufferParameter<FViewUniformShaderParameters>(), View.ViewUniformBuffer);
		SetShaderValue(RHICmdList, VertexShader, g_bThinTip, true); // just always use true for this, no ones wants hairs that are an inch thick

		SetUniformBufferParameter(RHICmdList, VertexShader, tressfxShadeParameters, TFXVertexFactory->TressFXHairObject->ShadeParametersUniformBuffer);
		SetSRVParameter(RHICmdList, VertexShader, g_GuideHairVertexPositions, TFXVertexFactory->TressFXHairObject->PosTanCollection.Positions.SRV);

		SetSRVParameter(RHICmdList, VertexShader, g_HairVertexPositionsPrev, TFXVertexFactory->TressFXHairObject->PosTanCollection.PositionsPrev.SRV);

		SetSRVParameter(RHICmdList, VertexShader, g_GuideHairVertexTangents, TFXVertexFactory->TressFXHairObject->PosTanCollection.Tangents.SRV);
		SetSRVParameter(RHICmdList, VertexShader, HairStrandTexCd, TFXVertexFactory->TressFXHairObject->HairTexCoords.SRV);
		SetSRVParameter(RHICmdList, VertexShader, HairThicknessCoeffs, TFXVertexFactory->TressFXHairObject->HairThicknessCoeffs.SRV);
	}
}

FVertexFactoryShaderParameters* FTressFXVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	if (ShaderFrequency == SF_Vertex)
	{
		return new FTressFXVertexFactoryShaderParameters();
	}
	return NULL;
}

bool FTressFXVertexFactory::ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::Type::SM5) && (Material->IsUsedWithTressFX() || Material->IsSpecialEngineMaterial());
}

void FTressFXVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("TRESSFX_VERTEX_FACTORY"), 1);
	OutEnvironment.SetDefine(TEXT("WITH_TRESSFX_VERTEX_FACTORY"), 1);
	OutEnvironment.SetDefine(TEXT("USE_TRESSFX"), 1);
}

TGlobalResource<FEmptyVertexDeclaration> GTFXVertexDeclaration;

void FTressFXVertexFactory::InitRHI()
{
	SetDeclaration(GTFXVertexDeclaration.VertexDeclarationRHI);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FTressFXVertexFactory, "/Engine/Private/TressFXVertexFactory.ush", true, false, true, false, false);
