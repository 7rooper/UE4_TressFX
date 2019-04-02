
#include "TressFX/TressFXBoneSkinning.h"
#include "Engine/Public/TressFX/TressFXTypes.h"

bool FTressBoneSkinningCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FTressBoneSkinningCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), TRESSFX_SIM_THREAD_GROUP_SIZE);
	OutEnvironment.SetDefine(TEXT("AMD_TRESSFX_MAX_NUM_BONES"), AMD_TRESSFX_MAX_NUM_BONES);
}

const TCHAR* FTressBoneSkinningCS::GetSourceFilename()
{
	return  TEXT("TressFXBoneSkinning");
}

const TCHAR* FTressBoneSkinningCS::GetFunctionName()
{
	return  TEXT("BoneSkinning");
}

FTressBoneSkinningCS::FTressBoneSkinningCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
{
	BoneSkinningData.Bind(Initializer.ParameterMap, TEXT("g_BoneSkinningData"));
	InitialVertexPositions.Bind(Initializer.ParameterMap, TEXT("initialVertexPositions"));
	ColMeshVertexPositions.Bind(Initializer.ParameterMap, TEXT("collMeshVertexPositions"));
	TressFXBoneSkinningParametersUniformBuffer.Bind(Initializer.ParameterMap, TEXT("TressFXBoneSkinningParameters"));
}

bool FTressBoneSkinningCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << BoneSkinningData << InitialVertexPositions << ColMeshVertexPositions;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FTressBoneSkinningCS, TEXT("/Engine/Private/TressFXBoneSkinning.usf"), TEXT("BoneSkinning"), SF_Compute)