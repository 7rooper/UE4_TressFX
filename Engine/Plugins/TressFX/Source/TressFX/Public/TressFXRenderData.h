#pragma once
#include "CoreMinimal.h"
#include "RHIResources.h"
#include "ShaderParameters.h"
#include "TressFXRenderData.generated.h"

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION < 22
BEGIN_UNIFORM_BUFFER_STRUCT_WITH_CONSTRUCTOR(FTressFXStrandStyleUniformBuffer, TRESSFX_API)
    UNIFORM_MEMBER(FVector4, g_StrandStyle)
    UNIFORM_MEMBER(FVector, g_StrandClumpParam)
    UNIFORM_MEMBER(FVector4, g_StrandStiffnessParam)
    UNIFORM_MEMBER(FVector4, g_StrandWavinessParam)
END_UNIFORM_BUFFER_STRUCT(FTressFXStrandStyleUniformBuffer)
#else
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FTressFXStrandStyleUniformBuffer, TRESSFX_API)
    SHADER_PARAMETER(FVector4, g_StrandStyle)
    SHADER_PARAMETER(FVector, g_StrandClumpParam)
    SHADER_PARAMETER(FVector4, g_StrandStiffnessParam)
    SHADER_PARAMETER(FVector4, g_StrandWavinessParam)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
#endif
struct TRESSFX_API FTressFXShaderUtil
{
    static void UploadGPUData(FRHIStructuredBuffer* Buffer, int32 ElementSize, int32 ElementCount, void* InData, EResourceLockMode LockMode = RLM_WriteOnly);
};


USTRUCT(BlueprintType)
struct FTressFXStrandStyleParameter
{
    GENERATED_USTRUCT_BODY()
public:
    FTressFXStrandStyleParameter() :
        StrandLength(1.0f)
        , StrandClumpScale(0.5f)
        , StrandClumpRoughness(0.5f)
        , StrandClumpNoise(0)
        , StrandStiffnessTip(0.5f)
        , StrandStiffnessRoot(1.0f)
        , StrandWavinessScale(0)
        , StrandWavinessScaleNoise(0)
        , StrandWavinessFreq(3)
        , StrandWavinessFreqNoise(0)
        , StrandWavinessRootStraighten(0)
    {}

    void VarifyParameter()
    {
        //clamp value in range
        StrandClumpScale = FMath::Clamp<float >(StrandClumpScale, -1.0f, 1.0f);
        StrandClumpRoughness = FMath::Clamp<float >(StrandClumpRoughness, 0.01f, 2.0f);
        StrandClumpNoise = FMath::Clamp<float >(StrandClumpNoise, 0, 1.0f);

        StrandStiffnessRoot = FMath::Clamp<float >(StrandStiffnessRoot, 0, 1.0f);
        StrandStiffnessTip = FMath::Clamp<float >(StrandStiffnessTip, 0, 1.0f);

        StrandWavinessScale = FMath::Clamp<float>(StrandWavinessScale, 0, 0.5);
        StrandWavinessFreq = FMath::Clamp<float>(StrandWavinessFreq, 1, 5);
        StrandWavinessScaleNoise = FMath::Clamp<float >(StrandWavinessScaleNoise, 0, 1.0f);
        StrandWavinessFreqNoise = FMath::Clamp<float >(StrandWavinessFreqNoise, 0, 1.0f);
        StrandWavinessRootStraighten = FMath::Clamp<float >(StrandWavinessRootStraighten, 0, 1.0f);
    }
    
    FVector GetClumpParam()
    {
        return FVector(StrandClumpScale, StrandClumpRoughness, StrandClumpNoise);
    }

    FVector4 GetStiffnessParam()
    {
        return FVector4(StrandStiffnessRoot, StrandStiffnessTip, 0, StrandWavinessRootStraighten);
    }
    FVector4 GetWavinessParam()
    {
        return FVector4(StrandWavinessScale, StrandWavinessScaleNoise, StrandWavinessFreq, StrandWavinessFreqNoise);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandLength;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandClumpScale;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandClumpRoughness;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandClumpNoise;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandStiffnessTip;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandStiffnessRoot;

    //g_StrandWavinessParam in uniform buffer 
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandWavinessScale;
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandWavinessScaleNoise;
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandWavinessFreq;
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandWavinessFreqNoise;

    //g_StrandStiffnessParam in uniform buffer
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StrandStyle")
        float StrandWavinessRootStraighten;
};

//follow strand buffer

struct FTressFXStrandCollection
{
public:
    FTressFXStrandCollection() {}
    FTressFXStrandCollection(FTressFXStrandStyleParameter* InStrandParameter, uint32 NumOfStrands);
    void InitResources(uint32 InNumOfStrands);
    void ReleaseResources();

    void UAVBarrierCSToVS(FRHICommandList& RHICmdList, FRHIComputeFence* Fence);
    void UAVBarrierCSToCS(FRHICommandList& RHICmdList, FRHIComputeFence* Fence);
    void UAVBarrierVSToCS(FRHICommandList& RHICmdList, FRHIComputeFence* Fence);

    //call in game thread
    void UpdateStrandParameter(FTressFXStrandStyleParameter* InStrandParameter);
    //only call in render thread
    void UpdateBuffer();

    void SetUAV(FRHICommandList& RHICmdList, FRHIComputeShader* Shader, uint32 index);

    void UnsetUAV(FRHICommandList& RHICmdList, FRHIComputeShader* Shader, uint32 index);

    FRWBufferStructured StrandStyleMaskBuffer;
    TUniformBufferRef<FTressFXStrandStyleUniformBuffer> StrandStyleUniformBuffer;

    TArray<FVector4> StrandCombineData;
    FVector4 StrandStyleParam;
    FVector StrandClumpParam;
    FVector4 StrandStiffnessParam;
    FVector4 StrandWavinessParam;

    uint32 NumOfStrands;
};