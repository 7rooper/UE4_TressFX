
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

#pragma region "TressFX Simulation"

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

	FRWShaderParameter g_HairVertexPositions;
	FRWShaderParameter g_HairVertexPositionsPrev;
	FRWShaderParameter g_HairVertexPositionsPrevPrev;

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

	FShaderResourceParameter g_GlobalRotations;
	FShaderResourceParameter g_HairRefVecsInLocalFrame;
	FRWShaderParameter g_HairVertexPositions;
	FRWShaderParameter g_HairVertexTangents;


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
	FShaderResourceParameter g_GlobalRotations;
	FShaderResourceParameter g_HairRefVecsInLocalFrame;
	FRWShaderParameter g_HairVertexPositions;
	FRWShaderParameter g_HairVertexTangents;
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

	FRWShaderParameter g_HairVertexPositions;
	FRWShaderParameter g_HairVertexTangents;
	FRWShaderParameter g_HairVertexPositionsPrev;
    FRWShaderParameter g_HairVertexPositionsPrevPrev;
	FShaderResourceParameter g_HairRestLengthSRV;

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
		g_FollowHairRootOffset.Bind(Initializer.ParameterMap, TEXT("g_FollowHairRootOffset"));
		g_HairVertexTangents.Bind(Initializer.ParameterMap, TEXT("g_HairVertexTangents"));
		g_HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("g_HairVertexPositions"));
	}


	FUpdateFollowHairVerticesCS()
	{

	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << g_HairVertexPositions << g_HairVertexTangents << g_FollowHairRootOffset;
		return bShaderHasOutdatedParameters;
	}


public:
	FRWShaderParameter g_HairVertexPositions;
	FRWShaderParameter g_HairVertexTangents;
	FShaderResourceParameter g_FollowHairRootOffset;
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

	FRWShaderParameter g_HairVertexPositions;
	FRWShaderParameter g_HairVertexPositionsPrev;
	FShaderUniformBufferParameter TressfxSimParametersUniformBuffer;

};


#pragma endregion

void TRESSFX_API SimulateTressFX(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations);
