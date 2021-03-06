// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	VectorField.cpp: Implementation of vector fields.
==============================================================================*/

#include "VectorField.h"
#include "PrimitiveViewRelevance.h"
#include "RenderingThread.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "PrimitiveSceneProxy.h"
#include "Containers/ResourceArray.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "Engine/CollisionProfile.h"
#include "ComponentReregisterContext.h"
#include "VectorFieldVisualization.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"
#include "FXSystem.h"
#include "VectorField/VectorField.h"
#include "VectorField/VectorFieldAnimated.h"
#include "VectorField/VectorFieldStatic.h"
#include "Components/VectorFieldComponent.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/Material.h"
#include "Engine/Engine.h"

#if WITH_EDITORONLY_DATA
	#include "EditorFramework/AssetImportData.h"
#endif
	
#define MAX_GLOBAL_VECTOR_FIELDS (16)
DEFINE_LOG_CATEGORY(LogVectorField)

/*------------------------------------------------------------------------------
	FVectorFieldResource implementation.
------------------------------------------------------------------------------*/

/**
 * Release RHI resources.
 */
void FVectorFieldResource::ReleaseRHI()
{
	VolumeTextureRHI.SafeRelease();
}

/*------------------------------------------------------------------------------
	FVectorFieldInstance implementation.
------------------------------------------------------------------------------*/

/** Destructor. */
FVectorFieldInstance::~FVectorFieldInstance()
{
	if (Resource && bInstancedResource)
	{
		FVectorFieldResource* InResource = Resource.GetReference();
		InResource->AddRef();
		ENQUEUE_RENDER_COMMAND(FDestroyVectorFieldResourceCommand)(
			[InResource](FRHICommandList& RHICmdList)
			{
				InResource->ReleaseResource();
				InResource->Release();
			});
	}
	Resource = nullptr;
}

/**
 * Initializes the instance for the given resource.
 * @param InResource - The resource to be used by this instance.
 * @param bInstanced - true if the resource is instanced and ownership is being transferred.
 */
void FVectorFieldInstance::Init(FVectorFieldResource* InResource, bool bInstanced)
{
	check(!Resource);
	Resource = InResource;
	bInstancedResource = bInstanced;
}

/**
 * Update the transforms for this vector field instance.
 * @param LocalToWorld - Transform from local space to world space.
 */
void FVectorFieldInstance::UpdateTransforms(const FMatrix& LocalToWorld)
{
	check(Resource);
	const FVector VolumeOffset = Resource->LocalBounds.Min;
	const FVector VolumeScale = Resource->LocalBounds.Max - Resource->LocalBounds.Min;
	VolumeToWorldNoScale = LocalToWorld.GetMatrixWithoutScale().RemoveTranslation();
	VolumeToWorld = FScaleMatrix(VolumeScale) * FTranslationMatrix(VolumeOffset)
		* LocalToWorld;
	WorldToVolume = VolumeToWorld.Inverse();
}

/*------------------------------------------------------------------------------
	UVectorField implementation.
------------------------------------------------------------------------------*/


UVectorField::UVectorField(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Intensity = 1.0f;
}

/**
 * Initializes an instance for use with this vector field.
 */
void UVectorField::InitInstance(class FVectorFieldInstance* Instance, bool bPreviewInstance)
{
	UE_LOG(LogVectorField, Fatal,TEXT("%s must override InitInstance."), *(GetClass()->GetName()));
}

/*------------------------------------------------------------------------------
	UVectorFieldStatic implementation.
------------------------------------------------------------------------------*/


/**
 * Bulk data interface for initializing a static vector field volume texture.
 */
class FVectorFieldStaticResourceBulkDataInterface : public FResourceBulkDataInterface
{
public:

	/** Initialization constructor. */
	FVectorFieldStaticResourceBulkDataInterface( void* InBulkData, uint32 InBulkDataSize )
		: BulkData(InBulkData)
		, BulkDataSize(InBulkDataSize)
	{
	}

	/** 
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		check(BulkData != NULL);
		return BulkData;
	}

	/** 
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		check(BulkDataSize > 0);
		return BulkDataSize;
	}

	/**
	 * Free memory after it has been used to initialize RHI resource 
	 */
	virtual void Discard() override
	{
	}

private:

	/** Pointer to the bulk data. */
	void* BulkData;
	/** Size of the bulk data in bytes. */
	uint32 BulkDataSize;
};

/**
 * Resource for static vector fields.
 */
class FVectorFieldStaticResource : public FVectorFieldResource
{
public:

	/** Initialization constructor. */
	explicit FVectorFieldStaticResource( UVectorFieldStatic* InVectorField )
		: VolumeData(NULL)
	{
		// Copy vector field properties.
		SizeX = InVectorField->SizeX;
		SizeY = InVectorField->SizeY;
		SizeZ = InVectorField->SizeZ;
		Intensity = InVectorField->Intensity;
		LocalBounds = InVectorField->Bounds;

		// Grab a copy of the static volume data.
		InVectorField->SourceData.GetCopy(&VolumeData, /*bDiscardInternalCopy=*/ true);
	}

protected:
	/** Destructor. */
	virtual ~FVectorFieldStaticResource()
	{
		FMemory::Free(VolumeData);
		VolumeData = NULL;
	}

public:

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if (VolumeData && GSupportsTexture3D)
		{
			const uint32 DataSize = SizeX * SizeY * SizeZ * sizeof(FFloat16Color);
			FVectorFieldStaticResourceBulkDataInterface BulkDataInterface(VolumeData, DataSize);
			FRHIResourceCreateInfo CreateInfo(&BulkDataInterface);
			VolumeTextureRHI = RHICreateTexture3D(
				SizeX, SizeY, SizeZ, PF_FloatRGBA,
				/*NumMips=*/ 1,
				/*Flags=*/ TexCreate_ShaderResource,
				/*BulkData=*/ CreateInfo );
			FMemory::Free(VolumeData);
			VolumeData = NULL;
		}
	}

	/**
	 * Update this resource based on changes to the asset.
	 */
	void UpdateResource(UVectorFieldStatic* InVectorField)
	{
		struct FUpdateParams
		{
			int32 SizeX;
			int32 SizeY;
			int32 SizeZ;
			float Intensity;
			FBox Bounds;
			void* VolumeData;
		};

		FUpdateParams UpdateParams;
		UpdateParams.SizeX = InVectorField->SizeX;
		UpdateParams.SizeY = InVectorField->SizeY;
		UpdateParams.SizeZ = InVectorField->SizeZ;
		UpdateParams.Intensity = InVectorField->Intensity;
		UpdateParams.Bounds = InVectorField->Bounds;
		UpdateParams.VolumeData = NULL;
		InVectorField->SourceData.GetCopy(&UpdateParams.VolumeData, /*bDiscardInternalCopy=*/ true);

		FVectorFieldStaticResource* Resource = this;
		ENQUEUE_RENDER_COMMAND(FUpdateStaticVectorFieldCommand)(
			[Resource, UpdateParams](FRHICommandListImmediate& RHICmdList)
			{
				// Free any existing volume data on the resource.
				FMemory::Free(Resource->VolumeData);

				// Update settings on this resource.
				Resource->SizeX = UpdateParams.SizeX;
				Resource->SizeY = UpdateParams.SizeY;
				Resource->SizeZ = UpdateParams.SizeZ;
				Resource->Intensity = UpdateParams.Intensity;
				Resource->LocalBounds = UpdateParams.Bounds;
				Resource->VolumeData = UpdateParams.VolumeData;

				// Update RHI resources.
				Resource->UpdateRHI();
			});
	}

private:

	/** Static volume texture data. */
	void* VolumeData;
};

UVectorFieldStatic::UVectorFieldStatic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAllowCPUAccess(false)
{
}

void UVectorFieldStatic::InitInstance(FVectorFieldInstance* Instance, bool bPreviewInstance)
{
	Instance->Init(Resource, /*bInstanced=*/ false);
}


void UVectorFieldStatic::InitResource()
{
	check(!Resource);

	// Loads and copies the bulk data into CPUData if bAllowCPUAccess is set, otherwise clear CPUData. 
	UpdateCPUData();

	Resource = new FVectorFieldStaticResource(this);
	Resource->AddRef(); // Increment refcount because of UVectorFieldStatic::Resource is not a TRefCountPtr.

	BeginInitResource(Resource);
}


void UVectorFieldStatic::UpdateResource()
{
	check(Resource);

	// Loads and copies the bulk data into CPUData if bAllowCPUAccess is set, otherwise clears CPUData. 
	UpdateCPUData();

	FVectorFieldStaticResource* StaticResource = (FVectorFieldStaticResource*)Resource;
	StaticResource->UpdateResource(this);
}

#if WITH_EDITOR
ENGINE_API void UVectorFieldStatic::SetCPUAccessEnabled()
{
	bAllowCPUAccess = true;
	UpdateCPUData();
}
#endif // WITH_EDITOR

void UVectorFieldStatic::UpdateCPUData()
{
	if (bAllowCPUAccess)
	{
		// Grab a copy of the bulk vector data. 
		// If the data is already loaded it makes a copy and discards the old content,
		// otherwise it simply loads the data directly from file into the pointer after allocating.
		FFloat16Color *Ptr = nullptr;
		SourceData.GetCopy((void**)&Ptr, /* bDiscardInternalCopy */ true);

		// Make sure the data is actually valid. 
		if (!ensure(Ptr))
		{
			UE_LOG(LogVectorField, Error, TEXT("Vector field data is not loaded."));
			return;
		}

		// Make sure the size actually match what we expect
		if (!ensure(SourceData.GetBulkDataSize() == (SizeX*SizeY*SizeZ) * sizeof(FFloat16Color)))
		{
			UE_LOG(LogVectorField, Error, TEXT("Vector field bulk data size is different than expected. Expected %d bytes, got %d."), SizeX*SizeY*SizeZ, SourceData.GetBulkDataSize());
			FMemory::Free(Ptr);
			return;
		}

		// GetCopy should free/unload the data.
		if (SourceData.IsBulkDataLoaded())
		{
			// NOTE(mv): This assertion will fail in the case where the bulk data is still available even though the bDiscardInternalCopy
			//           flag is toggled when FUntypedBulkData::CanLoadFromDisk() also fail. This happens when the user tries to allow 
			//           CPU access to a newly imported file that isn't reloaded. We still have our valid data, so we just issue a 
			//           warning and move on. See FUntypedBulkData::GetCopy() for more details. 
			UE_LOG(LogVectorField, Warning, TEXT("SourceData.GetCopy() is supposed to unload the data after copying, but it is still loaded."));
		}

		// Convert from 16-bit to 32-bit floats.
			// Use vec4s instead of vec3s because of alignment, which in principle would be better for 
			// cache and automatic or manual vectorization, even if the memory usage is 33% larger. 
			// Need to profile to to make sure.
		CPUData.SetNumUninitialized(SizeX*SizeY*SizeZ);
		for (size_t i = 0; i < (size_t)(SizeX*SizeY*SizeZ); i++)
		{
			CPUData[i] = FVector4(float(Ptr[i].R), float(Ptr[i].G), float(Ptr[i].B), 0.0f);
		}

		FMemory::Free(Ptr);
	}
	else
	{
		// If there's no need to access the CPU data just empty the array.
		CPUData.Empty();
	}
}

FRHITexture* UVectorFieldStatic::GetVolumeTextureRef()
{
	if (Resource)
	{
		return Resource->VolumeTextureRHI;
	}
	else
	{	
		// Fallback to a global 1x1x1 black texture when no vector field is loaded or unavailable
		return GBlackVolumeTexture->TextureRHI;
	}
}

void UVectorFieldStatic::ReleaseResource()
{
	if (Resource)
	{
		FVectorFieldResource* InResource = Resource;
		ENQUEUE_RENDER_COMMAND(ReleaseVectorFieldCommand)(
			[InResource](FRHICommandList& RHICmdList)
			{
				InResource->ReleaseResource();
				// Decrement the refcount and possibly delete (see refs from FVectorFieldInstance).
				InResource->Release(); 
			});
		Resource = nullptr;
	}
}

void UVectorFieldStatic::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Store bulk data inline for streaming (to prevent PostLoad spikes)
	if (Ar.IsCooking())
	{
		SourceData.SetBulkDataFlags(BULKDATA_ForceInlinePayload | BULKDATA_SingleUse);
	}

	SourceData.Serialize(Ar,this);
}

void UVectorFieldStatic::PostLoad()
{
	Super::PostLoad();

	if ( !IsTemplate() )
	{
		InitResource();
	}

#if WITH_EDITORONLY_DATA
	if (!SourceFilePath_DEPRECATED.IsEmpty() && AssetImportData)
	{
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(SourceFilePath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
	}
#endif
}

void UVectorFieldStatic::BeginDestroy()
{
	ReleaseResource();
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UVectorFieldStatic::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateResource();
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UVectorFieldStatic::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
}

void UVectorFieldStatic::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	Super::PostInitProperties();
}
#endif

class FVectorFieldCollectorResources : public FOneFrameResource
{
public:
	FVectorFieldCollectorResources(ERHIFeatureLevel::Type InFeatureLevel)
		: VisualizationVertexFactory(InFeatureLevel){}

	FVectorFieldVisualizationVertexFactory VisualizationVertexFactory;

	virtual ~FVectorFieldCollectorResources()
	{
		VisualizationVertexFactory.ReleaseResource();
	}
};

/*------------------------------------------------------------------------------
	Scene proxy for visualizing vector fields.
------------------------------------------------------------------------------*/

class FVectorFieldSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	/** Initialization constructor. */
	explicit FVectorFieldSceneProxy( UVectorFieldComponent* VectorFieldComponent )
		: FPrimitiveSceneProxy(VectorFieldComponent)
		, VisualizationVertexFactory(GetScene().GetFeatureLevel())
	{
		bWillEverBeLit = false;
		VectorFieldInstance = VectorFieldComponent->VectorFieldInstance;
		check(VectorFieldInstance->Resource);
		check(VectorFieldInstance);
	}

	/** Destructor. */
	~FVectorFieldSceneProxy()
	{
		VisualizationVertexFactory.ReleaseResource();
	}

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources() override
	{
		VisualizationVertexFactory.InitResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{	
		QUICK_SCOPE_CYCLE_COUNTER( STAT_VectorFieldSceneProxy_GetDynamicMeshElements );

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				DrawVectorFieldBounds(PDI, View, VectorFieldInstance);

				// Draw a visualization of the vectors contained in the field when selected.
				if (IsSelected() || View->Family->EngineShowFlags.VectorFields)
				{
					FVectorFieldCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FVectorFieldCollectorResources>(View->GetFeatureLevel());
					CollectorResources.VisualizationVertexFactory.InitResource();

					GetVectorFieldMesh(&CollectorResources.VisualizationVertexFactory, VectorFieldInstance, ViewIndex, Collector);
				}
			}
		}
	}

	/**
	 * Computes view relevance for this scene proxy.
	 */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View); 
		Result.bDynamicRelevance = true;
		Result.bOpaqueRelevance = true;
		return Result;
	}

	/**
	 * Computes the memory footprint of this scene proxy.
	 */
	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

private:

	/** The vector field instance which this proxy is visualizing. */
	FVectorFieldInstance* VectorFieldInstance;
	/** Vertex factory for visualization. */
	FVectorFieldVisualizationVertexFactory VisualizationVertexFactory;
};

/*------------------------------------------------------------------------------
	UVectorFieldComponent implementation.
------------------------------------------------------------------------------*/
UVectorFieldComponent::UVectorFieldComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bHiddenInGame = true;
	Intensity = 1.0f;
}


FPrimitiveSceneProxy* UVectorFieldComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if (VectorFieldInstance)
	{
		Proxy = new FVectorFieldSceneProxy(this);
	}
	return Proxy;
}

FBoxSphereBounds UVectorFieldComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector(0.0f);
	NewBounds.BoxExtent = FVector(0.0f);
	NewBounds.SphereRadius = 0.0f;

	if ( VectorField )
	{
		VectorField->Bounds.GetCenterAndExtents( NewBounds.Origin, NewBounds.BoxExtent );
		NewBounds.SphereRadius = NewBounds.BoxExtent.Size();
	}

	return NewBounds.TransformBy( LocalToWorld );
}

void UVectorFieldComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (GEngine->LevelColorationUnlitMaterial != nullptr)
	{
		OutMaterials.Add(GEngine->LevelColorationUnlitMaterial);
	}
}

void UVectorFieldComponent::OnRegister()
{
	Super::OnRegister();

	if (VectorField)
	{
		if (bPreviewVectorField)
		{
			FVectorFieldInstance* Instance = new FVectorFieldInstance();
			VectorField->InitInstance(Instance, /*bPreviewInstance=*/ true);
			Instance->UpdateTransforms(GetComponentTransform().ToMatrixWithScale());
			VectorFieldInstance = Instance;
		}
		else
		{
			UWorld* World = GetWorld();
			if (World && World->Scene && World->Scene->GetFXSystem())
			{
				// Store the FX system for the world in which this component is registered.
				check(FXSystem == NULL);
				FXSystem = World->Scene->GetFXSystem();
				check(FXSystem != NULL);

				// Add this component to the FX system.
				FXSystem->AddVectorField(this);
			}
		}
	}
}


void UVectorFieldComponent::OnUnregister()
{
	if (bPreviewVectorField)
	{
		if (VectorFieldInstance)
		{
			FVectorFieldInstance* InVectorFieldInstance = VectorFieldInstance;
			ENQUEUE_RENDER_COMMAND(FDestroyVectorFieldInstanceCommand)(
				[InVectorFieldInstance](FRHICommandList& RHICmdList)
				{
					delete InVectorFieldInstance;
				});
			VectorFieldInstance = nullptr;
		}
	}
	else if (VectorFieldInstance)
	{
		check(FXSystem != NULL);
		// Remove the component from the FX system.
		FXSystem->RemoveVectorField(this);
	}
	FXSystem = NULL;
	Super::OnUnregister();
}


void UVectorFieldComponent::SendRenderTransform_Concurrent()
{
	Super::SendRenderTransform_Concurrent();
	if (FXSystem)
	{
		FXSystem->UpdateVectorField(this);
	}
}


void UVectorFieldComponent::SetIntensity(float NewIntensity)
{
	Intensity = NewIntensity;
	if (FXSystem)
	{
		FXSystem->UpdateVectorField(this);
	}
}


void UVectorFieldComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	static const FName IntensityPropertyName(TEXT("Intensity"));

	if (FXSystem && PropertyThatChanged
		&& PropertyThatChanged->GetFName() == IntensityPropertyName)
	{
		FXSystem->UpdateVectorField(this);
	}

	Super::PostInterpChange(PropertyThatChanged);
}

#if WITH_EDITOR
void UVectorFieldComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == TEXT("VectorField"))
	{
		if (VectorField && !VectorField->IsA(UVectorFieldStatic::StaticClass()))
		{
			VectorField = NULL;
		}
	}
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	AVectorFieldVolume implementation.
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
	Shader for constructing animated vector fields.
------------------------------------------------------------------------------*/

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FCompositeAnimatedVectorFieldUniformParameters, )
	SHADER_PARAMETER( FVector4, FrameA )
	SHADER_PARAMETER( FVector4, FrameB )
	SHADER_PARAMETER( FVector, VoxelSize )
	SHADER_PARAMETER( float, FrameLerp )
	SHADER_PARAMETER( float, NoiseScale )
	SHADER_PARAMETER( float, NoiseMax )
	SHADER_PARAMETER( uint32, Op )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FCompositeAnimatedVectorFieldUniformParameters,"CVF");

typedef TUniformBufferRef<FCompositeAnimatedVectorFieldUniformParameters> FCompositeAnimatedVectorFieldUniformBufferRef;

/** The number of threads per axis launched to construct the animated vector field. */
#define THREADS_PER_AXIS 8

/**
 * Compute shader used to generate an animated vector field.
 */
class FCompositeAnimatedVectorFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeAnimatedVectorFieldCS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREADS_X"), THREADS_PER_AXIS );
		OutEnvironment.SetDefine( TEXT("THREADS_Y"), THREADS_PER_AXIS );
		OutEnvironment.SetDefine( TEXT("THREADS_Z"), THREADS_PER_AXIS );
		OutEnvironment.SetDefine( TEXT("COMPOSITE_ANIMATED"), 1 );
	}

	/** Default constructor. */
	FCompositeAnimatedVectorFieldCS()
	{
	}

	/** Initialization constructor. */
	explicit FCompositeAnimatedVectorFieldCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		AtlasTexture.Bind( Initializer.ParameterMap, TEXT("AtlasTexture") );
		AtlasTextureSampler.Bind( Initializer.ParameterMap, TEXT("AtlasTextureSampler") );
		NoiseVolumeTexture.Bind( Initializer.ParameterMap, TEXT("NoiseVolumeTexture") );
		NoiseVolumeTextureSampler.Bind( Initializer.ParameterMap, TEXT("NoiseVolumeTextureSampler") );
		OutVolumeTexture.Bind( Initializer.ParameterMap, TEXT("OutVolumeTexture") );
		OutVolumeTextureSampler.Bind( Initializer.ParameterMap, TEXT("OutVolumeTextureSampler") );
	}

	/** Serialization. */
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
		Ar << AtlasTexture;
		Ar << AtlasTextureSampler;
		Ar << NoiseVolumeTexture;
		Ar << NoiseVolumeTextureSampler;
		Ar << OutVolumeTexture;
		Ar << OutVolumeTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	/**
	 * Set parameters for this shader.
	 * @param UniformBuffer - Uniform buffer containing parameters for compositing vector fields.
	 * @param AtlasTextureRHI - The atlas texture with which to create the vector field.
	 * @param NoiseVolumeTextureRHI - The volume texture to use to add noise to the vector field.
	 */
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FCompositeAnimatedVectorFieldUniformBufferRef& UniformBuffer,
		FRHITexture* AtlasTextureRHI,
		FRHITexture* NoiseVolumeTextureRHI )
	{
		FRHIComputeShader* ComputeShaderRHI = GetComputeShader();
		FRHISamplerState* SamplerStateLinear = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<FCompositeAnimatedVectorFieldUniformParameters>(), UniformBuffer );
		SetTextureParameter(RHICmdList, ComputeShaderRHI, AtlasTexture, AtlasTextureSampler, SamplerStateLinear, AtlasTextureRHI );
		SetTextureParameter(RHICmdList, ComputeShaderRHI, NoiseVolumeTexture, NoiseVolumeTextureSampler, SamplerStateLinear, NoiseVolumeTextureRHI );
	}

	/**
	 * Set output buffer for this shader.
	 */
	void SetOutput(FRHICommandList& RHICmdList, FRHIUnorderedAccessView* VolumeTextureUAV)
	{
		FRHIComputeShader* ComputeShaderRHI = GetComputeShader();
		if ( OutVolumeTexture.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVolumeTexture.GetBaseIndex(), VolumeTextureUAV);
		}
	}

	/**
	 * Unbinds any buffers that have been bound.
	 */
	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		FRHIComputeShader* ComputeShaderRHI = GetComputeShader();
		if ( OutVolumeTexture.IsBound() )
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, OutVolumeTexture.GetBaseIndex(), nullptr);
		}
	}

private:

	/** Vector field volume textures to composite. */
	FShaderResourceParameter AtlasTexture;
	FShaderResourceParameter AtlasTextureSampler;
	/** Volume texture to sample as noise. */
	FShaderResourceParameter NoiseVolumeTexture;
	FShaderResourceParameter NoiseVolumeTextureSampler;
	/** The global vector field volume texture to write to. */
	FShaderResourceParameter OutVolumeTexture;
	FShaderResourceParameter OutVolumeTextureSampler;
};
IMPLEMENT_SHADER_TYPE(,FCompositeAnimatedVectorFieldCS,TEXT("/Engine/Private/VectorFieldCompositeShaders.usf"),TEXT("CompositeAnimatedVectorField"),SF_Compute);

/*------------------------------------------------------------------------------
	Animated vector field asset.
------------------------------------------------------------------------------*/


enum
{
	/** Minimum volume size used for animated vector fields. */
	MIN_ANIMATED_VECTOR_FIELD_SIZE = 16,
	/** Maximum volume size used for animated vector fields. */
	MAX_ANIMATED_VECTOR_FIELD_SIZE = 64
};

class FVectorFieldAnimatedResource : public FVectorFieldResource
{
public:

	/** Unordered access view in to the volume texture. */
	FUnorderedAccessViewRHIRef VolumeTextureUAV;
	/** The animated vector field asset. */
	UVectorFieldAnimated* AnimatedVectorField;
	/** The accumulated frame time of the animation. */
	float FrameTime;

	/** Initialization constructor. */
	explicit FVectorFieldAnimatedResource(UVectorFieldAnimated* InVectorField)
		: AnimatedVectorField(InVectorField)
		, FrameTime(0.0f)
	{
		SizeX = InVectorField->VolumeSizeX;
		SizeY = InVectorField->VolumeSizeY;
		SizeZ = InVectorField->VolumeSizeZ;
		Intensity = InVectorField->Intensity;
		LocalBounds = InVectorField->Bounds;
	}

	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI() override
	{
		if (GSupportsTexture3D)
		{
			check(SizeX > 0);
			check(SizeY > 0);
			check(SizeZ > 0);
			UE_LOG(LogVectorField,Verbose,TEXT("InitRHI for 0x%016x %dx%dx%d"),(PTRINT)this,SizeX,SizeY,SizeZ);

			uint32 TexCreateFlags = 0;
			if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
			{
				TexCreateFlags = TexCreate_ShaderResource | TexCreate_UAV;
			}

			FRHIResourceCreateInfo CreateInfo;
			VolumeTextureRHI = RHICreateTexture3D(
				SizeX, SizeY, SizeZ,
				PF_FloatRGBA,
				/*NumMips=*/ 1,
				TexCreateFlags,
				CreateInfo);

			if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
			{
				VolumeTextureUAV = RHICreateUnorderedAccessView(VolumeTextureRHI);
			}
		}
	}

	/**
	 * Release RHI resources.
	 */
	virtual void ReleaseRHI() override
	{
		VolumeTextureUAV.SafeRelease();
		FVectorFieldResource::ReleaseRHI();
	}

	/**
	 * Updates the vector field.
	 * @param DeltaSeconds - Elapsed time since the last update.
	 */
	virtual void Update(FRHICommandListImmediate& RHICmdList, float DeltaSeconds) override
	{
		check(IsInRenderingThread());

		if (GetFeatureLevel() == ERHIFeatureLevel::SM5 && AnimatedVectorField && AnimatedVectorField->Texture && AnimatedVectorField->Texture->Resource)
		{
			SCOPED_DRAW_EVENT(RHICmdList, AnimateVectorField);

			// Move frame time forward.
			FrameTime += AnimatedVectorField->FramesPerSecond * DeltaSeconds;

			// Compute the two frames to lerp.
			const int32 FrameA_Unclamped = FMath::TruncToInt(FrameTime);
			const int32 FrameA = AnimatedVectorField->bLoop ?
				(FrameA_Unclamped % AnimatedVectorField->FrameCount) :
				FMath::Min<int32>(FrameA_Unclamped, AnimatedVectorField->FrameCount - 1);
			const int32 FrameA_X = FrameA % AnimatedVectorField->SubImagesX;
			const int32 FrameA_Y = FrameA / AnimatedVectorField->SubImagesX;

			const int32 FrameB_Unclamped = FrameA + 1;
			const int32 FrameB = AnimatedVectorField->bLoop ?
				(FrameB_Unclamped % AnimatedVectorField->FrameCount) :
				FMath::Min<int32>(FrameB_Unclamped, AnimatedVectorField->FrameCount - 1);
			const int32 FrameB_X = FrameB % AnimatedVectorField->SubImagesX;
			const int32 FrameB_Y = FrameB / AnimatedVectorField->SubImagesX;

			FCompositeAnimatedVectorFieldUniformParameters Parameters;
			const FVector2D AtlasScale(
				1.0f / AnimatedVectorField->SubImagesX,
				1.0f / AnimatedVectorField->SubImagesY);
			Parameters.FrameA = FVector4(
				AtlasScale.X,
				AtlasScale.Y,
				FrameA_X * AtlasScale.X,
				FrameA_Y * AtlasScale.Y );
			Parameters.FrameB = FVector4(
				AtlasScale.X,
				AtlasScale.Y,
				FrameB_X * AtlasScale.X,
				FrameB_Y * AtlasScale.Y );
			Parameters.VoxelSize = FVector(1.0f / SizeX, 1.0f / SizeY, 1.0f / SizeZ);
			Parameters.FrameLerp = FMath::Fractional(FrameTime);
			Parameters.NoiseScale = AnimatedVectorField->NoiseScale;
			Parameters.NoiseMax = AnimatedVectorField->NoiseMax;
			Parameters.Op = (uint32)AnimatedVectorField->ConstructionOp;

			FCompositeAnimatedVectorFieldUniformBufferRef UniformBuffer = 
				FCompositeAnimatedVectorFieldUniformBufferRef::CreateUniformBufferImmediate(Parameters, UniformBuffer_SingleDraw);

			TShaderMapRef<FCompositeAnimatedVectorFieldCS> CompositeCS(GetGlobalShaderMap(GetFeatureLevel()));
			FRHITexture* NoiseVolumeTextureRHI = GBlackVolumeTexture->TextureRHI;
			if (AnimatedVectorField->NoiseField && AnimatedVectorField->NoiseField->Resource)
			{
				NoiseVolumeTextureRHI = AnimatedVectorField->NoiseField->Resource->VolumeTextureRHI;
			}

			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, VolumeTextureUAV);
			RHICmdList.SetComputeShader(CompositeCS->GetComputeShader());
			CompositeCS->SetOutput(RHICmdList, VolumeTextureUAV);
			/// ?
			CompositeCS->SetParameters(
				RHICmdList,
				UniformBuffer,
				AnimatedVectorField->Texture->Resource->TextureRHI,
				NoiseVolumeTextureRHI );
			DispatchComputeShader(
				RHICmdList,
				*CompositeCS,
				SizeX / THREADS_PER_AXIS,
				SizeY / THREADS_PER_AXIS,
				SizeZ / THREADS_PER_AXIS );
			CompositeCS->UnbindBuffers(RHICmdList);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, VolumeTextureUAV);
		}
	}

	/**
	 * Resets the vector field simulation.
	 */
	virtual void ResetVectorField() override
	{
		FrameTime = 0.0f;
	}
};

UVectorFieldAnimated::UVectorFieldAnimated(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VolumeSizeX = 16;
	VolumeSizeY = 16;
	VolumeSizeZ = 16;
	Bounds.Min = FVector(-0.5f, -0.5f, -0.5f);
	Bounds.Max = FVector(0.5, 0.5, 0.5);

}


void UVectorFieldAnimated::InitInstance(FVectorFieldInstance* Instance, bool bPreviewInstance)
{
	FVectorFieldAnimatedResource* Resource = new FVectorFieldAnimatedResource(this);
	if (!bPreviewInstance)
	{
		BeginInitResource(Resource);
	}
	Instance->Init(Resource, /*bInstanced=*/ true);
}

static int32 ClampVolumeSize(int32 InVolumeSize)
{
	const int32 ClampedVolumeSize = FMath::Clamp<int32>(1 << FMath::CeilLogTwo(InVolumeSize),
		MIN_ANIMATED_VECTOR_FIELD_SIZE, MAX_ANIMATED_VECTOR_FIELD_SIZE);
	return ClampedVolumeSize;
}

#if WITH_EDITOR
void UVectorFieldAnimated::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	VolumeSizeX = ClampVolumeSize(VolumeSizeX);
	VolumeSizeY = ClampVolumeSize(VolumeSizeY);
	VolumeSizeZ = ClampVolumeSize(VolumeSizeZ);
	SubImagesX = FMath::Max<int32>(SubImagesX, 1);
	SubImagesY = FMath::Max<int32>(SubImagesY, 1);
	FrameCount = FMath::Max<int32>(FrameCount, 0);

	// If the volume size changes, all components must be reattached to ensure
	// that all volumes are resized.
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == TEXT("VolumeSize"))
	{
		FGlobalComponentReregisterContext ReregisterComponents;
	}
}
#endif // WITH_EDITOR
