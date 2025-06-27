// Copyright (c) 2025 Aida Drogan, SilverCord-VR Studio

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicRepActor.generated.h"

UCLASS()
class REPGRAPHTEST_API ADynamicRepActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADynamicRepActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};