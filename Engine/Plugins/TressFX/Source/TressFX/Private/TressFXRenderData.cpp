#include "TressFXRenderData.h"

void FTressFXShaderUtil::UploadGPUData(FStructuredBufferRHIParamRef Buffer, int32 ElementSize, int32 ElementCount, void* InData, EResourceLockMode LockMode/*=RLM_WriteOnly*/)
{
    void* LockData = RHILockStructuredBuffer(Buffer, 0, ElementSize*ElementCount, LockMode);
    FMemory::Memcpy(LockData, InData, ElementSize*ElementCount);
    RHIUnlockStructuredBuffer(Buffer);
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTressFXStrandStyleUniformBuffer, "TressFXStrandStyleParameters");
//FTressFXStrandStyleUniformBuffer::FTressFXStrandStyleUniformBuffer()
//{
//    FMemory::Memzero(*this);
//}
//

FTressFXStrandCollection::FTressFXStrandCollection(FTressFXStrandStyleParameter* InStrandParameter, uint32 InNumOfStrands)
{
    if (InStrandParameter == nullptr)
        return;

    StrandCombineData.Init(FVector4(1,1,1,1), InNumOfStrands);
    StrandStyleParam = FVector4(0, InStrandParameter->StrandLength, 0, 0);
    StrandClumpParam = InStrandParameter->GetClumpParam();
    StrandStiffnessParam = InStrandParameter->GetStiffnessParam();
    StrandWavinessParam = InStrandParameter->GetWavinessParam();
}


void FTressFXStrandCollection::InitResources(uint32 InNumOfStrands)
{
    if (InNumOfStrands > 0)
    {
        NumOfStrands = InNumOfStrands;

        StrandCombineData.Init(FVector4(1, 1, 1, 1), InNumOfStrands);
        StrandStyleMaskBuffer.Initialize(sizeof(FVector4), InNumOfStrands, 0, TEXT("StrandStyleMaskBuffer"));

        FTressFXShaderUtil::UploadGPUData(StrandStyleMaskBuffer.Buffer, sizeof(float), InNumOfStrands, StrandCombineData.GetData(), RLM_WriteOnly);
    }
}


void FTressFXStrandCollection::ReleaseResources()
{
    StrandStyleMaskBuffer.Release();
}

void FTressFXStrandCollection::UpdateStrandParameter(FTressFXStrandStyleParameter* InStrandParameter)
{
    if (InStrandParameter == nullptr)
        return;

    //clamp value in range
    StrandStyleParam = FVector4(0, InStrandParameter->StrandLength, 0, 0);
    StrandClumpParam = InStrandParameter->GetClumpParam();
    StrandStiffnessParam = InStrandParameter->GetStiffnessParam();
    StrandWavinessParam = InStrandParameter->GetWavinessParam();
}

void FTressFXStrandCollection::UpdateBuffer()
{
    FTressFXStrandStyleUniformBuffer StrandStyleParameter;
    StrandStyleParameter.g_StrandStyle = StrandStyleParam;
    //Scale, roughness, noise
    StrandStyleParameter.g_StrandClumpParam = StrandClumpParam;
    //stiffness root, tip, reserved, waviness root straighten
    StrandStyleParameter.g_StrandStiffnessParam = StrandStiffnessParam;
    //waviness scale, scale noise, freq, freq nosie
    StrandStyleParameter.g_StrandWavinessParam = StrandWavinessParam;

    StrandStyleUniformBuffer = TUniformBufferRef<FTressFXStrandStyleUniformBuffer>::CreateUniformBufferImmediate(StrandStyleParameter, UniformBuffer_SingleFrame);
}

void FTressFXStrandCollection::UAVBarrierCSToVS(FRHICommandList& RHICmdList, FComputeFenceRHIParamRef Fence)
{
    FUnorderedAccessViewRHIParamRef UAVs[] =
    {
        StrandStyleMaskBuffer.UAV,
    };

    RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, UAVs, ARRAY_COUNT(UAVs), Fence);
}

void FTressFXStrandCollection::UAVBarrierCSToCS(FRHICommandList& RHICmdList, FComputeFenceRHIParamRef Fence)
{
    FUnorderedAccessViewRHIParamRef UAVs[] =
    {
        StrandStyleMaskBuffer.UAV,
    };

    RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, UAVs, ARRAY_COUNT(UAVs), Fence);
}

void FTressFXStrandCollection::UAVBarrierVSToCS(FRHICommandList& RHICmdList, FComputeFenceRHIParamRef Fence)
{
    FUnorderedAccessViewRHIParamRef UAVs[] =
    {
        StrandStyleMaskBuffer.UAV,
    };

    RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, UAVs, ARRAY_COUNT(UAVs), Fence);
}

void FTressFXStrandCollection::SetUAV(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader, uint32 index)
{
    RHICmdList.SetUAVParameter(Shader, index, StrandStyleMaskBuffer.UAV);
}

void FTressFXStrandCollection::UnsetUAV(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader, uint32 index)
{
    RHICmdList.SetUAVParameter(Shader, index, nullptr);
}