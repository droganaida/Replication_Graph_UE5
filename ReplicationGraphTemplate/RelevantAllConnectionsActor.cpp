// Copyright (c) 2025 Aida Drogan, SilverCord-VR Studio

#include "RelevantAllConnectionsActor.h"

// Sets default values
ARelevantAllConnectionsActor::ARelevantAllConnectionsActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

}

// Called when the game starts or when spawned
void ARelevantAllConnectionsActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARelevantAllConnectionsActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}