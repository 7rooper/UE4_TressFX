
#pragma once
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "TressFXTypes.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"
#include "GPUSkinPublicDefs.h"
#include "ShaderCore.h"
#include "GlobalShader.h"

static TAutoConsoleVariable<int32> CVarTFXSDFDisable(TEXT("tfx.SDFCollisionDisable"), 1, TEXT("Disable SDF Collision"), ECVF_RenderThreadSafe);

struct BoneSkinningData
{
	FVector4 boneIndex;  // x, y, z and w component are four bone indices per strand
	FVector4 boneWeight; // x, y, z and w component are four bone weights per strand
};


class FVelocityShockPropagationCS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FVelocityShockPropagationCS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FVelocityShockPropagationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);


	FVelocityShockPropagationCS();

	virtual bool Serialize(FArchive& Ar) override;


public:

	FRWShaderParameter HairVertexPositions;
	FRWShaderParameter HairVertexPositionsPrev;
	FRWShaderParameter HairVertexPositionsPrevPrev;

};


class FLocalShapeConstraintsCS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FLocalShapeConstraintsCS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FLocalShapeConstraintsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);


	FLocalShapeConstraintsCS();

	virtual bool Serialize(FArchive& Ar) override;


public:

	FShaderResourceParameter GlobalRotations;
	FShaderResourceParameter HairRefVecsInLocalFrame;
	FRWShaderParameter HairVertexPositions;
	FRWShaderParameter HairVertexTangents;


};



class FLocalShapeConstraintsWithIterationCS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FLocalShapeConstraintsWithIterationCS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FLocalShapeConstraintsWithIterationCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);


	FLocalShapeConstraintsWithIterationCS();

	virtual bool Serialize(FArchive& Ar) override;


public:
	FShaderResourceParameter GlobalRotations;
	FShaderResourceParameter HairRefVecsInLocalFrame;
	FRWShaderParameter HairVertexPositions;
	FRWShaderParameter HairVertexTangents;
};


class FLengthConstriantsWindAndCollisionCS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FLengthConstriantsWindAndCollisionCS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FLengthConstriantsWindAndCollisionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
	FLengthConstriantsWindAndCollisionCS();

	virtual bool Serialize(FArchive& Ar) override;


public:

	FRWShaderParameter HairVertexPositions;
	FRWShaderParameter HairVertexTangents;
	FRWShaderParameter HairVertexPositionsPrev;
	FRWShaderParameter HairVertexPositionsPrevPrev;
	FShaderResourceParameter HairRestLengthSRV;

};

class FUpdateFollowHairVerticesCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FUpdateFollowHairVerticesCS, Global, TRESSFX_API);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Plugin/TressFX/Private/TressFXSimulation.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("UpdateFollowHairVertices");
	}

	FUpdateFollowHairVerticesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		FollowHairRootOffset.Bind(Initializer.ParameterMap, TEXT("FollowHairRootOffset"));
		HairVertexTangents.Bind(Initializer.ParameterMap, TEXT("HairVertexTangents"));
		HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("HairVertexPositions"));
	}


	FUpdateFollowHairVerticesCS()
	{

	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HairVertexPositions << HairVertexTangents << FollowHairRootOffset;
		return bShaderHasOutdatedParameters;
	}


public:
	FRWShaderParameter HairVertexPositions;
	FRWShaderParameter HairVertexTangents;
	FShaderResourceParameter FollowHairRootOffset;
};


class FPrepareFollowHairBeforeTurningIntoGuideCS : public FGlobalShader
{

	DECLARE_SHADER_TYPE(FPrepareFollowHairBeforeTurningIntoGuideCS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FPrepareFollowHairBeforeTurningIntoGuideCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);


	FPrepareFollowHairBeforeTurningIntoGuideCS();

	virtual bool Serialize(FArchive& Ar) override;


public:

	FRWShaderParameter HairVertexPositions;
	FRWShaderParameter HairVertexPositionsPrev;
	FShaderUniformBufferParameter TressfxSimParametersUniformBuffer;

};

void TRESSFX_API SimulateTressFX(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations);

template <FTressFXSimFeatures::Type TSimFeatures>
void SimulateTressFX_impl(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations);