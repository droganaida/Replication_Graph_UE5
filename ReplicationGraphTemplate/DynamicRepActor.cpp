// Copyright (c) 2025 Aida Drogan, SilverCord-VR Studio

#include "DynamicRepActor.h"

// Sets default values
ADynamicRepActor::ADynamicRepActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ADynamicRepActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADynamicRepActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}