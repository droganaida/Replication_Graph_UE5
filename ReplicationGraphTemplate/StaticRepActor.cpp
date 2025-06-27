// Copyright (c) 2025 Aida Drogan, SilverCord-VR Studio

#include "StaticRepActor.h"

// Sets default values
AStaticRepActor::AStaticRepActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AStaticRepActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AStaticRepActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}