
#include "TressFX/TressFXVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "ShaderParameterUtils.h"
#include "UniformBuffer.h"
#include "Shader.h"
#include "RenderResource.h"
#include "CommonRenderResources.h"
#include "MeshMaterialShader.h"
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
	Ar << tressfxShadeParameters
		<< HairThicknessCoeffs
		<< HairStrandTexCd
		<< NumVerticesPerStrand
		<< g_bThinTip
		<< g_GuideHairVertexPositions
		<< g_GuideHairVertexTangents
		<< g_HairVertexPositionsPrev;
}

void FTressFXVertexFactoryShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	bool bShaderRequiresPositionOnlyStream,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams
) const
{
	FTressFXVertexFactoryUserData* BatchData = (FTressFXVertexFactoryUserData*)BatchElement.VertexFactoryUserData;
	const FTressFXVertexFactory* TFXVertexFactory = ((const FTressFXVertexFactory*)VertexFactory);
	FRHIVertexShader* VertexShader = Shader->GetVertexShader();
	if (BatchData->TressFXHairObject)
	{
		// UAVS should already be transitioned to readable by the end of the simulation since we dont have rhicmdlist here
		ShaderBindings.Add(tressfxShadeParameters, BatchData->TressFXHairObject->ShadeParametersUniformBuffer);
		ShaderBindings.Add(g_bThinTip, 1); // new system does not accept bools
		ShaderBindings.Add(g_GuideHairVertexPositions, BatchData->TressFXHairObject->PosTanCollection.Positions.SRV);
		ShaderBindings.Add(g_HairVertexPositionsPrev, BatchData->TressFXHairObject->PosTanCollection.PositionsPrev.SRV);
		ShaderBindings.Add(g_GuideHairVertexTangents, BatchData->TressFXHairObject->PosTanCollection.Tangents.SRV);
		ShaderBindings.Add(HairStrandTexCd, BatchData->TressFXHairObject->HairTexCoords.SRV);
		ShaderBindings.Add(HairThicknessCoeffs, BatchData->TressFXHairObject->HairThicknessCoeffs.SRV);
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

void FTressFXVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Type, Platform, Material, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("TRESSFX_VERTEX_FACTORY"), 1);
	OutEnvironment.SetDefine(TEXT("WITH_TRESSFX_VERTEX_FACTORY"), 1);
	OutEnvironment.SetDefine(TEXT("USE_TRESSFX"), 1);
}

void FTressFXVertexFactory::InitRHI()
{
	SetDeclaration(GEmptyVertexDeclaration.VertexDeclarationRHI);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FTressFXVertexFactory, "/Engine/Private/TressFXVertexFactory.ush", true, false, true, false, false);