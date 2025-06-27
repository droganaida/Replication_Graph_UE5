// Copyright (c) 2025 Aida Drogan, SilverCord-VR Studio
// 
// This file is part of a customized replication graph implementation
// adapted for SilverCord-VR multiplayer systems.
//
// Based on original source code from:
// MazyModz - UE4-DAReplicationGraphExample (https://github.com/MazyModz/UE4-DAReplicationGraphExample)

#include "MyReplicationGraph.h"
#include "DynamicRepActor.h"
#include "StaticRepActor.h"
#include "DormantRepActor.h"
#include "RelevantAllConnectionsActor.h"

void UMyReplicationGraph::ResetGameWorldState()
{
	Super::ResetGameWorldState();
	AlwaysRelevantStreamingLevelActors.Empty();

	// Iterate over both active and pending network connections.
	// This ensures we reset all per-connection replication nodes,
	// including those still initializing.
	for (auto& ConnectionList : { Connections, PendingConnections })
	{
		for (UNetReplicationGraphConnection* Connection : ConnectionList)
		{
			for (UReplicationGraphNode* ConnectionNode : Connection->GetConnectionGraphNodes())
			{
				UDAReplicationGraphNode_AlwaysRelevant_ForConnection* Node = Cast<UDAReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode);
				if (Node != nullptr)
				{
					Node->ResetGameWorldState();
				}
			}
		}
	}
}

void UMyReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager)
{
	UDAReplicationGraphNode_AlwaysRelevant_ForConnection* Node = CreateNewNode<UDAReplicationGraphNode_AlwaysRelevant_ForConnection>();
	ConnectionManager->OnClientVisibleLevelNameAdd.AddUObject(Node, &UDAReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd);
	ConnectionManager->OnClientVisibleLevelNameRemove.AddUObject(Node, &UDAReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove);

	AddConnectionGraphNode(Node, ConnectionManager);
}

void UMyReplicationGraph::InitGlobalActorClassSettings()
{
	Super::InitGlobalActorClassSettings();

	auto SetRule = [&](UClass* InClass, EClassRepPolicy Mapping) { ClassRepPolicies.Set(InClass, Mapping); };

	SetRule(AReplicationGraphDebugActor::StaticClass(), EClassRepPolicy::NotRouted);
	SetRule(AInfo::StaticClass(), EClassRepPolicy::RelevantAllConnections);

	SetRule(ADynamicRepActor::StaticClass(), EClassRepPolicy::Spatialize_Dynamic);
	SetRule(AStaticRepActor::StaticClass(), EClassRepPolicy::Spatialize_Static);
	SetRule(ADormantRepActor::StaticClass(), EClassRepPolicy::Spatialize_Dormancy);
	SetRule(ARelevantAllConnectionsActor::StaticClass(), EClassRepPolicy::RelevantAllConnections);

	TArray<UClass*> ReplicatedClasses;
	for (TObjectIterator<UClass> Itr; Itr; ++Itr)
	{
		UClass* Class = *Itr;
		AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());

		// Skip this class if it’s not an actor or it’s not marked for replication
		if (!ActorCDO || !ActorCDO->GetIsReplicated())
		{
			continue;
		}

		// Skip temporary classes generated during Blueprint compilation
		FString ClassName = Class->GetName();
		if (ClassName.StartsWith("SKEL_") || ClassName.StartsWith("REINST_"))
		{
			continue;
		}

		ReplicatedClasses.Add(Class);

		// If this class already has a replication policy, skip further processing
		if (ClassRepPolicies.Contains(Class, false))
		{
			continue;
		}

		// Determines whether an actor should be spatialized (placed in the replication grid).
		// Spatialization is used for actors whose relevance is based on location and proximity.
		// Do not spatialize:
		// - AlwaysRelevant actors (replicated to all clients, no matter where they are)
		// - Actors only relevant to owner
		// - Actors that inherit relevancy from their owner (e.g., weapons or components)

		auto ShouldSpatialize = [](const AActor* Actor)
			{
				return Actor->GetIsReplicated() && (!(Actor->bAlwaysRelevant || Actor->bOnlyRelevantToOwner || Actor->bNetUseOwnerRelevancy));
			};

		// Skip if this class inherits replication settings unchanged from its superclass.
		// This avoids redundant policy checks when the child class does not override any replication-related flags.
		UClass* SuperClass = Class->GetSuperClass();
		if (AActor* SuperCDO = Cast<AActor>(SuperClass->GetDefaultObject()))
		{
			if (SuperCDO->GetIsReplicated() == ActorCDO->GetIsReplicated()
				&& SuperCDO->bAlwaysRelevant == ActorCDO->bAlwaysRelevant
				&& SuperCDO->bOnlyRelevantToOwner == ActorCDO->bOnlyRelevantToOwner
				&& SuperCDO->bNetUseOwnerRelevancy == ActorCDO->bNetUseOwnerRelevancy)
			{
				continue;
			}

			if (ShouldSpatialize(ActorCDO) == false && ShouldSpatialize(SuperCDO) == true)
			{
				NonSpatializedClasses.Add(Class);
			}
		}

		if (ShouldSpatialize(ActorCDO) == true)
		{
			SetRule(Class, EClassRepPolicy::Spatialize_Dynamic);
		}
		else if (ActorCDO->bAlwaysRelevant && !ActorCDO->bOnlyRelevantToOwner)
		{
			SetRule(Class, EClassRepPolicy::RelevantAllConnections);
		}
	}

	// This ensures manually configured classes won't be overridden by default logic later.
	TArray<UClass*> ExplicitlySetClasses;
	auto SetClassInfo = [&](UClass* InClass, FClassReplicationInfo& RepInfo)
		{
			GlobalActorReplicationInfoMap.SetClassInfo(InClass, RepInfo);
			ExplicitlySetClasses.Add(InClass);
		};

	// Configure custom replication settings for key actor classes used in this project.
	FClassReplicationInfo PawnClassInfo;
	PawnClassInfo.SetCullDistanceSquared(FMath::Square(CullDistanceForDynamic));
	SetClassInfo(APawn::StaticClass(), PawnClassInfo);
	
	FClassReplicationInfo DynamicRepActorInfo;
	DynamicRepActorInfo.SetCullDistanceSquared(FMath::Square(CullDistanceForDynamic));
	DynamicRepActorInfo.ReplicationPeriodFrame = FMath::Max(1u, (uint32)ActorReplicationPeriodForDynamic);
	SetClassInfo(ADynamicRepActor::StaticClass(), DynamicRepActorInfo);

	FClassReplicationInfo StaticInfo;
	StaticInfo.SetCullDistanceSquared(FMath::Square(CullDistanceForStatic));
	StaticInfo.ReplicationPeriodFrame = FMath::Max(1u, (uint32)ActorReplicationPeriodForStatic);
	GlobalActorReplicationInfoMap.SetClassInfo(AStaticRepActor::StaticClass(), StaticInfo);

	FClassReplicationInfo DormantInfo;
	DormantInfo.SetCullDistanceSquared(FMath::Square(CullDistanceForDormancy));
	DormantInfo.ReplicationPeriodFrame = FMath::Max(1u, (uint32)ActorReplicationPeriodForDormancy);
	GlobalActorReplicationInfoMap.SetClassInfo(ADormantRepActor::StaticClass(), DormantInfo);
	
	// Process all remaining replicated classes that haven't been explicitly configured.
	// For each, determine if it should be spatialized and apply default replication settings.
	for (UClass* ReplicatedClass : ReplicatedClasses)
	{
		if (ExplicitlySetClasses.FindByPredicate([&](const UClass* InClass) { return ReplicatedClass->IsChildOf(InClass); }) != nullptr)
		{
			continue;
		}

		bool bSptatilize = IsSpatialized(ClassRepPolicies.GetChecked(ReplicatedClass));

		FClassReplicationInfo ClassInfo;
		InitClassReplicationInfo(ClassInfo, ReplicatedClass, bSptatilize, NetDriver->NetServerMaxTickRate);
		GlobalActorReplicationInfoMap.SetClassInfo(ReplicatedClass, ClassInfo);
	}
}

// Initializes global graph nodes for spatialized and always relevant actors.
// Adds a 2D spatial grid node for dynamic relevance and a static list node for actors always relevant to all connections.
void UMyReplicationGraph::InitGlobalGraphNodes()
{
	GridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = GridCellSize;
	GridNode->SpatialBias = FVector2D(SpatialBiasX, SpatialBiasY);

	AddGlobalGraphNode(GridNode);
	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);
}

// Routes a newly replicated actor to the appropriate replication graph node,
// based on its class replication policy (e.g., spatialized, always relevant).
void UMyReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{

	UE_LOG(LogTemp, Warning, TEXT("Routing: %s (Policy: %d)"), *GetNameSafe(ActorInfo.Actor), (int32)GetMappingPolicy(ActorInfo.Class));

	EClassRepPolicy MappingPolicy = GetMappingPolicy(ActorInfo.Class);
	switch (MappingPolicy)
	{
	case EClassRepPolicy::RelevantAllConnections:
	{
		if (ActorInfo.StreamingLevelName == NAME_None)
		{
			AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
		}
		else
		{
			FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindOrAdd(ActorInfo.StreamingLevelName);
			RepList.Add(ActorInfo.Actor);
		}
		break;
	}
	case EClassRepPolicy::Spatialize_Static:
		GridNode->AddActor_Static(ActorInfo, GlobalInfo);
		break;
	case EClassRepPolicy::Spatialize_Dynamic:
		GridNode->AddActor_Dynamic(ActorInfo, GlobalInfo);
		break;
	case EClassRepPolicy::Spatialize_Dormancy:
		GridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);
		break;
	default:
		break;
	}
}

// Removes a network actor from the appropriate replication graph node,
// based on its class replication policy (e.g., spatialized, always relevant).
void UMyReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	EClassRepPolicy MappingPolicy = GetMappingPolicy(ActorInfo.Class);
	switch (MappingPolicy)
	{
	case EClassRepPolicy::RelevantAllConnections:
	{
		if (ActorInfo.StreamingLevelName == NAME_None)
		{
			AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
		}
		else
		{
			FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindOrAdd(ActorInfo.StreamingLevelName);
			RepList.RemoveFast(ActorInfo.Actor);
		}
		break;
	}
	case EClassRepPolicy::Spatialize_Static:
		GridNode->RemoveActor_Static(ActorInfo);
		break;
	case EClassRepPolicy::Spatialize_Dynamic:
		GridNode->RemoveActor_Dynamic(ActorInfo);
		break;
	case EClassRepPolicy::Spatialize_Dormancy:
		GridNode->RemoveActor_Dormancy(ActorInfo);
		break;
	default:
		break;
	}
}

// Initializes replication settings for a specific actor class.
// - Sets cull distance if spatialized.
// - Calculates update frequency in frames based on server tick rate and NetUpdateFrequency.
void UMyReplicationGraph::InitClassReplicationInfo(FClassReplicationInfo& Info, UClass* InClass, bool bSpatilize, float ServerMaxTickRate)
{
	if (AActor* CDO = Cast<AActor>(InClass->GetDefaultObject()))
	{
		if (bSpatilize == true)
		{
			Info.SetCullDistanceSquared(CDO->NetCullDistanceSquared);
		}

		Info.ReplicationPeriodFrame = FMath::Max<uint32>((uint32)FMath::RoundToFloat(ServerMaxTickRate / CDO->NetUpdateFrequency), 1);
	}
}

/*============================================================================*/

// Returns the custom AlwaysRelevant node for the given PlayerController.
// This node manages always-relevant actors for the client's streaming levels.
// Used to add/remove actors from per-connection relevance lists.

UDAReplicationGraphNode_AlwaysRelevant_ForConnection* UMyReplicationGraph::GetAlwaysRelevantNode(APlayerController* PlayerController)
{
	if (PlayerController != NULL)
	{
		if (UNetConnection* NetConnection = PlayerController->NetConnection)
		{
			if (UNetReplicationGraphConnection* GraphConnection = FindOrAddConnectionManager(NetConnection))
			{
				for (UReplicationGraphNode* ConnectionNode : GraphConnection->GetConnectionGraphNodes())
				{
					UDAReplicationGraphNode_AlwaysRelevant_ForConnection* Node = Cast<UDAReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode);
					if (Node != NULL)
					{
						return Node;
					}
				}
			}
		}
	}

	return nullptr;
}

EClassRepPolicy UMyReplicationGraph::GetMappingPolicy(UClass* InClass)
{

	return ClassRepPolicies.Get(InClass) != NULL ? *ClassRepPolicies.Get(InClass) : EClassRepPolicy::NotRouted;
}

// Gathers AlwaysRelevant actor lists for streaming levels currently visible to this client.
// Skips dormant actors and removes streaming levels with no active actors from the client's view.
// Called every replication tick to update which streaming-level actors should be replicated to this connection.
void UDAReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	Super::GatherActorListsForConnection(Params);
	UMyReplicationGraph* RepGraph = CastChecked<UMyReplicationGraph>(GetOuter());

	FPerConnectionActorInfoMap& ConnectionActorInfoMap = Params.ConnectionManager.ActorInfoMap;
	TMap<FName, FActorRepListRefView>& AlwaysRelevantStreamingLevelActors = RepGraph->AlwaysRelevantStreamingLevelActors;

	for (int32 Idx = AlwaysRelevantStreamingLevels.Num() - 1; Idx >= 0; --Idx)
	{
		FName StreamingLevel = AlwaysRelevantStreamingLevels[Idx];
		FActorRepListRefView* ListPtr = AlwaysRelevantStreamingLevelActors.Find(StreamingLevel);

		if (ListPtr == nullptr)
		{
			AlwaysRelevantStreamingLevels.RemoveAtSwap(Idx, 1, false);
			continue;
		}

		FActorRepListRefView& RepList = *ListPtr;
		if (RepList.Num() > 0)
		{
			bool bAllDormant = true;
			for (FActorRepListType Actor : RepList)
			{
				FConnectionReplicationActorInfo& ConnectionActorInfo = ConnectionActorInfoMap.FindOrAdd(Actor);
				if (ConnectionActorInfo.bDormantOnConnection == false)
				{
					bAllDormant = false;
					break;
				}
			}

			if (bAllDormant == true)
			{
				AlwaysRelevantStreamingLevels.RemoveAtSwap(Idx, 1, false);
			}
			else
			{
				Params.OutGatheredReplicationLists.AddReplicationActorList(RepList);
			}
		}
	}


}

void UDAReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd(FName LevelName, UWorld* LevelWorld)
{
	AlwaysRelevantStreamingLevels.Add(LevelName);
}

void UDAReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove(FName LevelName)
{
	AlwaysRelevantStreamingLevels.Remove(LevelName);
}

void UDAReplicationGraphNode_AlwaysRelevant_ForConnection::ResetGameWorldState()
{
	AlwaysRelevantStreamingLevels.Empty();
}