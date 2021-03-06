// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in the //Fortnite/Main stream
struct CORE_API FFortniteMainBranchObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// World composition tile offset changed from 2d to 3d
		WorldCompositionTile3DOffset,

		// Minor material serialization optimization
		MaterialInstanceSerializeOptimization_ShaderFName,

		// Refactored cull distances to account for HLOD, explicit override and globals in priority
		CullDistanceRefactor_RemovedDefaultDistance,
		CullDistanceRefactor_NeverCullHLODsByDefault,
		CullDistanceRefactor_NeverCullALODActorsByDefault,

		// Support to remove morphtarget generated by bRemapMorphtarget
		SaveGeneratedMorphTargetByEngine,

		// Convert reduction setting options
		ConvertReductionSettingOptions,

		// Serialize the type of blending used for landscape layer weight static params
		StaticParameterTerrainLayerWeightBlendType,
	
		// Fix up None Named animation curve names, 
		FixUpNoneNameAnimationCurves, 

		// Ensure ActiveBoneIndices to have parents even not skinned for old assets
		EnsureActiveBoneIndicesToContainParents,

		// Serialize the instanced static mesh render data, to avoid building it at runtime
		SerializeInstancedStaticMeshRenderData,

		// Cache material quality node usage
		CachedMaterialQualityNodeUsage,
		
		// Font outlines no longer apply to drop shadows for new objects but we maintain the opposite way for backwards compat
		FontOutlineDropShadowFixup,

		// New skeletal mesh import workflow (Geometry only or animation only re-import )
		NewSkeletalMeshImporterWorkflow,

		// Migrate data from previous data structure to new one to support materials per LOD on the Landscape
		NewLandscapeMaterialPerLOD,

		// New Pose Asset data type
		RemoveUnnecessaryTracksFromPose, 

		// Migrate Foliage TLazyObjectPtr to TSoftObjectPtr
		FoliageLazyObjPtrToSoftObjPtr,

		// TimelineTemplates store their derived names instead of dynamically generating
		// This code tied to this version was reverted and redone at a later date
		REVERTED_StoreTimelineNamesInTemplate,

		// Added BakePoseOverride for LOD setting
		AddBakePoseOverrideForSkeletalMeshReductionSetting,

		// TimelineTemplates store their derived names instead of dynamically generating
		StoreTimelineNamesInTemplate,
		
		// New Pose Asset data type
		WidgetStopDuplicatingAnimations,

		// Allow reducing of the base LOD, we need to store some imported model data so we can reduce again from the same data.
		AllowSkeletalMeshToReduceTheBaseLOD,

		// Curve Table size reduction
		ShrinkCurveTableSize,

		// Widgets upgraded with WidgetStopDuplicatingAnimations, may not correctly default-to-self for the widget parameter.
		WidgetAnimationDefaultToSelfFail,

		// HUDWidgets now require an element tag
		FortHUDElementNowRequiresTag,

		// Animation saved as bulk data when cooked
		FortMappedCookedAnimation,

		// Support Virtual Bone in Retarget Manager
		SupportVirtualBoneInRetargeting,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FFortniteMainBranchObjectVersion() {}
};
