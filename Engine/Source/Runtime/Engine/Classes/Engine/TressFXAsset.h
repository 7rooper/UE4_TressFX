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
#include "TressFX/TressFXTypes.h"
#include "UObject/ObjectMacros.h"
#include "Engine/TressFXBoneSkinningAsset.h"
#include "Animation/Skeleton.h"
#include "TressFXAsset.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(TressFXAssetLog, Log, All);

UCLASS()
class ENGINE_API UTressFXAsset : public UObject
{

	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Settings")
		int32 NumFollowStrandsPerGuide;

	UPROPERTY(EditAnywhere, Category = "Settings")
		float TipSeparationFactor;

	UPROPERTY(EditAnywhere, Category = "Settings")
		float MaxRadiusAroundGuideHair;

	UPROPERTY(EditAnywhere, Category = "Settings")
		class UTressFXBoneSkinningAsset* TressFXBoneSkinningAsset;

	UPROPERTY(EditAnywhere, Category = "Settings")
		class USkeletalMesh* BaseSkeleton;

	UPROPERTY()
		bool bIsValid;


	virtual void Serialize(FArchive& Ar) override;

	TSharedPtr<FTressFXRuntimeData> ImportData;

	TArray<FTressFXBoneSkinningData> SkinningData;

	bool IsValid() { return bIsValid; }

public:

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

};