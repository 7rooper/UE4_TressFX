
#include "TressFXBoneSkinning.h"
#include "TressFXTypes.h"

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
	BoneSkinningData.Bind(Initializer.ParameterMap, TEXT("BoneSkinningData"));
	InitialVertexPositions.Bind(Initializer.ParameterMap, TEXT("InitialVertexPositions"));
	ColMeshVertexPositions.Bind(Initializer.ParameterMap, TEXT("CollMeshVertexPositions"));
	TressFXBoneSkinningParametersUniformBuffer.Bind(Initializer.ParameterMap, TEXT("TressFXBoneSkinningParameters"));
	BoneIndexData.Bind(Initializer.ParameterMap, TEXT("BoneIndexData"));
}

bool FTressBoneSkinningCS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << BoneSkinningData << InitialVertexPositions << ColMeshVertexPositions << BoneIndexData;
	return bShaderHasOutdatedParameters;
}

IMPLEMENT_SHADER_TYPE(, FTressBoneSkinningCS, TEXT("/Plugin/TressFX/Private/TressFXBoneSkinning.usf"), TEXT("BoneSkinning"), SF_Compute)