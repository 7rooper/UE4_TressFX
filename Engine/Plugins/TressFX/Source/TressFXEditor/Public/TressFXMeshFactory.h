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

#include "Factories/Factory.h"
#include "CoreMinimal.h"
#include "TressFXImportUI.h"
#include "UObject/ObjectMacros.h"
#include "TressFXMeshFactory.generated.h"

struct FAssetData;
class SWindow;

UCLASS()
class TRESSFXEDITOR_API UTressFXMeshFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
		class USkeletalMesh* TargetSkeleton;

public:

	virtual bool CanCreateNew() const override;

	virtual bool FactoryCanImport(const FString& Filename) override;

	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;

	virtual bool ConfigureProperties() override;

private:

	void OnTargetSkeletonSelected(const FAssetData& SelectedAsset);

private:

	TSharedPtr<SWindow> PickerWindow;

protected:
	TressFXImportOptions ImportConfig;

};