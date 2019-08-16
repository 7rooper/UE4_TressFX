
#include "TressFXVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "ShaderParameterUtils.h"
#include "UniformBuffer.h"
#include "Shader.h"
#include "RenderResource.h"
#include "CommonRenderResources.h"
#include "MeshMaterialShader.h"
#include "RendererInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(TressFXVertexFactoryLog, Log, All);
DEFINE_LOG_CATEGORY(TressFXVertexFactoryLog);

void FTressFXVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	TressfxShadeParameters.Bind(ParameterMap, TEXT("TressfxShadeParameters"));
	HairThicknessCoeffs.Bind(ParameterMap, TEXT("g_HairThicknessCoeffs"));
	HairRootToTipTexcoords.Bind(ParameterMap, TEXT("g_HairRootToTipTexcoords"));
	HairStrandTexCd.Bind(ParameterMap, TEXT("g_HairStrandTexCd"));
	NumVerticesPerStrand.Bind(ParameterMap, TEXT("g_NumVerticesPerStrand"));
	g_bThinTip.Bind(ParameterMap, TEXT("g_bThinTip"));
	g_GuideHairVertexPositions.Bind(ParameterMap, TEXT("g_GuideHairVertexPositions"));
	g_GuideHairVertexTangents.Bind(ParameterMap, TEXT("g_GuideHairVertexTangents"));
	g_HairVertexPositionsPrev.Bind(ParameterMap, TEXT("g_HairVertexPositionsPrev"));
}

void FTressFXVertexFactoryShaderParameters::Serialize(FArchive& Ar)
{
	Ar << TressfxShadeParameters
		<< HairThicknessCoeffs
		<< HairRootToTipTexcoords
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
		ShaderBindings.Add(TressfxShadeParameters, BatchData->InstanceRenderData->ShadeParametersUniformBuffer);
		ShaderBindings.Add(g_bThinTip, 1); // new system does not accept bools
		ShaderBindings.Add(g_GuideHairVertexPositions, BatchData->InstanceRenderData->PosTanCollection.Positions.SRV);
		ShaderBindings.Add(g_HairVertexPositionsPrev, BatchData->InstanceRenderData->PosTanCollection.PositionsPrev.SRV);
		ShaderBindings.Add(g_GuideHairVertexTangents, BatchData->InstanceRenderData->PosTanCollection.Tangents.SRV);
		ShaderBindings.Add(HairRootToTipTexcoords, BatchData->TressFXHairObject->HairRootToTipTexcoords.SRV);
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
#ifdef TRESSFX_STANDALONE_PLUGIN
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::Type::SM5) && (Material->IsSpecialEngineMaterial());
#else
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::Type::SM5) && (Material->IsUsedWithTressFX() || Material->IsSpecialEngineMaterial());
#endif
}

void FTressFXVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Type, Platform, Material, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("TRESSFX_VERTEX_FACTORY"), 1);
	OutEnvironment.SetDefine(TEXT("WITH_TRESSFX_VERTEX_FACTORY"), 1);
	OutEnvironment.SetDefine(TEXT("USE_TRESSFX"), 1);
#ifdef TRESSFX_STANDALONE_PLUGIN
	OutEnvironment.SetDefine(TEXT("TRESSFX_STANDALONE_PLUGIN"), 1);
#endif
}

void FTressFXVertexFactory::InitRHI()
{
	SetDeclaration(GEmptyVertexDeclaration.VertexDeclarationRHI);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FTressFXVertexFactory, "/Engine/Private/TressFXVertexFactory.ush", true, false, true, false, false);