// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMeshBuilder, Log, All);

class UObject;
class UMeshDescription;
struct FMeshBuildSettings;
struct FVertexInstanceID;

class FMeshDescriptionHelper
{
public:
	enum ETangentOptions
	{
		None = 0,
		BlendOverlappingNormals = 0x1,
		IgnoreDegenerateTriangles = 0x2,
		UseMikkTSpace = 0x4,
	};

	enum class ELightmapUVVersion : int32
	{
		BitByBit = 0,
		Segments = 1,
		SmallChartPacking = 2,
		Latest = SmallChartPacking
	};

	FMeshDescriptionHelper(FMeshBuildSettings* InBuildSettings, const UMeshDescription* InOriginalMeshDescription);

	//Build a render mesh description with the BuildSettings. This will update the InRenderMeshDescription ptr content
	UMeshDescription* GetRenderMeshDescription(UObject* Owner);

	void CopyMeshDescription(UMeshDescription* SourceMeshDescription, UMeshDescription* DestinationMeshDescription) const;

	//Return true if there is a valid original mesh description, false otherwise(Auto generate LOD).
	bool IsValidOriginalMeshDescription();

	const TMultiMap<int32, int32>& GetOverlappingCorners() const { return OverlappingCorners; }

private:

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE function declarations

	FORCEINLINE const FVector& GetVertexPositionFromVertexInstance(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID) const;
	FORCEINLINE FVector2D& GetVertexInstanceUV(UMeshDescription* MeshDescription, const FVertexInstanceID& VertexInstanceID, int32 UVLayer) const;
	void CreateNormals(UMeshDescription* MeshDescription, ETangentOptions TangentOptions, bool bComputeTangent);
	void CreateMikktTangents(UMeshDescription* MeshDescription, ETangentOptions TangentOptions);
	void CreatePolygonNTB(UMeshDescription* MeshDescription, float ComparisonThreshold);
	void FindOverlappingCorners(float ComparisonThreshold);

	//////////////////////////////////////////////////////////////////////////
	//PRIVATE class members

	const UMeshDescription* OriginalMeshDescription;
	FMeshBuildSettings *BuildSettings;
	TMultiMap<int32, int32> OverlappingCorners;

	
	//////////////////////////////////////////////////////////////////////////
	//INLINE small helper use to optimize search and compare

	/** Helper struct for building acceleration structures. */
	struct FIndexAndZ
	{
		float Z;
		int32 Index;

		/** Default constructor. */
		FIndexAndZ() {}

		/** Initialization constructor. */
		FIndexAndZ(int32 InIndex, FVector V)
		{
			Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
			Index = InIndex;
		}
	};

	/** Sorting function for vertex Z/index pairs. */
	struct FCompareIndexAndZ
	{
		FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
	};

	/**
	* Smoothing group interpretation helper structure.
	*/
	struct FFanFace
	{
		int32 FaceIndex;
		int32 LinkedVertexIndex;
		bool bFilled;
		bool bBlendTangents;
		bool bBlendNormals;
	};

};
