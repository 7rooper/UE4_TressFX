// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Components/BillboardComponent.h"

#include "GeometryCollectionDebugDrawActor.generated.h"

class AChaosSolverActor;
class AGeometryCollectionActor;
class UGeometryCollectionComponent;
template<class InElementType> class TManagedArray;
template<class T, int d> class TGeometryCollectionParticlesData;
template<class T> class TAutoConsoleVariable;
typedef TGeometryCollectionParticlesData<float, 3> FGeometryCollectionParticlesData;

/**
* EGeometryCollectionDebugDrawActorHideGeometry
*   Visibility enum.
*/
UENUM()
enum class EGeometryCollectionDebugDrawActorHideGeometry : uint8
{
	// Do not hide any geometry.
	HideNone,
	// Hide the geometry associated with rigid bodies that are selected for collision volume visualization.
	HideWithCollision,
	// Hide the geometry associated with the selected rigid bodies.
	HideSelected,
	// Hide the entire geometry collection associated with the selected rigid bodies.
	HideWholeCollection,
	// Hide all geometry collections.
	HideAll
};

/**
* FGeometryCollectionDebugDrawActorSelectedRigidBody
*   Structure used to select a rigid body id with a picking tool through a detail customization.
*/
USTRUCT()
struct FGeometryCollectionDebugDrawActorSelectedRigidBody
{
	GENERATED_USTRUCT_BODY()

	explicit FGeometryCollectionDebugDrawActorSelectedRigidBody(int32 InId = -1) : Id(InId), Solver(nullptr), GeometryCollection(nullptr) {}
	
	/** Id of the selected rigid body whose to visualize debug informations. Use -1 to visualize all Geometry Collections. */
	UPROPERTY(EditAnywhere, Category = "Selected Rigid Body", meta = (ClampMin="-1"))
	int32 Id;

	/** Chaos RBD Solver. Will use the world's default solver actor if null. */
	UPROPERTY(EditAnywhere, Category = "Selected Rigid Body")
	AChaosSolverActor* Solver;

	/** Currently selected geometry collection. */
	UPROPERTY(VisibleAnywhere, Category = "Selected Rigid Body")
	AGeometryCollectionActor* GeometryCollection;

	/** Return the name of selected solver, or "None" if none is selected. */
	FString GetSolverName() const;
};

/**
* AGeometryCollectionDebugDrawActor
*   An actor representing the collection of data necessary to visualize the 
*   geometry collections' debug informations.
*   Only one actor is to be used in the world, and should be automatically 
*   spawned by any GeometryDebugDrawComponent that needs it.
*/
UCLASS(HideCategories = ("Rendering", "Replication", "Input", "Actor", "Collision", "LOD", "Cooking"))
class GEOMETRYCOLLECTIONENGINE_API AGeometryCollectionDebugDrawActor : public AActor
{
	GENERATED_UCLASS_BODY()
public:

	/** Picking tool used to select a rigid body id. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw")
	FGeometryCollectionDebugDrawActorSelectedRigidBody SelectedRigidBody;

	/** Show debug visualization for the rest of the geometry collection related to the current rigid body id selection. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw")
	bool bDebugDrawWholeCollection;

	/** Show debug visualization for the top level node rather than the bottom leaf nodes of a cluster's hierarchy. * Only affects Clustering and Geometry visualization.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug Draw")
	bool bDebugDrawHierarchy;

	/** Show debug visualization for all clustered children associated to the current rigid body id selection. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw")
	bool bDebugDrawClustering;

	/** Geometry visibility setting. Select the part of the geometry to hide in order to better visualize the debug information. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw")
	EGeometryCollectionDebugDrawActorHideGeometry HideGeometry;

	/** Display the selected rigid body's id. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bShowRigidBodyId;

	/** Show the selected rigid body's collision volume. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bShowRigidBodyCollision;

	/** Show the selected rigid body's collision volume at the origin, in local space. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bCollisionAtOrigin;

	/** Show the selected rigid body's transform. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bShowRigidBodyTransform;

	/** Show the selected rigid body's inertia tensor box. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bShowRigidBodyInertia;

	/** Show the selected rigid body's linear and angular velocity. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bShowRigidBodyVelocity;

	/** Show the selected rigid body's applied force and torque. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bShowRigidBodyForce;

	/** Show the selected rigid body's on screen text information. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Rigid Body")
	bool bShowRigidBodyInfos;

	/** Show the transform index for the selected rigid body's associated cluster nodes. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Clustering")
	bool bShowTransformIndex;

	/** Show the transform for the selected rigid body's associated cluster nodes. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Clustering")
	bool bShowTransform;

	/** Show a link from the selected rigid body's associated cluster nodes to their parent's nodes. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Clustering")
	bool bShowParent;

	/** Show the hierarchical level for the selected rigid body's associated cluster nodes. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Clustering")
	bool bShowLevel;

	/** Show the connectivity edges for the selected rigid body's associated cluster nodes. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Clustering")
	bool bShowConnectivityEdges;

	/** Show the geometry index for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowGeometryIndex;

	/** Show the geometry transform for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowGeometryTransform;

	/** Show the bounding box for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowBoundingBox;

	/** Show the faces for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowFaces;

	/** Show the face indices for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowFaceIndices;

	/** Show the face normals for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowFaceNormals;

	/** Enable single face visualization for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowSingleFace;

	/** The index of the single face to visualize. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry", meta = (ClampMin="0"))
	int32 SingleFaceIndex;

	/** Show the vertices for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowVertices;

	/** Show the vertex indices for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowVertexIndices;

	/** Show the vertex normals for the selected rigid body's associated geometries. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Geometry")
	bool bShowVertexNormals;

	/** Adapt visualization depending of the cluster nodes' hierarchical level. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings")
	bool bUseActiveVisualization;

	/** Thickness of points when visualizing vertices. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings", meta = (ClampMin="0"))
	float PointThickness;

	/** Thickness of lines when visualizing faces, normals, ...etc. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings", meta = (ClampMin="0"))
	float LineThickness;

	/** Draw shadows under the displayed text. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings")
	bool bTextShadow;

	/** Scale of the font used to display text. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings", meta = (ClampMin="0.0001"))
	float TextScale;

	/** Scale factor used for visualizing normals. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings", meta = (ClampMin="0.0001"))
	float NormalScale;

	/** Scale factor used for visualizing transforms. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings", meta = (ClampMin="0.0001"))
	float TransformScale;

	/** Size of arrows used for visualizing normals, breaking information, ...etc. */
	UPROPERTY(EditAnywhere, Category = "Debug Draw|Settings", meta = (ClampMin="0.0001"))
	float ArrowScale;

	/** Display icon in the editor. */
	UPROPERTY()
	UBillboardComponent* SpriteComponent;

	/** Game tick callback. This tick function is required to clean up the persistent debug lines. */
	static AGeometryCollectionDebugDrawActor* FindOrCreate(UWorld* World);

	/** Game tick callback. This tick function is required to clean up the persistent debug lines. */
	virtual void Tick(float DeltaSeconds) override;

	/** Actor destruction callback. Used here to clear up the command callbacks. */
	virtual void BeginDestroy() override;

	/** Register debug draw service. */
	virtual void BeginPlay() override;

	/** Unregister debug draw service. */
	virtual void EndPlay(EEndPlayReason::Type ReasonEnd) override;

	/** Reset command variables from the newly loaded properties. */
	virtual void PostLoad() override;

#if WITH_EDITOR
	/** Property changed callback. Required to synchronize the command variables to this Actor's properties. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Some properties are unlocked depending on the value of the indices not being -1. */
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	/** Draw vertices. */
	void DrawVertices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw vertices for the part of the geometry attached to the specified transform index. */
	void DrawVertices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw vertex indices. */
	void DrawVertexIndices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw vertex indices for the part of the geometry attached to the specified transform index. */
	void DrawVertexIndices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor & Color);

	/** Draw vertex normals. */
	void DrawVertexNormals(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw vertex normals for the part of the geometry attached to the specified transform index. */
	void DrawVertexNormals(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw faces. */
	void DrawFaces(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw faces for the part of the geometry attached to the specified transform index. */
	void DrawFaces(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw face indices. */
	void DrawFaceIndices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw face indices for the part of the geometry attached to the specified transform index. */
	void DrawFaceIndices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw face normals. */
	void DrawFaceNormals(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw face normals for the part of the geometry attached to the specified transform index. */
	void DrawFaceNormals(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw single face. */
	void DrawSingleFace(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const int32 FaceIndex, const FColor& Color);

	/** Draw geometry indices. */
	void DrawGeometryIndices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw the geometry index for the part of the geometry attached to the specified transform index. */
	void DrawGeometryIndex(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw transforms. */
	void DrawTransforms(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, float Scale);

	/** Draw the transform for the part of the geometry attached to the specified transform index. */
	void DrawTransform(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, float Scale);

	/** Draw transform indices. */
	void DrawTransformIndices(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw the transform index for the part of the geometry attached to the specified transform index. */
	void DrawTransformIndex(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw hierarchical levels. */
	void DrawLevels(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw the hierarchical level for the part of the geometry attached to the specified transform index. */
	void DrawLevel(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw links to the parent. */
	void DrawParents(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw a link to the parent for the part of the geometry attached to the specified transform index. */
	void DrawParent(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

	/** Draw bounding boxes. */
	void DrawBoundingBoxes(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, const FColor& Color);

	/** Draw the bounding box for the part of the geometry attached to the specified transform index. */
	void DrawBoundingBox(const TArray<FTransform>& GlobalTransforms, const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FColor& Color);

#if INCLUDE_CHAOS
	/** Return the concatenated transform for the specified particle. */
	static FTransform GetParticleTransform(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData);

	/** Draw Chaos' rigid body ids. */
	void DrawRigidBodiesId(const UGeometryCollectionComponent* GeometryCollectionComponent,  const FGeometryCollectionParticlesData& ParticlesData, const TManagedArray<int32>& RigidBodyIdArray, const FColor& Color);

	/** Draw Chaos' rigid body id. */
	void DrawRigidBodyId(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const TManagedArray<int32>& RigidBodyIdArray, const FColor& Color);

	/** Draw Chaos' rigid body transform. */
	void DrawRigidBodiesTransform(const UGeometryCollectionComponent* GeometryCollectionComponent, const FGeometryCollectionParticlesData& ParticlesData, float Scale);

	/** Draw Chaos' single rigid body transform. */
	void DrawRigidBodyTransform(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, float Scale);

	/** Draw Chaos' implicit collision primitives. */
	void DrawRigidBodiesCollision(const UGeometryCollectionComponent* GeometryCollectionComponent, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' implicit single collision primitive. */
	void DrawRigidBodyCollision(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' inertia tensors. */
	void DrawRigidBodiesInertia(const UGeometryCollectionComponent* GeometryCollectionComponent, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' single rigid body inertia tensor. */
	void DrawRigidBodyInertia(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' rigid body informations. */
	void DrawRigidBodiesInfo(const UGeometryCollectionComponent* GeometryCollectionComponent, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' single rigid body informations. */
	void DrawRigidBodyInfo(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' rigid clustering's connectivities edges */
	void DrawConnectivityEdges(const UGeometryCollectionComponent* GeometryCollectionComponent, const FGeometryCollectionParticlesData& ParticlesData, const TManagedArray<int32>& RigidBodyIdArray, float Thickness);

	/** Draw Chaos' single rigid clustering's connectivity edges. */
	void DrawConnectivityEdges(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const TManagedArray<int32>& RigidBodyIdArray, float Thickness, FColor HSVColor = FColor(157, 160, 128));

	/** Draw Chaos' rigid body velocity. */
	void DrawRigidBodiesVelocity(const UGeometryCollectionComponent* GeometryCollectionComponent, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' single rigid body velocity. */
	void DrawRigidBodyVelocity(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' rigid body applied force and torque. */
	void DrawRigidBodiesForce(const UGeometryCollectionComponent* GeometryCollectionComponent, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' single rigid body applied force and torque. */
	void DrawRigidBodyForce(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);
#endif  // #if INCLUDE_CHAOS

private:
	/** Return a darker color depending on level. */
	static FColor MakeDarker(const FColor& Color, int32 Level = 1);

	/** Return a smaller scale depending on level. */
	static float MakeSmaller(float Scale, int32 Level = 1) { while (--Level >= 0) { Scale *= 0.666666f; } return Scale; }

	/** Return the hierarchy level for this transform index. */
	static int32 GetLevel(int32 TransformIndex, const TManagedArray<int32>& Parents);

	/** Callback on property changes. */
	void OnPropertyChanged(bool bForceVisibilityUpdate);

	/** Property update function helper. */
	template<typename T1, typename T2>
	void UpdatePropertyValue(T1& PropertyValue, const TAutoConsoleVariable<T2>& ConsoleVariable, bool& bHasChanged);

	/** Callback on console variable. */
	void OnCVarsChanged();

	/** Add debug text output. */
	void AddDebugText(const FString& Text, const FVector& Position = FVector::ZeroVector, const FColor& Color = FColor::White, float Scale = 1.f, bool bDrawShadow = false);

	/** Draw all text output. */
	void DebugDrawText(UCanvas* Canvas, APlayerController* PlayerController);

	/** Clear all persistent strings and debug lines. */
	void Flush();

#if INCLUDE_CHAOS
	/** Return the concatenated transform for the specified particle. Must have the ParticleData X, R, and ChildToParentMap already synced. */
	static FTransform GetParticleTransformNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData);

	/** Draw Chaos' single rigid body id without synchronization checks. */
	void DrawRigidBodyIdNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const TManagedArray<int32>& RigidBodyIdArray, const FColor& Color);

	/** Draw Chaos' single rigid body transform without synchronization checks. */
	void DrawRigidBodyTransformNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, float Scale);

	/** Draw Chaos' implicit single collision primitive without synchronization checks. */
	void DrawRigidBodyCollisionNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' implicit inertia tensor without synchronization checks. */
	void DrawRigidBodyInertiaNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' single rigid body informations without synchronization checks. */
	void DrawRigidBodyInfoNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' single rigid clustering's connectivity edges without synchronization checks. */
	void DrawConnectivityEdgesNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const TManagedArray<int32>& RigidBodyIdArray, float Thickness, const FColor& Color);

	/** Draw Chaos' single rigid body velocity without synchronization checks. */
	void DrawRigidBodyVelocityNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);

	/** Draw Chaos' single rigid body applied force and torque without synchronization checks. */
	void DrawRigidBodyForceNoChecks(const UGeometryCollectionComponent* GeometryCollectionComponent, int32 TransformIndex, const FGeometryCollectionParticlesData& ParticlesData, const FColor& Color);
#endif  // #if INCLUDE_CHAOS

private:
	struct FDebugDrawText
	{
		FString Text;
		FVector Position;
		FColor Color;
		float Scale;
		bool bDrawShadow;
	};

	FConsoleVariableSinkHandle ConsoleVariableSinkHandle;
	FDelegateHandle DebugDrawTextDelegateHandle;
	TArray<FDebugDrawText> DebugDrawTexts;

	bool bNeedsDebugLinesFlush;
#if INCLUDE_CHAOS && WITH_EDITOR
	bool bWasEditorPaused;
#endif  // #if INCLUDE_CHAOS && WITH_EDITOR
};
