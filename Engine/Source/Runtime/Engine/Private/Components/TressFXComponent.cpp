
#include "Components/TressFXComponent.h"
#include "Engine/TressFXAsset.h"
#include "Engine/TressFXMesh.h"
#include "TressFX/TressFXSceneProxy.h"
#include "TressFX/TressFXSimulation.h"
#include "Engine/TressFXBoneSkinningAsset.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletalRenderGPUSkin.h"
#include "Components/SkeletalMeshComponent.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Texture2D.h"


DEFINE_LOG_CATEGORY(TressFXComponentLog);

UTressFXComponent::UTressFXComponent(const class FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
}

FPrimitiveSceneProxy* UTressFXComponent::CreateSceneProxy()
{
	return new FTressFXSceneProxy(this);
}

bool UTressFXComponent::ShouldCreateRenderState() const
{
	return Asset != nullptr && Asset->ImportData.IsValid() && Asset->IsValid() && Asset->ImportData->SkinningData.Num();
}

void UTressFXComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MarkRenderDynamicDataDirty();
}

FBoxSphereBounds UTressFXComponent::CalcBounds(const FTransform& LocalToWorld) const
{

	if (Asset && Asset->ImportData.IsValid())
	{
		//IDK why this keeps happening, this is just a bandaid...
		if (!FMath::IsFinite(Asset->ImportData->Bounds.SphereRadius))
		{
			UE_LOG(TressFXComponentLog, Warning, TEXT("Asset bounds contained non-finite number. relcalculating."), *this->GetFName().ToString());
			Asset->ImportData->Bounds = Asset->ImportData->CalcBounds();
		}
		return Asset->ImportData->Bounds.TransformBy(LocalToWorld.ToMatrixWithScale());
	}

	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(50), 100);
}

void UTressFXComponent::PostLoad()
{
	Super::PostLoad();

}
#if WITH_EDITOR
void UTressFXComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if(Name == TEXT("bEnableMorphTargets") && this->bEnableMorphTargets == true)
	{
		this->SetUpMorphMapping();
		this->MarkRenderDynamicDataDirty();
	}
	else if (Name == TEXT("bEnableMorphTargets"))
	{
		MorphIndices.Empty(true);
		CachedSkeletalMeshForMorph = nullptr;
	}
}
#endif

void UTressFXComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();
	ParentSkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());
	SetUpMorphMapping();
	MarkRenderDynamicDataDirty();
}


void UTressFXComponent::GetUsedMaterials(TArray<UMaterialInterface *>& OutMaterials, bool bGetDebugMaterials /* = false */) const
{
	if (HairMaterial)
	{
		OutMaterials.Add(HairMaterial);
	}
}

void UTressFXComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	if (Asset && Asset->ImportData.IsValid() && Asset->IsValid())
	{
		HairObject = ::new FTressFXHairObject(Asset->ImportData.Get());
		BeginInitResource(HairObject);


		if (CVarTFXSDFDisable.GetValueOnAnyThread() == 0 && SDFCollisionMeshAsset && CollisionType == ETressFXCollisionType::TFXCollsion_SDF)
		{
			SDFMeshResources = ::new FTressFXMeshResources(SDFCollisionMeshAsset->MeshData);
			BeginInitResource(SDFMeshResources);
		}
		// Setup mapping
		SetUpMorphMapping();
	}
}

void UTressFXComponent::DestroyRenderState_Concurrent()
{
	if (HairObject)
	{
		FTressFXHairObject* LocalHairObject = this->HairObject;
		
		ENQUEUE_RENDER_COMMAND(CleanupHairObject)(
			[LocalHairObject](FRHICommandListImmediate& RHICmdList)
			{
				LocalHairObject->ReleaseResource();
				delete LocalHairObject;
			}
		);

		HairObject = nullptr;

	}

	if (SDFMeshResources)
	{
		FTressFXMeshResources* LocalSDFResources = this->SDFMeshResources;
		ENQUEUE_RENDER_COMMAND(CleanUpTRessFXSDF)(
			[LocalSDFResources](FRHICommandListImmediate& RHICmdList)
			{
				LocalSDFResources->ReleaseResource();
				delete LocalSDFResources;
			}
		);

		SDFMeshResources = nullptr;
	}

	Super::DestroyRenderState_Concurrent();
}

void UTressFXComponent::SetUpMorphMapping()
{
	if (!bEnableMorphTargets || !ParentSkeletalMeshComponent || !ParentSkeletalMeshComponent->SkeletalMesh || !Asset || !Asset->IsValid())
	{
		return;
	}

	UE_LOG(TressFXComponentLog, Warning, TEXT("SetupMorphMapping Called"), *this->GetFName().ToString());
	// Setup morph index mapping
	do
	{
		if (ParentSkeletalMeshComponent->SkeletalMesh->MorphTargets.Num() <= 0)
		{
			MorphIndices.Empty(true);
			break;
		}

		// Check if parent skeletal mesh has changed. 
		if (!bAutoRemapMorphTarget)
		{
			if (CachedSkeletalMeshForMorph == ParentSkeletalMeshComponent->SkeletalMesh)
			{
				break;
			}
			CachedSkeletalMeshForMorph = ParentSkeletalMeshComponent->SkeletalMesh;
		}

		// Get vertices of parent skeletal mesh
		const FPositionVertexBuffer& ParentMeshVertexBuffer = ParentSkeletalMeshComponent->SkeletalMesh->GetResourceForRendering()->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;

		TArray<FVector> ParentMeshVertices;
		ParentMeshVertices.SetNumUninitialized(ParentMeshVertexBuffer.GetNumVertices());

		for (int32 VertexIdx = 0; VertexIdx < ParentMeshVertices.Num(); ++VertexIdx)
		{
			ParentMeshVertices[VertexIdx] = ParentMeshVertexBuffer.VertexPosition(VertexIdx);
		}

		const int32 GuideNum = this->Asset->ImportData->NumGuideStrands;
		TArray<FVector> GuideRootVertices = this->Asset->ImportData->GetRootPositions();

		// Find closest skeletal mesh vertex for each vertex of HairWorks growth mesh
		const FTransform RelativeTransform = GetRelativeTransform();

		MorphIndices.SetNumUninitialized(GuideNum, true);

		for (int32 GuideIdx = 0; GuideIdx < GuideNum; ++GuideIdx)
		{
			const FVector GuideRootVertex = RelativeTransform.TransformPosition(GuideRootVertices[GuideIdx]);
			float ClosestSqrDist = TNumericLimits<float>::Max();
			int32 ClosestVertexIdx = 0;

			for (int32 VertexIdx = 0; VertexIdx < ParentMeshVertices.Num(); ++VertexIdx)
			{
				const float SqrDist = FVector::DistSquared(GuideRootVertex, ParentMeshVertices[VertexIdx]);
				if (SqrDist < ClosestSqrDist)
				{
					ClosestSqrDist = SqrDist;
					ClosestVertexIdx = VertexIdx;
				}
			}

			MorphIndices[GuideIdx] = ClosestVertexIdx;
		}

		// Propagate to all instances in editor
#if WITH_EDITOR
		do
		{
			if (bAutoRemapMorphTarget)
			{
				break;
			}

			auto* Archetype = GetArchetype();
			if (Archetype == nullptr || Archetype->HasAllFlags(RF_ClassDefaultObject))
			{
				break;
			}

			TArray<UObject*> Instances;
			Archetype->GetArchetypeInstances(Instances);

			Instances.Add(Archetype);

			for (auto* Instance : Instances)
			{
				auto* TFXComp = CastChecked<UTressFXComponent>(Instance);
				TFXComp->CachedSkeletalMeshForMorph = CachedSkeletalMeshForMorph;
				TFXComp->MorphIndices = MorphIndices;
				TFXComp->Modify();
			}
		} while (false);
#endif
	} while (false);

	// Update morph indices to scene proxy
	if (SceneProxy)
	{
		const auto& LocalMorphIndices = MorphIndices;
		auto& TFXSceneProxy = static_cast<FTressFXSceneProxy&>(*SceneProxy);
		auto UpdateMorphIndices = [&TFXSceneProxy, LocalMorphIndices]()
		{			
			TFXSceneProxy.UpdateMorphIndices_RenderThread(LocalMorphIndices);
		};

		ENQUEUE_RENDER_COMMAND(UpdateHairMorphs)(
			[UpdateMorphIndices](FRHICommandListImmediate& RHICmdList)
			{
				UpdateMorphIndices();
			}
		);
	}
	else
	{
		UE_LOG(TressFXComponentLog, Log, TEXT("No scene proxy found, cannot call UpdateMorphIndices_RenderThread"), *this->GetFName().ToString());
	}
}

void UTressFXComponent::SendRenderDynamicData_Concurrent()
{
	if (!SceneProxy || !ParentSkeletalMeshComponent || !Asset->IsValid())
	{
		return;
	}

	Super::SendRenderDynamicData_Concurrent();

	UWorld* World = GetWorld();
	FVector OutWindDirection(0, 0, 0);
	float OutWindSpeed = 0.f;

	if (World && World->Scene)
	{
		FVector Position = GetComponentToWorld().GetTranslation();
		float WindMinGust;
		float WindMaxGust;
		World->Scene->GetWindParameters_GameThread(Position, OutWindDirection, OutWindSpeed, WindMinGust, WindMaxGust);
		//OutWindSpeed *= -1; // not sure why this was here...
		//adjust windspeed, tressfx needs much stronger wind to have any effect, 1000 seems to be a good number for now
		OutWindSpeed *= 1000;
	}

	TSharedRef<FTressFXSceneProxy::FDynamicRenderData> DynamicRenderData(new FTressFXSceneProxy::FDynamicRenderData);

	 //Update morph data
	do
	{
		if (!bEnableMorphTargets || MorphIndices.Num() <= 0 || ParentSkeletalMeshComponent == nullptr || ParentSkeletalMeshComponent->MeshObject == nullptr)
		{
			break;
		}

		if (ParentSkeletalMeshComponent->MeshObject->IsCPUSkinned())
		{
			break;
		}

		DynamicRenderData->ParentSkin = static_cast<FSkeletalMeshObjectGPUSkin*>(ParentSkeletalMeshComponent->MeshObject);
	} while (false);

	DynamicRenderData->bEnableMorphTargets = this->bEnableMorphTargets;
	DynamicRenderData->TressFXSimulationSettings = TressFXSimulationSettings;
	DynamicRenderData->TressFXSimulationSettings.WindDirection.X = OutWindDirection.X;
	DynamicRenderData->TressFXSimulationSettings.WindDirection.Y = OutWindDirection.Y;
	DynamicRenderData->TressFXSimulationSettings.WindDirection.Z = OutWindDirection.Z;
	DynamicRenderData->TressFXSimulationSettings.WindMagnitude = OutWindSpeed;
	DynamicRenderData->TressFXSimulationSettings.TipSeparation = Asset->TipSeparationFactor;
	DynamicRenderData->TressFXShadeSettings = ShadeSettings;
	DynamicRenderData->HairObject = HairObject;
	DynamicRenderData->SDFMeshResources = SDFMeshResources;
	DynamicRenderData->NumFollowStrandsPerGuide = Asset->NumFollowStrandsPerGuide;
	DynamicRenderData->HairMaterial = HairMaterial;
	DynamicRenderData->CollisionType = CollisionType.GetValue();

	if (!DynamicRenderData->HairMaterial)
	{
		DynamicRenderData->HairMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	int32 NumCapsules = 0;
	if (ParentSkeletalMeshComponent && CollisionType == ETressFXCollisionType::TFXCollsion_Capsule)
	{
		TArray<USceneComponent*> ChildrenComponents;
		ParentSkeletalMeshComponent->GetChildrenComponents(false, ChildrenComponents);

		for (int32 i = 0; i < ChildrenComponents.Num(); ++i)
		{
			UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(ChildrenComponents[i]);

			if (CapsuleComponent)
			{
				FTransform CapsuleTransform = CapsuleComponent->GetComponentTransform();
				float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
				float CapsuleRad = CapsuleComponent->GetScaledCapsuleRadius();
				FVector CapsuleZAxis = CapsuleTransform.GetScaledAxis(EAxis::Z)*CapsuleHalfHeight;

				DynamicRenderData->CollisionCapsuleCenterAndRadius0[NumCapsules] = FVector4(CapsuleTransform.GetLocation() + CapsuleZAxis, CapsuleRad);
				DynamicRenderData->CollisionCapsuleCenterAndRadius1[NumCapsules] = FVector4(CapsuleTransform.GetLocation() - CapsuleZAxis, CapsuleRad);
				NumCapsules++;
			}
		}
	}
	DynamicRenderData->NumCollisionCapsules = FIntVector4(NumCapsules, NumCapsules, NumCapsules, NumCapsules);

#if WITH_EDITOR
	// for debugging
	TArray<int32> UsedBoneIndexes;
#endif

	for (auto It = Asset->ImportData->BoneNameIndexMap.CreateIterator(); It; ++It)
	{
		FName TFXBoneName = It.Value();
		int32 TFXIndex = It.Key();

		FTransform BoneTransform = FTransform::Identity;
		if (ParentSkeletalMeshComponent && ParentSkeletalMeshComponent->GetBoneIndex(TFXBoneName) != INDEX_NONE)
		{
			// find the bone index to use, rather than relying on the one that was imported with the TFX file
			// this way, we can use different skeletons with the same bones names and have it still work
			int32 SkelBoneIndex = ParentSkeletalMeshComponent->GetBoneIndex(TFXBoneName);
			FMatrix RefBase = ParentSkeletalMeshComponent->SkeletalMesh->RefBasesInvMatrix[SkelBoneIndex];
			FMatrix ParentSkel = ParentSkeletalMeshComponent->GetBoneMatrix(SkelBoneIndex);

			if (RefBase.ContainsNaN())
			{
				UE_LOG(TressFXComponentLog, Log, TEXT("Found NaN in RefBase Bone Matrix:  %s"), *this->GetFName().ToString());
			}
			if (ParentSkel.ContainsNaN())
			{
				UE_LOG(TressFXComponentLog, Log, TEXT("Found NaN in ParentSkel Bone Matrix:  %s"), *this->GetFName().ToString());
			}

			FMatrix Result = (RefBase * ParentSkel);

			if (Result.ContainsNaN())
			{
				UE_LOG(TressFXComponentLog, Log, TEXT("Found NaN in Result Bone Matrix:  %s"), *this->GetFName().ToString());
			}		

			if(SkelBoneIndex < AMD_TRESSFX_MAX_NUM_BONES)
			{
				DynamicRenderData->BoneTransforms[SkelBoneIndex] = Result;
#if WITH_EDITOR
				UsedBoneIndexes.Add(SkelBoneIndex);
#endif
			}		
		}
		else
		{
			DynamicRenderData->BoneTransforms[TFXIndex] = FMatrix::Identity;
		}
	}

#if WITH_EDITOR
	//useful for debugging
	for(int32 i = 0; i < AMD_TRESSFX_MAX_NUM_BONES; i++)
	{
		if (!UsedBoneIndexes.Contains(i))
		{
			DynamicRenderData->BoneTransforms[i] = FMatrix(FPlane(0, 0, 0, 0), FPlane(0, 0, 0, 0), FPlane(0, 0, 0, 0), FPlane(0, 0, 0, 0));
		}
	}
#endif
	DynamicRenderData->BoneSkinningData = Asset->ImportData->SkinningData;

	FTressFXSceneProxy* LocalTFXProxy = static_cast<FTressFXSceneProxy*>(SceneProxy);

	ENQUEUE_RENDER_COMMAND(TRessFXSimulateCommand)(
		[LocalTFXProxy, DynamicRenderData](FRHICommandListImmediate& RHICmdList)
		{
			LocalTFXProxy->UpdateDynamicData_RenderThread(*DynamicRenderData);
			LocalTFXProxy->CopyMorphs(RHICmdList);
			SimulateTressFX(RHICmdList, LocalTFXProxy, DynamicRenderData->TressFXSimulationSettings.LengthConstraintsIterations);
		}
	);
}