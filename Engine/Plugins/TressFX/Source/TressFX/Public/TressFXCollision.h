
#pragma once
#include "ShaderCore.h"
#include "ShaderParameters.h"
#include "GlobalShader.h"

class FInitializeSignedDistanceFieldCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FInitializeSignedDistanceFieldCS, Global, ENGINE_API)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static const TCHAR* GetSourceFilename()
	{
		return  TEXT("TressFXSDFCollision");
	}

	static const TCHAR* GetFunctionName()
	{
		return  TEXT("InitializeSignedDistanceField");
	}

	FInitializeSignedDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
	{
		g_SignedDistanceField.Bind(Initializer.ParameterMap, TEXT("g_SignedDistanceField"));
		ConstBuffer_SDF.Bind(Initializer.ParameterMap, TEXT("ConstBuffer_SDF"));

	}

	virtual bool Serialize(FArchive& Ar) override
	{

		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ConstBuffer_SDF << g_SignedDistanceField;
		return bShaderHasOutdatedParameters;
	}

	FInitializeSignedDistanceFieldCS() {}

public:

	FRWShaderParameter g_SignedDistanceField;
	FShaderUniformBufferParameter ConstBuffer_SDF;

};


class FConstructSignedDistanceFieldCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FConstructSignedDistanceFieldCS, Global, ENGINE_API)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static const TCHAR* GetSourceFilename()
	{
		return  TEXT("TressFXSDFCollision");
	}

	static const TCHAR* GetFunctionName()
	{
		return  TEXT("ConstructSignedDistanceField");
	}

	FConstructSignedDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
	{
		ConstBuffer_SDF.Bind(Initializer.ParameterMap, TEXT("ConstBuffer_SDF"));
		g_SignedDistanceField.Bind(Initializer.ParameterMap, TEXT("g_SignedDistanceField"));
		ColMeshVertexPositions.Bind(Initializer.ParameterMap, TEXT("collMeshVertexPositions"));
		g_TrimeshVertexIndices.Bind(Initializer.ParameterMap, TEXT("g_TrimeshVertexIndices"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{

		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ConstBuffer_SDF << g_SignedDistanceField << g_TrimeshVertexIndices << ColMeshVertexPositions;
		return bShaderHasOutdatedParameters;
	}

	FConstructSignedDistanceFieldCS() {}

public:

	FShaderUniformBufferParameter ConstBuffer_SDF;
	FShaderResourceParameter g_TrimeshVertexIndices;
	FRWShaderParameter ColMeshVertexPositions;
	FRWShaderParameter g_SignedDistanceField;

};

class FFinalizeSignedDistanceFieldCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FFinalizeSignedDistanceFieldCS, Global, ENGINE_API)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}


	static const TCHAR* GetSourceFilename()
	{
		return  TEXT("TressFXSDFCollision");
	}

	static const TCHAR* GetFunctionName()
	{
		return  TEXT("FinalizeSignedDistanceField");
	}

	FFinalizeSignedDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
	{
		ConstBuffer_SDF.Bind(Initializer.ParameterMap, TEXT("ConstBuffer_SDF"));
		g_SignedDistanceField.Bind(Initializer.ParameterMap, TEXT("g_SignedDistanceField"));
	}

	FFinalizeSignedDistanceFieldCS() {}

	virtual bool Serialize(FArchive& Ar) override
	{

		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ConstBuffer_SDF << g_SignedDistanceField;
		return bShaderHasOutdatedParameters;
	}

public:

	FRWShaderParameter g_SignedDistanceField;
	FShaderUniformBufferParameter ConstBuffer_SDF;
};



class FCollideHairVerticesWithSdf_forwardCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FCollideHairVerticesWithSdf_forwardCS, Global, ENGINE_API)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}


	static const TCHAR* GetSourceFilename()
	{
		return  TEXT("TressFXSDFCollision");
	}

	static const TCHAR* GetFunctionName()
	{
		return  TEXT("CollideHairVerticesWithSdf_forward");
	}

	FCollideHairVerticesWithSdf_forwardCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
	{
		ConstBuffer_SDF.Bind(Initializer.ParameterMap, TEXT("ConstBuffer_SDF"));
		g_SignedDistanceField.Bind(Initializer.ParameterMap, TEXT("g_SignedDistanceField"));
		g_PrevHairVertices.Bind(Initializer.ParameterMap, TEXT("g_PrevHairVertices"));
		g_HairVertices.Bind(Initializer.ParameterMap, TEXT("g_HairVertices"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{

		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ConstBuffer_SDF << g_SignedDistanceField << g_HairVertices << g_PrevHairVertices;
		return bShaderHasOutdatedParameters;
	}

	FCollideHairVerticesWithSdf_forwardCS() {}

public:

	FRWShaderParameter g_HairVertices;
	FRWShaderParameter g_PrevHairVertices;
	FRWShaderParameter g_SignedDistanceField;
	FShaderUniformBufferParameter ConstBuffer_SDF;

};

class FCollideHairVerticesWithSdfCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FCollideHairVerticesWithSdfCS, Global, ENGINE_API)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}


	static const TCHAR* GetSourceFilename()
	{
		return  TEXT("TressFXSDFCollision");
	}

	static const TCHAR* GetFunctionName()
	{
		return  TEXT("CollideHairVerticesWithSdf");
	}

	FCollideHairVerticesWithSdfCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
	{
		ConstBuffer_SDF.Bind(Initializer.ParameterMap, TEXT("ConstBuffer_SDF"));
		g_SignedDistanceField.Bind(Initializer.ParameterMap, TEXT("g_SignedDistanceField"));
		g_PrevHairVertices.Bind(Initializer.ParameterMap, TEXT("g_PrevHairVertices"));
		g_HairVertices.Bind(Initializer.ParameterMap, TEXT("g_HairVertices"));
	}

	virtual bool Serialize(FArchive& Ar) override
	{

		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ConstBuffer_SDF << g_SignedDistanceField << g_HairVertices << g_PrevHairVertices;
		return bShaderHasOutdatedParameters;
	}

	FCollideHairVerticesWithSdfCS() {}

public:

	FRWShaderParameter g_HairVertices;
	FRWShaderParameter g_PrevHairVertices;
	FRWShaderParameter g_SignedDistanceField;
	FShaderUniformBufferParameter ConstBuffer_SDF;
};

static TAutoConsoleVariable<int32> CVarTFXSDFApply(TEXT("tfx.SDFCollisionApply"), 0, TEXT("Final SDF Collision Apply Pass toggle."), ECVF_RenderThreadSafe);
void ENGINE_API UpdateSDF(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy);
FComputeFenceRHIRef ENGINE_API ApplySDF(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, FComputeFenceRHIRef InFence, FUnorderedAccessViewRHIParamRef SimResources[]);