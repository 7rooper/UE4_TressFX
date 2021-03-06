#pragma  once

#include "PrimitiveSceneProxy.h"
#include "Engine/Classes/Materials/MaterialInterface.h"
#include "TressFXTypes.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Containers/ResourceArray.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "TressFXComponent.h"
#include "TressFXBoneSkinningAsset.h"
#include "TressFXVertexFactory.h"
#include "RHIResources.h"
#ifndef TRESSFX_STANDALONE_PLUGIN
	#include "TressFXPublicDef.h"
#endif

struct FBoneSkinningData
{
	FVector4 boneIndex;
	FVector4 weight;
};

struct FTressFXSimParameters
{

public:

	FVector4 Wind;

	// damping, local stiffness, global stiffness, global range.
	// X: m_Damping;
	// Y: m_StiffnessForLocalShapeMatching;
	// Z: m_StiffnessForGlobalShapeMatching;
	// W: m_GlobalShapeMatchingEffectiveRange;
	FVector4 m_Shape;

	// gravity, time step size,
	// X: m_GravityMagnitude;
	// Y: m_TimeStep;
	// Z: m_TipSeparationFactor;
	// W: m_velocityShockPropogation;
	FVector4 m_GravTimeTip;

	// 4th component unused.
	// X:  m_NumLengthConstraintIterations;
	// Y:  m_NumLocalShapeMatchingIterations;
	// Z:  seems to be unused!!!! m_bCollision on off
	// W:  unused;
	FIntVector4 m_SimInts; 

	// X: m_NumOfStrandsPerThreadGroup;
	// Y: m_NumFollowHairsPerGuideHair;
	// Z: m_NumVerticesPerStrand; // should be 2^n (n is integer and greater than 2) and less than or equal to TRESSFX_SIM_THREAD_GROUP_SIZE. i.e. 8, 16, 32 or 64
	// W: total hairs
	FIntVector4 m_Counts;

	// X: VelocityShockPropogation;
	// Y: VSP Acceleration Threshold;
	// Z: unused;
	// W: unused
	FVector4 m_VSP;

	FMatrix m_BoneSkinningMatrix[AMD_TRESSFX_MAX_NUM_BONES];

#if TRESSFX_COLLISION_CAPSULES
	TStaticArray<FVector4, TRESSFX_MAX_NUM_COLLISION_CAPSULES> m_centerAndRadius0;
	TStaticArray<FVector4, TRESSFX_MAX_NUM_COLLISION_CAPSULES> m_centerAndRadius1;
	FIntVector4 m_numCollisionCapsules;
#endif

	int32 NumVertsPerStrand;
	int32 NumFollowHairsPerGuideHair;

	int32 CPULocalShapeIterations;

	void SetDamping(float d) { m_Shape.X = d; }
	void SetLocalStiffness(float s) { m_Shape.Y = s; }
	void SetGlobalStiffness(float s) { m_Shape.Z = s; }
	void SetGlobalRange(float r) { m_Shape.W = r; }

	void SetGravity(float g) { m_GravTimeTip.X = g; }
	void SetTimeStep(float dt) { m_GravTimeTip.Y = dt; }
	void SetTipSeperation(float ts) { m_GravTimeTip.Z = ts; }

	void SetVelocityShockPropogation(float vsp) { m_VSP.X = vsp; }
	void SetVSPAccelThreshold(float vspAccelThreshold) { m_VSP.Y = vspAccelThreshold; }

	void SetLengthIterations(int32 i) { m_SimInts.X = i; }
	void SetLocalIterations(int32 i) { m_SimInts.Y = i; }
	void SetCollision(bool on) { m_SimInts.Z = on ? 1 : 0; }

	void SetTotalHairs(int32 TotalHairs)
	{
		m_Counts.W = TotalHairs;
	}
	void SetVerticesPerStrand(int32 n)
	{
		m_Counts.X = TRESSFX_SIM_THREAD_GROUP_SIZE / n;
		m_Counts.Z = n;
	}
	void SetFollowHairsPerGuideHair(int32 n) { m_Counts.Y = n; }

	FTressFXSimParameters() {}


	FTressFXSimParameters(int32 InNumVertsPerStrand, int32 InNumFollowHairsPerGuideHair, FTressFXSimulationSettings SimSettings = FTressFXSimulationSettings()) :
		NumVertsPerStrand(InNumVertsPerStrand),
		NumFollowHairsPerGuideHair(InNumFollowHairsPerGuideHair)
	{

	}

	void UpdateSimulationParameters(const FTressFXSimulationSettings& settings, uint32 frame, FTressFXHairObject* HairObject, float LastDeltaTime);

	void SetWind(const FVector& WindDir, float WindMagnitude, int32 FrameNumber);

	FTressFXSimParametersUniformBuffer GetBuffer();

};

#ifndef TRESSFX_STANDALONE_PLUGIN
class TRESSFX_API FTressFXSceneProxy : public ITressFXSceneProxy
#else
class TRESSFX_API FTressFXSceneProxy : public FPrimitiveSceneProxy
#endif
{

public:

	FTressFXSceneProxy(class UPrimitiveComponent* InComponent, FName ResourceName = NAME_None, FTressFXHairObject* InHairObject = nullptr);

	virtual ~FTressFXSceneProxy();

	virtual uint32 GetMemoryFootprint(void) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)const override;
	virtual void CreateRenderThreadResources() override;

	virtual void OnTransformChanged()override;

	virtual SIZE_T GetTypeHash() const override;

	//Returns true if the material has velocity option checked (on by default)
	bool WantsVelocityDraw();

	struct FDynamicRenderData
	{
		ETressFXCollisionType CollisionType;
		FTressFXSimulationSettings TressFXSimulationSettings;
		FTressFXShadeSettings TressFXShadeSettings;
		TStaticArray<FMatrix, AMD_TRESSFX_MAX_NUM_BONES> BoneTransforms;
		FTressFXHairObject* HairObject;
		FTressFXInstanceRenderData* InstanceRenderData;
		FTressFXMeshResources* SDFMeshResources;
		class UMaterialInterface* HairMaterial;
		int32 NumFollowStrandsPerGuide;
		uint32 FrameNumber;
		TStaticArray<FVector4, TRESSFX_MAX_NUM_COLLISION_CAPSULES> CollisionCapsuleCenterAndRadius0;
		TStaticArray<FVector4, TRESSFX_MAX_NUM_COLLISION_CAPSULES> CollisionCapsuleCenterAndRadius1;
		FIntVector4 NumCollisionCapsules;
		class FSkeletalMeshObjectGPUSkin* ParentSkin = nullptr;
		bool bEnableMorphTargets = false;
		float LODCullingFactor;
		float LODScreenSizeThreshold;
		float MinLODRate;
		float LastDeltaTime;

		FDynamicRenderData() {}

		FDynamicRenderData& operator = (const FDynamicRenderData& Other)
		{
			TressFXSimulationSettings = Other.TressFXSimulationSettings;
			TressFXShadeSettings = Other.TressFXShadeSettings;
			BoneTransforms = Other.BoneTransforms;
			CollisionCapsuleCenterAndRadius0 = Other.CollisionCapsuleCenterAndRadius0;
			CollisionCapsuleCenterAndRadius1 = Other.CollisionCapsuleCenterAndRadius1;
			NumCollisionCapsules = Other.NumCollisionCapsules;
			CollisionType = Other.CollisionType;
			ParentSkin = Other.ParentSkin;
			bEnableMorphTargets = Other.bEnableMorphTargets;
			LODCullingFactor = Other.LODCullingFactor;
			LODScreenSizeThreshold = Other.LODScreenSizeThreshold;
			MinLODRate = Other.MinLODRate;
			LastDeltaTime = Other.LastDeltaTime;
			return *this;
		}
	};

	void UpdateDynamicData_RenderThread(FDynamicRenderData* InDynamicData);
	FDynamicRenderData* DynamicData;

	void CopyMorphs(FRHICommandList& RHICmdList);

	void UpdateMorphIndices_RenderThread(const TArray<int32>& MorphIndices);

	bool bInitialized;

	uint32 SimulationFrame;

	class UTressFXComponent* TFXComponent;

	FTressFXHairObject* TressFXHairObject;
	FTressFXInstanceRenderData* InstanceRenderData;

	ETressFXCollisionType CollisionType;
	FTressFXMeshResources* SDFMeshResources;

	FTressFXVertexFactory VertexFactory;

	UMaterialInterface* Material;

	float LODCullingFactor;
	float LODScreenSizeThreshold;
	float MinLODRate;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView *>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;
	virtual FRHIUniformBuffer* GetHairObjectShaderUniformBufferParam();
private:

	FMaterialRelevance MaterialRelevance;

	void CreateMeshBatchForView(
		const FSceneView* View,
		const FSceneViewFamily& ViewFamily,
		FTressFXVertexFactoryUserDataWrapper& UserDataWrapper,
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer,
		FMeshBatch& OutMeshBatch,
		float &OutLodRate
	) const;

protected:

	//** For morph targets*/
	bool bEnableMorphTargets = false;
	uint32 MorphVertexUpdateFrameNumber = 0;
	FReadBuffer MorphIndexBuffer;
	FShaderResourceViewRHIRef MorphVertexBuffer;
public:
	FRWBufferStructured MorphPositionDeltaBuffer;
	bool GetMorphTargetsEnabled() { return bEnableMorphTargets; }

#if TRESSFX_RAYTRACING && RHI_RAYTRACING

	virtual bool IsRayTracingRelevant() const override { return true; }
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override final;
	FRayTracingGeometry RayTracingGeometry;
	FRWBuffer RayTracingDynamicVertexBuffer;
	bool bRaytraceGeometryNeedsRecreate = false;

#endif // RHI_RAYTRACING

};