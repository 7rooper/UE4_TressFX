// ----------------------------------------------------------------------------
//
//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//This code was modified for use with Unreal Engine 4 by Leon Rosengarten 2018.
#pragma once
#include "CoreMinimal.h"
#include "TressFXTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "TressFXMesh.generated.h"

UCLASS()
class TRESSFX_API UTressFXMesh : public UObject
{

	GENERATED_UCLASS_BODY()

public:

	FTressFXMeshData MeshData;

public:

	void LoadData(
		const TArray<FVector>& InPositions,
		const TArray<FVector>& InNormals,
		const TArray<int32>& InIndices,
		const TArray<FTressFXBoneSkinningData> &InSkinningData,
		int32 NumTriangles,
		FRotator InImportRotation = FRotator(0, 0, 0),
		FVector InImportTranslation = FVector(0, 0, 0),
		bool bProcessImportRoationAndTranslation = false
		);

public:

	virtual void Serialize(FArchive& Ar) override;

};