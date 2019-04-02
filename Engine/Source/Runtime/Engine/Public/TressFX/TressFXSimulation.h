
#pragma once
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "TressFX/TressFXTypes.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "UniformBuffer.h"
#include "GPUSkinPublicDefs.h"
#include "ShaderCore.h"
#include "GlobalShader.h"

#pragma region "TressFX Simulation"

static TAutoConsoleVariable<int32> CVarTFXSDFDisable(TEXT("tfx.SDFCollisionDisable"), 1, TEXT("Disable SDF Collision"), ECVF_RenderThreadSafe);

struct FTressFXSimFeatures
{
	enum Type
	{
		None = 0,
		Morphs = 1,
		MorphsAndVelocity = 2,
		Velocity = 3
	};
};

struct BoneSkinningData
{
	FVector4 boneIndex;  // x, y, z and w component are four bone indices per strand
	FVector4 boneWeight; // x, y, z and w component are four bone weights per strand
};

template <FTressFXSimFeatures::Type TSimFeatures>
class FIntegrationAndGlobalShapeConstraintsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FIntegrationAndGlobalShapeConstraintsCS, Global);

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		const bool bHasMorphs = TSimFeatures == FTressFXSimFeatures::Morphs || TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity;
		const bool bHasVelocity = TSimFeatures == FTressFXSimFeatures::Velocity || TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity;
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MORPHS"), bHasMorphs ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("UPDATE_PREV_FOLLOW_HAIRS"), bHasVelocity ? TEXT("1") : TEXT("0"));
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("TressFXSimulation");
	};

	static const TCHAR*GetFunctionName()
	{
		return TEXT("IntegrationAndGlobalShapeConstraints");
	};

	FIntegrationAndGlobalShapeConstraintsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		g_HairVertexPositions.Bind(Initializer.ParameterMap, TEXT("g_HairVertexPositions"));
		g_HairVertexPositionsPrev.Bind(Initializer.ParameterMap, TEXT("g_HairVertexPositionsPrev"));
		g_HairVertexPositionsPrevPrev.Bind(Initializer.ParameterMap, TEXT("g_HairVertexPositionsPrevPrev"));
		g_HairVertexTangents.Bind(Initializer.ParameterMap, TEXT("g_HairVertexTangents"));
		g_BoneSkinningData.Bind(Initializer.ParameterMap, TEXT("g_BoneSkinningData"));
		g_InitialHairPositions.Bind(Initializer.ParameterMap, TEXT("g_InitialHairPositions"));

		const bool bHasMorphs = TSimFeatures == FTressFXSimFeatures::Morphs || TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity;
		if (bHasMorphs)
		{
			g_MorphDeltas.Bind(Initializer.ParameterMap, TEXT("g_MorphDeltas"));
		}
		const bool bHasVelocity = TSimFeatures == FTressFXSimFeatures::Velocity || TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity;
		if (bHasVelocity)
		{
			g_FollowHairRootOffset.Bind(Initializer.ParameterMap, TEXT("g_FollowHairRootOffset"));
		}

		TressfxSimParametersUniformBuffer.Bind(Initializer.ParameterMap, TEXT("tressfxSimParameters"));
	};


	FIntegrationAndGlobalShapeConstraintsCS() {};

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		Ar << g_HairVertexPositions << g_HairVertexPositionsPrev << g_HairVertexPositionsPrevPrev
			<< g_HairVertexTangents << g_BoneSkinningData << g_InitialHairPositions;

		const bool bHasMorphs = TSimFeatures == FTressFXSimFeatures::Morphs || TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity;
		if (bHasMorphs)
		{
			Ar << g_MorphDeltas;
		}
		const bool bHasVelocity = TSimFeatures == FTressFXSimFeatures::Velocity || TSimFeatures == FTressFXSimFeatures::MorphsAndVelocity;
		if (bHasVelocity)
		{
			Ar << g_FollowHairRootOffset;
		}

		return bShaderHasOutdatedParameters;
	};


public:

	FRWShaderParameter g_HairVertexPositions;
	FRWShaderParameter g_HairVertexPositionsPrev;
	FRWShaderParameter g_HairVertexPositionsPrevPrev;
	FRWShaderParameter g_HairVertexTangents;

	FShaderResourceParameter g_BoneSkinningData;
	FShaderResourceParameter g_InitialHairPositions;
	FShaderResourceParameter g_MorphDeltas;
	FShaderResourceParameter g_FollowHairRootOffset;

	FShaderUniformBufferParameter TressfxSimParametersUniformBuffer;

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
	FShaderResourceParameter g_HairRestLengthSRV;

};

class FUpdateFollowHairVerticesCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FUpdateFollowHairVerticesCS, Global, ENGINE_API);

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
		return TEXT("/Engine/Private/TressFXSimulation.usf");
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

void ENGINE_API SimulateTressFX(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations);

template <FTressFXSimFeatures::Type TSimFeatures>
void SimulateTressFX_impl(FRHICommandList& RHICmdList, class FTressFXSceneProxy* Proxy, int32 CPULocalShapeIterations);