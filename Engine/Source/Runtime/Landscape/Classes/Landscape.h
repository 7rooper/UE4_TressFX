// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LandscapeProxy.h"
#include "LandscapeBPCustomBrush.h"

#include "Landscape.generated.h"

class ULandscapeComponent;

UENUM()
enum ELandscapeSetupErrors
{
	LSE_None,
	/** No Landscape Info available. */
	LSE_NoLandscapeInfo,
	/** There was already component with same X,Y. */
	LSE_CollsionXY,
	/** No Layer Info, need to add proper layers. */
	LSE_NoLayerInfo,
	LSE_MAX,
};


enum class ERTDrawingType : uint8
{
	RTAtlas,
	RTAtlasToNonAtlas,
	RTNonAtlasToAtlas,
	RTNonAtlas,
	RTMips
};

enum EHeightmapRTType : uint8
{
	HeightmapRT_CombinedAtlas,
	HeightmapRT_CombinedNonAtlas,
	HeightmapRT_Scratch1,
	HeightmapRT_Scratch2,
	HeightmapRT_Scratch3,
	// Mips RT
	HeightmapRT_Mip1,
	HeightmapRT_Mip2,
	HeightmapRT_Mip3,
	HeightmapRT_Mip4,
	HeightmapRT_Mip5,
	HeightmapRT_Mip6,
	HeightmapRT_Mip7,
	HeightmapRT_Count
};

enum EWeightmapRTType : uint8
{
	WeightmapRT_Scratch_RGBA,
	WeightmapRT_Scratch1,
	WeightmapRT_Scratch2,
	WeightmapRT_Scratch3,

	// Mips RT
	WeightmapRT_Mip0,
	WeightmapRT_Mip1,
	WeightmapRT_Mip2,
	WeightmapRT_Mip3,
	WeightmapRT_Mip4,
	WeightmapRT_Mip5,
	WeightmapRT_Mip6,
	WeightmapRT_Mip7,
	
	WeightmapRT_Count
};

// Internal Update Flags that shouldn't be used alone
enum EInternalLandscapeLayersContentUpdateFlag : uint32
{
	Internal_Heightmap_Setup = 0x00000001u,
	Internal_Heightmap_Render = 0x00000002u,
	Internal_Heightmap_BoundsAndCollision = 0x00000004u,
	Internal_Heightmap_ResolveToTexture = 0x00000008u,
	Internal_Heightmap_ClientUpdate = 0x00000010u,

	Internal_Weightmap_Setup = 0x00000100u,
	Internal_Weightmap_Render = 0x00000200u,
	Internal_Weightmap_Collision = 0x00000400u,
	Internal_Weightmap_ResolveToTexture = 0x00000800u,
};

enum ELandscapeLayersContentUpdateFlag : uint32
{
	Heightmap_Render = Internal_Heightmap_Render,
	Weightmap_Render = Internal_Weightmap_Render,

	// Combinations
	Heightmap_All = Internal_Heightmap_Render | Internal_Heightmap_BoundsAndCollision | Internal_Heightmap_ResolveToTexture | Internal_Heightmap_ClientUpdate,
	Weightmap_All = Internal_Weightmap_Render | Internal_Weightmap_Collision | Internal_Weightmap_ResolveToTexture,

	All = Heightmap_All | Weightmap_All,
	All_Setup = Internal_Heightmap_Setup | Internal_Weightmap_Setup,
	All_Render = Heightmap_Render | Weightmap_Render,
};

USTRUCT()
struct FLandscapeLayerBrush
{
	GENERATED_USTRUCT_BODY()

	FLandscapeLayerBrush()
		: BPCustomBrush(nullptr)
	{}

	FLandscapeLayerBrush(ALandscapeBlueprintCustomBrush* InBrush)
		: BPCustomBrush(InBrush)
	{}

#if WITH_EDITOR
	UTextureRenderTarget2D* Render(bool InIsHeightmap, UTextureRenderTarget2D* InCombinedResult)
	{
		if (BPCustomBrush != nullptr)
		{
			TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);
			return BPCustomBrush->Render(InIsHeightmap, InCombinedResult);
		}

		return nullptr;
	}

	bool IsInitialized() const 
	{
		return BPCustomBrush != nullptr ? BPCustomBrush->IsInitialized() : false;
	}

	void Initialize(const FIntRect& InBoundRect, const FIntPoint& InLandscapeRenderTargetSize)
	{
		if (BPCustomBrush != nullptr)
		{
			TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);
			FIntPoint LandscapeSize = InBoundRect.Max - InBoundRect.Min;
			BPCustomBrush->Initialize(LandscapeSize, InLandscapeRenderTargetSize);
			BPCustomBrush->SetIsInitialized(true);
		}
	}
#endif

	UPROPERTY()
	ALandscapeBlueprintCustomBrush* BPCustomBrush;
};

UENUM()
enum ELandscapeBlendMode
{
	LSBM_AdditiveBlend,
	LSBM_AlphaBlend,
	LSBM_MAX,
};

USTRUCT()
struct FLandscapeLayer
{
	GENERATED_USTRUCT_BODY()

	FLandscapeLayer()
		: Guid(FGuid::NewGuid())
		, Name(NAME_None)
		, bVisible(true)
		, bLocked(false)
		, HeightmapAlpha(1.0f)
		, WeightmapAlpha(1.0f)
		, BlendMode(LSBM_AdditiveBlend)
	{}

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FGuid Guid;

	UPROPERTY()
	FName Name;

	UPROPERTY(Transient)
	bool bVisible;

	UPROPERTY()
	bool bLocked;

	UPROPERTY()
	float HeightmapAlpha;

	UPROPERTY()
	float WeightmapAlpha;

	UPROPERTY()
	TEnumAsByte<enum ELandscapeBlendMode> BlendMode;

	UPROPERTY()
	TArray<FLandscapeLayerBrush> Brushes;

	UPROPERTY()
	TArray<int8> HeightmapBrushOrderIndices;

	UPROPERTY()
	TArray<int8> WeightmapBrushOrderIndices;

	UPROPERTY()
	TMap<ULandscapeLayerInfoObject*, bool> WeightmapLayerAllocationBlend; // True -> Substractive, False -> Additive
};

UCLASS(MinimalAPI, showcategories=(Display, Movement, Collision, Lighting, LOD, Input), hidecategories=(Mobility))
class ALandscape : public ALandscapeProxy
{
	GENERATED_BODY()

public:
	ALandscape(const FObjectInitializer& ObjectInitializer);

	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	//~ Begin ALandscapeProxy Interface
	LANDSCAPE_API virtual ALandscape* GetLandscapeActor() override;
#if WITH_EDITOR
	//~ End ALandscapeProxy Interface

	LANDSCAPE_API bool HasAllComponent(); // determine all component is in this actor
	
	// Include Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads, 
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	// Exclude Components with overlapped vertices
	// X2/Y2 Coordinates are "inclusive" max values
	LANDSCAPE_API static void CalcComponentIndicesNoOverlap(const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const int32 ComponentSizeQuads,
		int32& ComponentIndexX1, int32& ComponentIndexY1, int32& ComponentIndexX2, int32& ComponentIndexY2);

	static void SplitHeightmap(ULandscapeComponent* Comp, bool bMoveToCurrentLevel = false, class FMaterialUpdateContext* InOutUpdateContext = nullptr, TArray<class FComponentRecreateRenderStateContext>* InOutRecreateRenderStateContext = nullptr, bool InReregisterComponent = true);
	
	//~ Begin UObject Interface.
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	//~ End UObject Interface

	LANDSCAPE_API bool IsUpToDate() const;

	// Layers stuff
#if WITH_EDITOR
	LANDSCAPE_API void RequestLayersContentUpdate(ELandscapeLayersContentUpdateFlag InDataFlags, bool InUpdateAllMaterials = false);
	LANDSCAPE_API void CreateLayer(FName InName = NAME_None, bool bInUpdateLayersContent = true);
	LANDSCAPE_API bool ReorderLayer(int32 InStartingLayerIndex, int32 InDestinationLayerIndex);
	LANDSCAPE_API bool IsLayerNameUnique(const FName& InName) const;
	LANDSCAPE_API void SetLayerName(int32 InLayerIndex, const FName& InName);
	LANDSCAPE_API void SetLayerAlpha(int32 InLayerIndex, const float InAlpha, bool bInHeightmap);
	LANDSCAPE_API void SetLayerVisibility(int32 InLayerIndex, bool bInVisible);
	LANDSCAPE_API void SetLayerLocked(int32 InLayerIndex, bool bLocked);
	LANDSCAPE_API struct FLandscapeLayer* GetLayer(int32 InLayerIndex);
	LANDSCAPE_API const struct FLandscapeLayer* GetLayer(int32 InLayerIndex) const;
	LANDSCAPE_API const struct FLandscapeLayer* GetLayer(const FGuid& InLayerGuid) const;
	LANDSCAPE_API void ForEachLayer(TFunctionRef<void(struct FLandscapeLayer&)> Fn);
	LANDSCAPE_API void ClearLayer(int32 InLayerIndex, bool bInUpdateCollision = true);
	LANDSCAPE_API void ClearLayer(const FGuid& InLayerGuid, bool bInUpdateCollision = true);
	LANDSCAPE_API void DeleteLayer(int32 InLayerIndex);
	LANDSCAPE_API void SetEditingLayer(const FGuid& InLayerGuid = FGuid());
	LANDSCAPE_API void ShowOnlySelectedLayer(int32 InLayerIndex);
	LANDSCAPE_API void ShowAllLayers();
	LANDSCAPE_API void UpdateLandscapeSplines(const FGuid& InLayerGuid = FGuid(), bool bUpdateOnlySelected = false);
	LANDSCAPE_API void SetLandscapeSplinesReservedLayer(int32 InLayerIndex);
	LANDSCAPE_API struct FLandscapeLayer* GetLandscapeSplinesReservedLayer();
	LANDSCAPE_API const struct FLandscapeLayer* GetLandscapeSplinesReservedLayer() const;
	LANDSCAPE_API bool IsEditingLayerReservedForSplines() const;

	LANDSCAPE_API bool IsLayerBlendSubstractive(int32 InLayerIndex, const TWeakObjectPtr<ULandscapeLayerInfoObject>& InLayerInfoObj) const;
	LANDSCAPE_API void SetLayerSubstractiveBlendStatus(int32 InLayerIndex, bool InStatus, const TWeakObjectPtr<ULandscapeLayerInfoObject>& InLayerInfoObj);

	LANDSCAPE_API void AddBrushToLayer(int32 InLayerIndex, int32 InTargetType, class ALandscapeBlueprintCustomBrush* InBrush);
	LANDSCAPE_API void RemoveBrushFromLayer(int32 InLayerIndex, int32 InTargetType, class ALandscapeBlueprintCustomBrush* InBrush);
	LANDSCAPE_API bool AreAllBrushesCommitedToLayer(int32 InLayerIndex, int32 InTargetType);
	LANDSCAPE_API void SetBrushesCommitStateForLayer(int32 InLayerIndex, int32 InTargetType, bool InCommited);
	LANDSCAPE_API TArray<int8>& GetBrushesOrderForLayer(int32 InLayerIndex, int32 InTargetType);
	LANDSCAPE_API class ALandscapeBlueprintCustomBrush* GetBrushForLayer(int32 InLayerIndex, int32 InTargetType, int8 BrushIndex) const;
	LANDSCAPE_API TArray<class ALandscapeBlueprintCustomBrush*> GetBrushesForLayer(int32 InLayerIndex, int32 InTargetType) const;

private:
	void TickLayers(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction);
	void RegenerateLayersContent();
	void RegenerateLayersHeightmaps();
	void RegenerateLayersWeightmaps();
	void ResolveLayersHeightmapTexture(const TArray<ALandscapeProxy*>& InAllLandscapes);
	void ResolveLayersWeightmapTexture(const TArray<ALandscapeProxy*>& InAllLandscapes);
	void ResolveLayersTexture(FLandscapeLayersTexture2DCPUReadBackResource* InCPUReadBackTexture, UTexture2D* InOriginalTexture);

	bool AreLayersHeightmapTextureResourcesReady(const TArray<ALandscapeProxy*>& InAllLandscapes) const;
	bool AreLayersWeightmapTextureResourcesReady(const TArray<ALandscapeProxy*>& InAllLandscapes) const;

	void UpdateLayersMaterialInstances(const TArray<ULandscapeComponent*>& InComponentsToUpdate, const TMap<ULandscapeComponent*, TArray<ULandscapeLayerInfoObject*>>& InZeroAllocationsPerComponents);

	void PrepareComponentDataToExtractMaterialLayersCS(const FLandscapeLayer& InLayer, int32 InCurrentWeightmapToProcessIndex, bool InOutputDebugName, const TArray<ALandscapeProxy*>& InAllLandscape, class FLandscapeTexture2DResource* InOutTextureData,
														  TArray<struct FLandscapeLayerWeightmapExtractMaterialLayersComponentData>& OutComponentData, TMap<ULandscapeLayerInfoObject*, int32>& OutLayerInfoObjects);
	void PrepareComponentDataToPackMaterialLayersCS(int32 InCurrentWeightmapToProcessIndex, bool InOutputDebugName, const TArray<ULandscapeComponent*>& InAllLandscapeComponents, 
													   TArray<UTexture2D*>& InOutProcessedWeightmaps, TArray<class FLandscapeLayersTexture2DCPUReadBackResource*>& InOutProcessedWeightmapCPUCopy, TArray<struct FLandscapeLayerWeightmapPackMaterialLayersComponentData>& OutComponentData);
	void ReallocateLayersWeightmaps(const TArray<ALandscapeProxy*>& InAllLandscape, const TArray<ULandscapeLayerInfoObject*>& InBrushRequiredAllocations, TArray<ULandscapeComponent*>& OutComponentThatNeedMaterialRebuild);
	void InitLayersWeightmapResources(uint8 InLayerCount);
	bool GenerateZeroAllocationPerComponents(const TArray<ALandscapeProxy*>& InAllLandscape, const TMap<ULandscapeLayerInfoObject*, bool>& InWeightmapLayersBlendSubstractive, TMap<ULandscapeComponent*, TArray<ULandscapeLayerInfoObject*>>& OutZeroAllocationsPerComponents);

	void GenerateLayersRenderQuad(const FIntPoint& InVertexPosition, float InVertexSize, const FVector2D& InUVStart, const FVector2D& InUVSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsAtlasToNonAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsNonAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsNonAtlasToAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<struct FLandscapeLayersTriangle>& OutTriangles) const;
	void GenerateLayersRenderQuadsMip(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, uint8 InCurrentMip, TArray<FLandscapeLayersTriangle>& OutTriangles) const;

	void ClearLayersWeightmapTextureResource(const FString& InDebugName, FTextureRenderTargetResource* InTextureResourceToClear);
	void DrawHeightmapComponentsToRenderTarget(const FString& InDebugName, const TArray<ULandscapeComponent*>& InComponentsToDraw, UTexture* InHeightmapRTRead, UTextureRenderTarget2D* InOptionalHeightmapRTRead2, UTextureRenderTarget2D* InHeightmapRTWrite, ERTDrawingType InDrawType,
											   bool InClearRTWrite, struct FLandscapeLayersHeightmapShaderParameters& InShaderParams, uint8 InMipRender = 0) const;

	void DrawWeightmapComponentsToRenderTarget(const FString& InDebugName, const TArray<ULandscapeComponent*>& InComponentsToDraw, UTexture* InWeightmapRTRead, UTextureRenderTarget2D* InOptionalWeightmapRTRead2, UTextureRenderTarget2D* InWeightmapRTWrite,
											   bool InClearRTWrite, struct FLandscapeLayersWeightmapShaderParameters& InShaderParams, uint8 InMipRender) const;

	void DrawWeightmapComponentsToRenderTarget(const FString& InDebugName, const FIntPoint& InSectionBase, const FVector2D& InScaleBias, UTexture* InWeightmapRTRead, UTextureRenderTarget2D* InOptionalWeightmapRTRead2, UTextureRenderTarget2D* InWeightmapRTWrite,
									 		   bool InClearRTWrite, struct FLandscapeLayersWeightmapShaderParameters& InShaderParams, uint8 InMipRender) const;

	void DrawHeightmapComponentsToRenderTargetMips(TArray<ULandscapeComponent*>& InComponentsToDraw, UTexture* InReadHeightmap, bool InClearRTWrite, struct FLandscapeLayersHeightmapShaderParameters& InShaderParams) const;
	void DrawWeightmapComponentToRenderTargetMips(const FIntPoint& TopLeftTexturePosition, UTexture* InReadWeightmap, bool InClearRTWrite, struct FLandscapeLayersWeightmapShaderParameters& InShaderParams) const;

	void CopyLayersTexture(UTexture* InSourceTexture, UTexture* InDestTexture, FTextureResource* InDestCPUResource = nullptr, const FIntPoint& InFirstComponentSectionBase = FIntPoint(0, 0), uint8 InSourceCurrentMip = 0, uint8 InDestCurrentMip = 0,
							   uint32 InSourceArrayIndex = 0, uint32 InDestArrayIndex = 0) const;
	void CopyLayersTexture(const FString& InSourceDebugName, FTextureResource* InSourceResource, const FString& InDestDebugName, FTextureResource* InDestResource, FTextureResource* InDestCPUResource = nullptr, const FIntPoint& InFirstComponentSectionBase = FIntPoint(0, 0),
							   uint8 InSourceCurrentMip = 0, uint8 InDestCurrentMip = 0, uint32 InSourceArrayIndex = 0, uint32 uInDestArrayIndex = 0) const;

	void PrintLayersDebugRT(const FString& InContext, UTextureRenderTarget2D* InDebugRT, uint8 InMipRender = 0, bool InOutputHeight = true, bool InOutputNormals = false) const;
	void PrintLayersDebugTextureResource(const FString& InContext, FTextureResource* InTextureResource, uint8 InMipRender = 0, bool InOutputHeight = true, bool InOutputNormals = false) const;
	void PrintLayersDebugHeightData(const FString& InContext, const TArray<FColor>& InHeightmapData, const FIntPoint& InDataSize, uint8 InMipRender, bool InOutputNormals = false) const;
	void PrintLayersDebugWeightData(const FString& InContext, const TArray<FColor>& InWeightmapData, const FIntPoint& InDataSize, uint8 InMipRender) const;
#endif

public:

#if WITH_EDITORONLY_DATA
	/** Target Landscape Layer for Landscape Splines */
	UPROPERTY()
	FGuid LandscapeSplinesTargetLayerGuid;
	
	/** Current Editing Landscape Layer*/
	FGuid EditingLayer;

	UPROPERTY(TextExportTransient)
	TArray<FLandscapeLayer> LandscapeLayers;

	UPROPERTY(Transient)
	TArray<UTextureRenderTarget2D*> HeightmapRTList;

	UPROPERTY(Transient)
	TArray<UTextureRenderTarget2D*> WeightmapRTList;

	UPROPERTY(Transient)
	bool PreviousExperimentalLandscapeLayers;

private:

	UPROPERTY(Transient)
	bool WasCompilingShaders;

	UPROPERTY(Transient)
	uint32 LayersContentUpdateFlags;

	UPROPERTY(Transient)
	bool LayersUpdateAllMaterials;

	// Represent all the resolved paint layer, from all layers blended together (size of the landscape x material layer count)
	class FLandscapeTexture2DArrayResource* CombinedLayersWeightmapAllMaterialLayersResource;
	
	// Represent all the resolved paint layer, from the current layer only (size of the landscape x material layer count)
	class FLandscapeTexture2DArrayResource* CurrentLayersWeightmapAllMaterialLayersResource;	
	
	// Used in extracting the material layers data from layer weightmaps (size of the landscape)
	class FLandscapeTexture2DResource* WeightmapScratchExtractLayerTextureResource;	
	
	// Used in packing the material layer data contained into CombinedLayersWeightmapAllMaterialLayersResource to be set again for each component weightmap (size of the landscape)
	class FLandscapeTexture2DResource* WeightmapScratchPackLayerTextureResource;	
#endif

protected:
#if WITH_EDITOR
	FName GenerateUniqueLayerName(FName InName = NAME_None) const;
#endif
};

#if WITH_EDITOR
class LANDSCAPE_API FScopedSetLandscapeEditingLayer
{
public:
	FScopedSetLandscapeEditingLayer(ALandscape* InLandscape, const FGuid& InLayerGUID, TFunction<void()> InCompletionCallback = TFunction<void()>());
	~FScopedSetLandscapeEditingLayer();

private:
	TWeakObjectPtr<ALandscape> Landscape;
	const FGuid& LayerGUID;
	TFunction<void()> CompletionCallback;
};
#endif
