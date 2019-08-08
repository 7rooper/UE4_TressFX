#pragma once

#include "ShaderCore.h"
#include "ShaderParameters.h"
#include "GlobalShader.h"

class FTressBoneSkinningCS : public FGlobalShader
{

	DECLARE_EXPORTED_SHADER_TYPE(FTressBoneSkinningCS, Global, ENGINE_API)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static const TCHAR* GetSourceFilename();

	static const TCHAR* GetFunctionName();

	FTressBoneSkinningCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	virtual bool Serialize(FArchive& Ar) override;

	FTressBoneSkinningCS() {}

public:

	FRWShaderParameter ColMeshVertexPositions;
	FShaderResourceParameter BoneSkinningData;
	FShaderResourceParameter InitialVertexPositions;

	FShaderUniformBufferParameter TressFXBoneSkinningParametersUniformBuffer;

};