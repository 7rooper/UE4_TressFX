#pragma once


#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Components.h"
#include "VertexFactory.h"
#include "TressFXTypes.h"


class ENGINE_API FTressFXVertexFactory : public FVertexFactory
{

	DECLARE_VERTEX_FACTORY_TYPE(FTressFXVertexFactory);

public:

	struct FDataType
	{

	};

	FTressFXVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) : FVertexFactory(InFeatureLevel), TressFXHairObject(nullptr) {}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
	static bool SupportsTessellationShaders() { return false; }

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory?
	*/
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	static void ModifyCompilationEnvironment(const FVertexFactoryType* Type, EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	FTressFXHairObject* TressFXHairObject;

	virtual void InitRHI() override;

};

/**
* Shader parameters for LocalVertexFactory.
*/
class FTressFXVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override;
	virtual void Serialize(FArchive& Ar) override;
	
	//virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override;

	virtual void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		bool bShaderRequiresPositionOnlyStream,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const override;


	FTressFXVertexFactoryShaderParameters()
	{
	}
	FShaderUniformBufferParameter tressfxShadeParameters;

	FShaderResourceParameter HairThicknessCoeffs;
	FShaderResourceParameter HairStrandTexCd;
	FShaderResourceParameter g_GuideHairVertexPositions;
	FShaderResourceParameter g_GuideHairVertexTangents;

	FShaderParameter g_bThinTip;
	FShaderParameter NumVerticesPerStrand;

	FShaderResourceParameter g_HairVertexPositionsPrev;

};
