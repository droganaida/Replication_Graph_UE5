
// Copyright (c) 2025 Aida Drogan, SilverCord-VR Studio
// 
// This file is part of a customized replication graph implementation
// adapted for SilverCord-VR multiplayer systems.
//
// Based on original source code from:
// MazyModz - UE4-DAReplicationGraphExample (https://github.com/MazyModz/UE4-DAReplicationGraphExample)

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "MyReplicationGraph.generated.h"

// This enum defines how a given actor class should be handled by the Replication Graph system.
// It determines which replication node the actor will be routed to and how often it will be replicated.

enum class EClassRepPolicy : uint8
{
	// The actor will not be routed by the Replication Graph at all.
	// Typically used for debug actors or other classes that are handled manually.
	NotRouted,

	// The actor is always relevant to all connections.
	// This means it is sent to every connected client regardless of distance, ownership, or visibility.
	// These actors are added to a special AlwaysRelevant list.
	RelevantAllConnections,

	// The actor is spatialized and assumed to have frequent updates (e.g., every frame).
	// It will be placed in the spatial grid, in a node for *static* actors.
	// Use this for actors that update frequently but don't move (e.g., ticking effects).
	Spatialize_Static,

	// The actor is spatialized and updates are needed every frame.
	// These are actors that frequently move or change and should be re-evaluated for relevance every tick.
	// Use for pawns, projectiles, or other highly dynamic objects.
	Spatialize_Dynamic,

	// The actor is spatialized and its replication depends on its NetDormancy state.
	// If the actor is dormant, it won't be replicated until it "wakes up".
	// Use for actors that can become inactive for long periods (e.g., treasure chests, doors).
	Spatialize_Dormancy
};

class UReplicationGraphNode_ActorList;
class UReplicationGraphNode_GridSpatialization2D;
class UReplicationGraphNode_AlwaysRelevant_ForConnection;
class AGameplayDebuggerCategoryReplicator;

/**
 * Custom replication graph for handling spatialized and always relevant actors.
 */
UCLASS(Transient)
class REPGRAPHTEST_API UMyReplicationGraph : public UReplicationGraph
{
public:

	GENERATED_BODY()

	// Called to clear all state before loading a new level or restarting the game.
	// Resets any cached actor/connection mappings.
	virtual void ResetGameWorldState() override;

	// Initializes per-connection replication graph nodes.
	// Called once per client connection. You typically set up AlwaysRelevant nodes here.
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager) override;

	// Registers replication settings per actor class (e.g., cull distances, update frequency).
	// This allows the Replication Graph to decide how to replicate each class.
	virtual void InitGlobalActorClassSettings() override;

	// Initializes the global replication nodes (spatial grid, always relevant list, dormancy, etc).
	// Called once on server start.
	virtual void InitGlobalGraphNodes() override;

	// Determines which replication node(s) an actor should be routed to.
	// Called when an actor is added to the replication system.
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;

	// Removes an actor from the graph and its associated replication nodes.
	// Called when an actor is destroyed or no longer relevant.
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;

	// Helper function to initialize FClassReplicationInfo values for a given actor class.
	// You can configure cull distances, update frequency, and spatialization here.
	void InitClassReplicationInfo(FClassReplicationInfo& Info, UClass* InClass, bool bSpatilize, float ServerMaxTickRate);

	// Array of classes that are spatialized (added to spatial grid for relevance checking).
	// These classes are routed to the spatial replication nodes.
	UPROPERTY()
	TArray<UClass*> SpatializedClasses;

	// Array of classes that are not spatialized (handled manually or by special nodes).
	// Typically includes always relevant actors or those with custom logic.
	UPROPERTY()
	TArray<UClass*> NonSpatializedClasses;

	// Array of classes that are always relevant to all connections.
	// These actors are sent to every client, regardless of position or visibility.
	UPROPERTY()
	TArray<UClass*> AlwaysRelevantClasses;

	// Main 2D spatial grid node used to determine relevance based on actor location.
	// Needed for spatialized actors to be grouped by world space efficiently.
	// Declared as UPROPERTY to prevent garbage collection.
	UPROPERTY()
	UReplicationGraphNode_GridSpatialization2D* GridNode;

	// Node that holds actors which are always relevant to all clients.
	// Prevents garbage collection and ensures proper replication behavior.
	UPROPERTY()
	UReplicationGraphNode_ActorList* AlwaysRelevantNode;

	// Stores per-level actor lists that are always relevant to all connections.
	// Used for streaming levels to ensure key actors (e.g. doors, triggers) are replicated
	// to clients when the corresponding level becomes visible.
	TMap<FName, FActorRepListRefView> AlwaysRelevantStreamingLevelActors;

protected:

	// Returns the custom AlwaysRelevant node for a specific PlayerController.
	// This node holds actors that are only relevant to this connection,
	// such as streaming level actors or player-owned objects.
	UDAReplicationGraphNode_AlwaysRelevant_ForConnection* GetAlwaysRelevantNode(APlayerController* PlayerController);

	// Returns true if the given replication policy represents a spatialized actor.
	// Spatialized actors are placed into the spatial grid based on world location
	// and replicated only to clients within nearby grid cells.
	FORCEINLINE bool IsSpatialized(EClassRepPolicy Mapping)
	{
		return Mapping >= EClassRepPolicy::Spatialize_Static;
	}

	// Returns the replication policy for a given actor class.
	// The policy determines how and where the actor will be routed in the replication graph.
	// Policies are stored in ClassRepPolicies, and fallback to NotRouted if not found.
	EClassRepPolicy GetMappingPolicy(UClass* InClass);

	// Stores replication policies for each actor class.
	// These mappings are used to determine how to route actors in the replication graph.
	// Populated during InitGlobalActorClassSettings().
	TClassMap<EClassRepPolicy> ClassRepPolicies;

	/*============================================================================*/
	// These parameters configure the 2D spatial grid used by the Replication Graph 
	// to efficiently determine which actors should be replicated to which clients 
	// based on their world location.

	UPROPERTY(config)
	float GridCellSize;
	// The size (in Unreal Units) of each cell in the 2D spatial grid.
	// This controls the granularity of spatial replication.
	// Example: 10000.f = 100 meters per cell.
	// Use smaller values for dense, small levels (e.g. 1000–5000).
	// Use larger values for open-world games (e.g. 10000–20000+).
	// Ideally, choose a size that covers a reasonable number of actors per cell
	// without being so large that clients receive too many irrelevant updates.

	UPROPERTY(config)
	float SpatialBiasX;

	UPROPERTY(config)
	float SpatialBiasY;
	// These values shift the origin of the replication grid along X and Y axes.
	// They are needed if your level contains actors with negative world coordinates.
	// The bias ensures that the grid starts "before" the minimum world bounds,
	// so all actors fit into positive grid indices.
	// Tip: Set these to the negative of the lowest X/Y actor positions in your level.

	/*============================================================================*/
	// The maximum distance (in UU) at which dynamically moving actors (e.g. pawns, projectiles)
	// are replicated to clients. Beyond this distance, they are culled.
	// This value is read from the config file (e.g. DefaultEngine.ini).
	UPROPERTY(config)
	float CullDistanceForDynamic;

	// The maximum replication distance for static actors (e.g. static meshes, ambient effects).
	// These actors are spatialized but don't change location, so can be culled aggressively.
	UPROPERTY(config)
	float CullDistanceForStatic;

	// The culling distance for actors using NetDormancy (e.g. chests, levers).
	// Dormant actors are replicated only when they "wake up", and this value controls
	// when they're considered out of range.
	UPROPERTY(config)
	float CullDistanceForDormancy;

	// Number of server frames between replication updates for actors.
	// A lower value means more frequent updates (e.g., 1 = every frame).
	UPROPERTY(config)
	float ActorReplicationPeriodForDynamic;

	UPROPERTY(config)
	float ActorReplicationPeriodForStatic;

	UPROPERTY(config)
	float ActorReplicationPeriodForDormancy;

};

// Custom replication graph node that extends the base class for handling
// per-connection "always relevant" actors.
// This node is used to replicate actors that are always important for a specific client,
// such as PlayerStates, HUD elements, or actors in loaded streaming levels.

UCLASS()
class UDAReplicationGraphNode_AlwaysRelevant_ForConnection : public UReplicationGraphNode_AlwaysRelevant_ForConnection
{
public:

	GENERATED_BODY()

	// Called once per frame for each client connection.
	// This function is responsible for gathering all actors that should be replicated
	// to this particular client, regardless of distance or visibility.
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	// Called when a streaming level becomes visible to the client.
	// You can use this to start replicating actors from that level.
	void OnClientLevelVisibilityAdd(FName LevelName, UWorld* LevelWorld);

	// Called when a streaming level becomes invisible to the client.
	// You can use this to stop replicating actors from that level.
	void OnClientLevelVisibilityRemove(FName LevelName);

	// Resets internal state when the game world is reset (e.g. level transition).
	void ResetGameWorldState();

protected:

	// Stores the names of streaming levels that are currently visible to the client.
	// Used to ensure that actors from these levels are included in replication for this connection.
	TArray<FName, TInlineAllocator<64>> AlwaysRelevantStreamingLevels;
};