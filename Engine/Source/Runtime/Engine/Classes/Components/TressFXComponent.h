
#pragma once
#include "CoreMinimal.h"
#include "TressFX/TressFXTypes.h"
#include "UObject/ObjectMacros.h"
#include "PrimitiveComponent.h"
#include "TressFXComponent.generated.h"

#pragma warning( push )
//no
#pragma warning( disable : 5038)

DECLARE_LOG_CATEGORY_EXTERN(TressFXComponentLog, Log, All);

USTRUCT(BlueprintType)
struct FTressFXSimulationSettings
{
	GENERATED_USTRUCT_BODY()

public:

	FTressFXSimulationSettings()
		: VSPCoefficient(0.8f)
		, VSPAccelerationThreshold(1.4f)
		, LocalConstraintStiffness(0.9f)
		, LocalContraintsIterations(2)
		, GlobalConstraintStiffness(0.0f)
		, GlobalShapeRange(0.0f)
		, LengthConstraintsIterations(2)
		, Damping(0.08f)
		, GravityMagnitude(10.0)
		, TipSeparation(0.0f)
		, WindMagnitude(0)
		, WindDirection(FVector(1, 0, 0))
	{
	}

	/*
	VSP(Velocity Shock Propagation) makes the root vertex velocity propagate through the rest of vertices in the hair strand.
	The amount of propagation is controlled by a value ranged from 0 to 1.
	If it is 0, then there will be no velocity propagation.If it is 1, then the full velocity of the root vertex will be passed to the rest of vertices.
	In this case, hair would act as if it is rigid and the rest of simulation would not affect the result.
	So in case you want to animate the hair or fur but do not want to simulate it, then VPS is a perfect solution.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float VSPCoefficient;

	/*
	VSP acceleration threshold makes VSP value increase when the pseudo - acceleration of root vertex is greater than it.
	This is particularly effective when the character makes a sudden movement because VSP
	value becomes to the highest amount when the pseudo - acceleration reaches the user input value.
	The acceleration is pseudo because it is not divided by time.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float VSPAccelerationThreshold;

	/*
	Local and Global Stiffness makes each strand stiffer – meaning that if the hair started out straight, it will try to keep that shape.
	If it was curved, it will try to keep the curve.
	The difference between the two is that global stiffness tries to move the hair back to where it was originally in a global way
	– so if there is something pushing a straight hair in the middle, the tip will try to go back around the ball to where it started, giving it a curved shape.
	The local stiffness only looks at the shape locally, so if that same straight hair is pushed in the middle, the hair will get out of the way of the ball, but try to stay straight, instead of curving around it.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float LocalConstraintStiffness;

	/*
	Local and Global Stiffness makes each strand stiffer – meaning that if the hair started out straight, it will try to keep that shape.
	If it was curved, it will try to keep the curve.
	The difference between the two is that global stiffness tries to move the hair back to where it was originally in a global way
	– so if there is something pushing a straight hair in the middle, the tip will try to go back around the ball to where it started, giving it a curved shape.
	The local stiffness only looks at the shape locally, so if that same straight hair is pushed in the middle, the hair will get out of the way of the ball, but try to stay straight, instead of curving around it.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float GlobalConstraintStiffness;

	/*
	Local Shape Constraint Iterations allocates more simulation time to keeping the hair shape.
	It can make the hair seem stiffer, but also greatly increases simulation time.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	int32   LocalContraintsIterations;

	/*
	Global Shape Range controls how much of each hair strand is affected by the global shape stiffness.
	If the value is 0, then the stiffness is off (as if it also has value of 0).
	If it is 1, then it affects the whole hair strand, and the tip of the hair will try to get back to where it was as described above.
	If the value is 0.5, then only the first half of the hair strand (from the root) tries to get back to its original position,
	so the tip will not try to get back to its original position in the previous example.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float GlobalShapeRange;

	//Length Constraint Iterations allocates more simulation time to keeping the hair the right length.Try to keep this number as low as possible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX", meta = (UIMin = 1, ClampMin = 1, UIMax = 10, ClampMax = 10))
	int32 LengthConstraintsIterations;

	//Damping smooths out motion of the hair.It also slows down the hair movement, making it more like hair under water, for example.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float Damping;

	// Gravity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float GravityMagnitude;

	// tip separation for follow hair from its guide
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float TipSeparation;

	// Wind Magnitude allows you to see the effect of wind on the hair
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float WindMagnitude;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	FVector WindDirection;

};


USTRUCT(BlueprintType)
struct FTressFXShadeSettings
{

	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float HairShadowAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float FiberRadius;

	/* the assumed, average, world space distance between hair fibers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
	float FiberSpacing;

	//Global Geometric thickness ratio of the hair strand meshes (0.0 -> 1.0). Kind of redundant with fiber radius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float HairThickness;

	FTressFXShadeSettings()
	{
		//sensible defaults
		HairShadowAlpha = 0.004;
		FiberRadius = 0.25;
		FiberSpacing = 0.1;
		HairThickness = 0.2f;
	}


};

/**
 *TressFX Component
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Rendering, Common), hidecategories = (Object, Activation, "Components|Activation"), ShowCategories = (Mobility), editinlinenew, meta = (BlueprintSpawnableComponent))
class UTressFXComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:

	/** Morph remapping */
	UPROPERTY()
	TArray<int32> MorphIndices;

	/** do remapping for morph target only when parent skeletal mesh is changed. */
	UPROPERTY()
	USkeletalMesh* CachedSkeletalMeshForMorph;

	UPROPERTY(EditDefaultsOnly, Category = "TressFX")
		bool bEnableMorphTargets = false;

	/** Morphs require a remapping process to support morph target of skeletal mesh. This process would be slow when vertex number is very large, and cause long halt in editor. If this option is on, remapping happens when any edit occurs. If this option is off, remapping happens only when the parent skeletal mesh of this component changes. If you want to do remapping once when you need, just turn it on and then off. */
	UPROPERTY(EditDefaultsOnly, Category = "TressFX")
		bool bAutoRemapMorphTarget = false;

	/**
	 * TressFX Asset
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
		class UTressFXAsset* Asset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
		class UTressFXMesh* SDFCollisionMeshAsset;

	// Collision. Signed Distance Field requries a valid SDFCollisionMeshAsset to be set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
		TEnumAsByte<ETressFXCollisionType> CollisionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
		class UMaterialInterface* HairMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
		FTressFXSimulationSettings TressFXSimulationSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TressFX")
		FTressFXShadeSettings ShadeSettings;

public:

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual bool ShouldCreateRenderState() const override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	virtual void PostLoad() override;
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif

	virtual void OnAttachmentChanged() override;

	friend class FTressFXSceneProxy;

	TSharedPtr<class FTressFXRuntimeData> TressFXRuntimeData;

public:

	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials /* = false */) const override;

protected:


	virtual void CreateRenderState_Concurrent() override;

	virtual void DestroyRenderState_Concurrent() override;

	virtual void SendRenderDynamicData_Concurrent() override;

private:

	class USkeletalMeshComponent* ParentSkeletalMeshComponent;

	void SetUpMorphMapping();

public:

	class FTressFXHairObject* HairObject;

	class FTressFXMeshResources* SDFMeshResources;

};
#pragma warning( pop ) 