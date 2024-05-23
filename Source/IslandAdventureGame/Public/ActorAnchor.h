// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActorAnchor.generated.h"

UCLASS()
class ISLANDADVENTUREGAME_API AActorAnchor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AActorAnchor();
	void InitAnchor(FVector Location, AActor* ActorToAttachTo);
	void UpdateAnchorLocation(FVector NewLocation);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
