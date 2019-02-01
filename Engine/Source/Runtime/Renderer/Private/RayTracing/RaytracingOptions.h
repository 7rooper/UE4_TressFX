// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RaytracingOptions.h declares ray tracing options for use in rendering
=============================================================================*/

extern bool IsRayTracingSkyLightSelected();

#if RHI_RAYTRACING

extern bool IsRayTracingRectLightSelected();

extern bool ShouldRenderRayTracingAmbientOcclusion();
extern bool ShouldRenderRayTracingStaticOrStationaryRectLight(const FLightSceneInfo& LightSceneInfo);
extern bool ShouldRenderRayTracingDynamicRectLight(const FLightSceneInfo& LightSceneInfo);

#endif
