// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "InstancedFoliage.h"

struct FFoliageActor : public FFoliageImpl
{
	TArray<TWeakObjectPtr<AActor>> ActorInstances;
	UClass* ActorClass;
	bool bInitialized;

	FFoliageActor()
		: ActorClass(nullptr)
		, bInitialized(false)
	{
	}

	virtual void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector) override;
	virtual void Serialize(FArchive& Ar) override;
	void DestroyActors(bool bOnLoad);

#if WITH_EDITOR
	virtual bool IsInitialized() const override;
	virtual void Initialize(AInstancedFoliageActor* IFA, const UFoliageType* FoliageType) override;
	virtual void Uninitialize() override;
	virtual int32 GetInstanceCount() const override;
	virtual void PreAddInstances(AInstancedFoliageActor* IFA, const UFoliageType* FoliageType, int32 Count) override;
	virtual void AddInstance(AInstancedFoliageActor* IFA, const FFoliageInstance& NewInstance) override;
	virtual void RemoveInstance(int32 InstanceIndex) override;
	virtual void SetInstanceWorldTransform(int32 InstanceIndex, const FTransform& Transform, bool bTeleport) override;
	virtual FTransform GetInstanceWorldTransform(int32 InstanceIndex) const override;
	virtual bool IsOwnedComponent(const UPrimitiveComponent* Component) const override;
	int32 FindIndex(const AActor* InActor) const;
	virtual int32 FindIndex(const UPrimitiveComponent* HitComponent) const override;

	virtual void SelectInstances(bool bSelect, int32 InstanceIndex, int32 Count) override;
	virtual void ApplySelection(bool bApply, const TSet<int32>& SelectedIndices) override;
	virtual void ClearSelection(const TSet<int32>& SelectedIndices) override;
	virtual void Refresh(AInstancedFoliageActor* IFA, const TArray<FFoliageInstance>& Instances, bool Async, bool Force) override;
	virtual void OnHiddenEditorViewMaskChanged(uint64 InHiddenEditorViews) override;
	virtual void PostEditUndo(AInstancedFoliageActor* IFA, UFoliageType* FoliageType, const TArray<FFoliageInstance>& Instances, const TSet<int32>& SelectedIndices) override;
	virtual void NotifyFoliageTypeChanged(AInstancedFoliageActor* IFA, UFoliageType* FoliageType, const TArray<FFoliageInstance>& Instances, const TSet<int32>& SelectedIndices, bool bSourceChanged) override;
	void Reapply(AInstancedFoliageActor* IFA, const UFoliageType* FoliageType, const TArray<FFoliageInstance>& Instances);
	AActor* Spawn(AInstancedFoliageActor* IFA, const FFoliageInstance& Instance);
	TArray<AActor*> GetActorsFromSelectedIndices(const TSet<int32>& SelectedIndices) const;
#endif
};