// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysXPublic.h"
#include "Containers/Union.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsInterfaceUtilsCore.h"
#include "PhysicsEngine/ConstraintInstance.h"

class FPhysScene_PhysX;

#if WITH_PHYSX

// FILTER DATA

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
FCollisionFilterData CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const struct FCollisionQueryParams& QueryParam, const struct FCollisionObjectQueryParams & ObjectParam, const bool bMultitrace);

struct FConstraintBrokenDelegateData
{
	FConstraintBrokenDelegateData(FConstraintInstance* ConstraintInstance);

	void DispatchOnBroken()
	{
		OnConstraintBrokenDelegate.ExecuteIfBound(ConstraintIndex);
	}

	FOnConstraintBroken OnConstraintBrokenDelegate;
	int32 ConstraintIndex;
};

class FPhysicsReplication;

/** Interface for the creation of customized physics replication.*/
class IPhysicsReplicationFactory
{
public:
	virtual FPhysicsReplication* Create(FPhysScene* OwningPhysScene) = 0;
	virtual void Destroy(FPhysicsReplication* PhysicsReplication) = 0;
};

#endif // WITH_PHYX
