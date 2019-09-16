
#include "TressFXComponent.h"
#include "TressFXAsset.h"
#include "TressFXMesh.h"
#include "TressFXSceneProxy.h"
#include "TressFXSimulation.h"
#include "TressFXBoneSkinningAsset.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Private/SkeletalRenderGPUSkin.h"
#include "Components/SkeletalMeshComponent.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Runtime/Engine/Classes/Animation/AnimInstance.h"
#include "Engine/Texture2D.h"
#include "TressFXUtils.h"


DEFINE_LOG_CATEGORY(TressFXComponentLog);

UTressFXComponent::UTressFXComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bCastShadowAsTwoSided = true;
	CastShadow = true;
	bCastInsetShadow = true;
	bCastShadowAsTwoSided = true;
	LODCullingFactor = 1.0f;
	LODScreenSizeThreshold = 2.5f;
	MinLODRate = 0.1f;
}

UTressFXComponent::~UTressFXComponent()
{
	ReleaseInstanceRenderData();
}

FPrimitiveSceneProxy* UTressFXComponent::CreateSceneProxy()
{
	return new FTressFXSceneProxy(this, Asset ? FName(*Asset->GetName()) : FName(*this->GetName()) );
}

bool UTressFXComponent::ShouldCreateRenderState() const
{
	return Asset != nullptr && Asset->ImportData.IsValid() && Asset->IsValid() && Asset->ImportData->SkinningData.Num();
}

void UTressFXComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCachedTransforms();

	MarkRenderDynamicDataDirty();
	LastDeltaTime = DeltaTime;
}

void UTressFXComponent::UpdateCachedTransforms()
{
	FTransform CurrentRelativeTransform = this->GetRelativeTransform();

	CachedRelativeTransform = CurrentRelativeTransform;
	CachedRelativeTransformMatrix = CachedRelativeTransform.ToMatrixWithScale();
	
}

FBoxSphereBounds UTressFXComponent::CalcBounds(const FTransform& LocalToWorld) const
{

	if (Asset && Asset->ImportData.IsValid())
	{
		//IDK why this keeps happening, this is just a bandaid...
		if (!FMath::IsFinite(Asset->ImportData->Bounds.SphereRadius))
		{
			UE_LOG(TressFXComponentLog, Error, TEXT("Asset bounds contained non-finite number. relcalculating."), *this->GetFName().ToString());
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

	if(Name == GET_MEMBER_NAME_CHECKED(UTressFXComponent, bEnableMorphTargets) && this->bEnableMorphTargets == true)
	{
		UpdateCachedTransforms();
		this->SetUpMorphMapping();
		this->MarkRenderDynamicDataDirty();
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UTressFXComponent, bEnableMorphTargets))
	{
		MorphIndices.Empty(true);
		CachedSkeletalMeshForMorph = nullptr;
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UTressFXComponent, bSupportVirtualBones))
	{
		UpdateCachedTransforms();
		this->MarkRenderDynamicDataDirty();
	}

	if (Name == GET_MEMBER_NAME_CHECKED(UTressFXComponent, Asset) || Name == GET_MEMBER_NAME_CHECKED(UTressFXComponent, SDFCollisionMeshAsset))
	{
		MarkInstanceRenderDataDirty();
	}
}
#endif

void UTressFXComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();
	ParentSkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());
	UpdateCachedTransforms();
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

void UTressFXComponent::MarkInstanceRenderDataDirty()
{
	bInstanceDataDirty = true;
}

void UTressFXComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	if (Asset && Asset->ImportData.IsValid() && Asset->IsValid())
	{
		Asset->GetOrCreateRenderData();

		if (bInstanceDataDirty || InstanceRenderData == nullptr)
		{
			ReleaseInstanceRenderData();
			InstanceRenderData = ::new FTressFXInstanceRenderData(Asset->ImportData.Get());
			BeginInitResource(InstanceRenderData);
		}

		if (CVarTFXSDFDisable.GetValueOnAnyThread() == 0 && SDFCollisionMeshAsset && CollisionType == ETressFXCollisionType::TFXCollsion_SDF)
		{
			SDFMeshResources = ::new FTressFXMeshResources(SDFCollisionMeshAsset->MeshData);
			BeginInitResource(SDFMeshResources);
		}
		// Setup mapping
		SetUpMorphMapping();
	}
}

void UTressFXComponent::ReleaseInstanceRenderData()
{
	if (InstanceRenderData)
	{
		auto LocalInstanceRenderData = InstanceRenderData;
		InstanceRenderData = nullptr;

		ENQUEUE_RENDER_COMMAND(CleanupHairObject)(
			[LocalInstanceRenderData](FRHICommandListImmediate& RHICmdList)
		{
			LocalInstanceRenderData->ReleaseResource();
			delete LocalInstanceRenderData;
		}
		);
	}
}

void UTressFXComponent::DestroyRenderState_Concurrent()
{
	if (bInstanceDataDirty)
	{
		ReleaseInstanceRenderData();
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

#ifndef TRESSFX_STANDALONE_PLUGIN

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

		// Find closest skeletal mesh vertex for each vertex of mesh
		const FTransform RelativeTransform = CachedRelativeTransform;

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
				UTressFXComponent* TFXComp = CastChecked<UTressFXComponent>(Instance);
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
		UE_LOG(TressFXComponentLog, Warning, TEXT("No scene proxy found, cannot call UpdateMorphIndices_RenderThread"), *this->GetFName().ToString());
	}
#endif
}

void UTressFXComponent::SendRenderDynamicData_Concurrent()
{
	if (!SceneProxy || !ParentSkeletalMeshComponent || !Asset->IsValid())
	{
		return;
	}

	Super::SendRenderDynamicData_Concurrent();
	// this is always invoke before rendering, so simulation is done before every base pass
	// bone skinning is done?
	RunSimulation();
}

void UTressFXComponent::RunSimulation()
{
	if (!SceneProxy || !ParentSkeletalMeshComponent || !Asset->IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	FVector OutWindDirection(0, 0, 0);
	float OutWindSpeed = 0.f;

	if (World && World->Scene)
	{
		FVector Position = GetComponentToWorld().GetTranslation();
		float WindMinGust;
		float WindMaxGust;
		World->Scene->GetWindParameters_GameThread(Position, OutWindDirection, OutWindSpeed, WindMinGust, WindMaxGust);
		//adjust windspeed, tressfx seems to needs much stronger wind to have any effect
		OutWindSpeed *= TressFXSimulationSettings.WindMagnitude;
	}

	TSharedRef<FTressFXSceneProxy::FDynamicRenderData> DynamicRenderData(new FTressFXSceneProxy::FDynamicRenderData);

	//Update morph data
	do
	{
		if (!bEnableMorphTargets || MorphIndices.Num() <= 0 || ParentSkeletalMeshComponent == nullptr || ParentSkeletalMeshComponent->MeshObject == nullptr)
		{
			break;
		}
#ifndef TRESSFX_STANDALONE_PLUGIN
		if (ParentSkeletalMeshComponent->MeshObject->IsCPUSkinned())
		{
			break;
		}

		DynamicRenderData->ParentSkin = static_cast<FSkeletalMeshObjectGPUSkin*>(ParentSkeletalMeshComponent->MeshObject);
#else
		DynamicRenderData->ParentSkin = nullptr;
#endif
	} while (false);

	DynamicRenderData->bEnableMorphTargets = this->bEnableMorphTargets;
	DynamicRenderData->TressFXSimulationSettings = TressFXSimulationSettings;
	DynamicRenderData->TressFXSimulationSettings.WindDirection.X = OutWindDirection.X;
	DynamicRenderData->TressFXSimulationSettings.WindDirection.Y = OutWindDirection.Y;
	DynamicRenderData->TressFXSimulationSettings.WindDirection.Z = OutWindDirection.Z;
	DynamicRenderData->TressFXSimulationSettings.WindMagnitude = OutWindSpeed;
	DynamicRenderData->TressFXSimulationSettings.TipSeparation = Asset->TipSeparationFactor;
	DynamicRenderData->TressFXShadeSettings = ShadeSettings;
	DynamicRenderData->HairObject = Asset->GetOrCreateRenderData();
	DynamicRenderData->InstanceRenderData = InstanceRenderData;
	DynamicRenderData->SDFMeshResources = SDFMeshResources;
	DynamicRenderData->NumFollowStrandsPerGuide = Asset->NumFollowStrandsPerGuide;
	DynamicRenderData->HairMaterial = HairMaterial;
	DynamicRenderData->CollisionType = CollisionType.GetValue();
	DynamicRenderData->LODCullingFactor = LODCullingFactor;
	DynamicRenderData->LODScreenSizeThreshold = LODScreenSizeThreshold;
	DynamicRenderData->MinLODRate = MinLODRate;

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
	else if (CollisionType == ETressFXCollisionType::TFXCollsion_PhysicsAsset)
	{
		if (ParentSkeletalMeshComponent->Bodies.Num() > 0)
		{
			for (FBodyInstance* BI : ParentSkeletalMeshComponent->Bodies)
			{				
				const TWeakObjectPtr<UBodySetup> BodySetup = BI->BodySetup;
				
				const bool bCollisionEnabled = (BodySetup.IsValid() && BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Enabled);

				//we only support sphyls for now
				if (bCollisionEnabled && BodySetup->AggGeom.SphylElems.Num() > 0)
				{	
					FRigidBodyState BodyState;
					FVector BodyPos;
					FQuat BodyRot;
					FVector BodyScale;

					if (BI->GetRigidBodyState(BodyState))
					{
						//its simulating physics
						BodyRot = BodyState.Quaternion;
						BodyPos = BodyState.Position;
					}
					else 
					{
						const FTransform Trans = BI->GetUnrealWorldTransform();
						BodyRot = Trans.GetRotation();
						BodyPos = Trans.GetTranslation();						
					}

					//to correct scaling on meshes with funky scaling
					const FVector ScaleMultiplier = FVector::OneVector / BI->Scale3D;
					
					// NOTE: BodyPos and BodyRot are the pos and rot of the body setup.
					// we need to offset it by the sphyl relative transforms now			

					for(const FKSphylElem& Sphyl : BodySetup->AggGeom.SphylElems)
					{
						if (NumCapsules >= TRESSFX_MAX_NUM_COLLISION_CAPSULES)
						{
							break;
						}
						// i dont want to add custom properties to collision assets/bodies to mark as use for tressfx
						// i would have to modify serialization and i dont want to do that.
						// so just doing this for now
						if (!Sphyl.GetName().ToString().EndsWith("tressfx", ESearchCase::CaseSensitive)) 
						{
							continue;
						}

						FTransform CapsuleTransform = Sphyl.GetTransform();
						FVector CapsuleRelativeLoc = Sphyl.Center;
						CapsuleRelativeLoc = BodyRot.RotateVector(CapsuleRelativeLoc);

						// scale the body to correct scale before adding capsule relative loc
						BodyPos *= ScaleMultiplier;
						BodyPos += CapsuleRelativeLoc;
						// re-apply scaling
						BodyPos *= BI->Scale3D;

						float CapsuleHalfHeight = Sphyl.GetScaledHalfLength(BI->Scale3D);
						float CapsuleRad = Sphyl.GetScaledRadius(BI->Scale3D);
						FVector CapsuleZAxis = CapsuleTransform.GetScaledAxis(EAxis::Z) * CapsuleHalfHeight;

						// apply rotation of body instance to sphyl
						CapsuleZAxis = BodyRot.RotateVector(CapsuleZAxis);

						DynamicRenderData->CollisionCapsuleCenterAndRadius0[NumCapsules] = FVector4(BodyPos + CapsuleZAxis, CapsuleRad);
						DynamicRenderData->CollisionCapsuleCenterAndRadius1[NumCapsules] = FVector4(BodyPos - CapsuleZAxis, CapsuleRad);
						NumCapsules++;
					}
				}
			}
		}
	}

#if WITH_EDITOR
	// for debugging
	TArray<int32> UsedBoneIndexes;
#endif

	for (auto It = Asset->ImportData->BoneNameIndexMap.CreateIterator(); It; ++It)
	{
		FName TFXBoneName = It.Value();
		int32 TFXIndex = It.Key();

		// find the bone index to use, rather than relying on the one that was imported with the TFX file
		// this way, we can use different skeletons with the same bones names and have it still work
		const USkeletalMesh* ParentSkelMesh = ParentSkeletalMeshComponent ? ParentSkeletalMeshComponent->SkeletalMesh : nullptr;
		
		//TODO: maybe cache this instead for faster lookups?
		const int32 SkelBoneIndex = ParentSkelMesh ? FTressFXUtils::FindEngineBoneIndex(ParentSkelMesh, TFXBoneName, bSupportVirtualBones) : INDEX_NONE;

		if (SkelBoneIndex != INDEX_NONE)
		{

			FMatrix RefBase = ParentSkeletalMeshComponent->SkeletalMesh->RefBasesInvMatrix[SkelBoneIndex];
			FMatrix ParentSkel = ParentSkeletalMeshComponent->GetBoneMatrix(SkelBoneIndex);

			if (RefBase.ContainsNaN())
			{
				UE_LOG(TressFXComponentLog, Warning, TEXT("Found NaN in RefBase Bone Matrix:  %s"), *this->GetFName().ToString());
			}
			if (ParentSkel.ContainsNaN())
			{
				UE_LOG(TressFXComponentLog, Warning, TEXT("Found NaN in ParentSkel Bone Matrix:  %s"), *this->GetFName().ToString());
			}

			FMatrix Result = (RefBase * ParentSkel);
			Result = CachedRelativeTransformMatrix * Result;
			if (Result.ContainsNaN())
			{
				UE_LOG(TressFXComponentLog, Warning, TEXT("Found NaN in Result Bone Matrix:  %s"), *this->GetFName().ToString());
			}

			if (SkelBoneIndex < AMD_TRESSFX_MAX_NUM_BONES)
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
	DynamicRenderData->NumCollisionCapsules = FIntVector4(NumCapsules, NumCapsules, NumCapsules, NumCapsules);

	FTressFXSceneProxy* LocalTFXProxy = static_cast<FTressFXSceneProxy*>(SceneProxy);

	if (LocalTFXProxy)
	{
		ENQUEUE_RENDER_COMMAND(TRessFXSimulateCommand)(
			[LocalTFXProxy, DynamicRenderData](FRHICommandListImmediate& RHICmdList)
			{
				LocalTFXProxy->UpdateDynamicData_RenderThread(&DynamicRenderData.Get());
				LocalTFXProxy->CopyMorphs(RHICmdList);
				SimulateTressFX(RHICmdList, LocalTFXProxy, DynamicRenderData->TressFXSimulationSettings.LengthConstraintsIterations);
			}
		);
	}
}

FTressFXShadeSettings::FTressFXShadeSettings()
{
	//sensible defaults
	FiberRadius = 0.01;
	FiberSpacing = 0.1;
	HairThickness = 0.2f;
	RootTangentBlending = 0.0f;
	DiffuseBlend = 0.40f;
	SelfShadowStrength = 0.05f;
}