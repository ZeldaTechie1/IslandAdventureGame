// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorAnchor.h"

// Sets default values
AActorAnchor::AActorAnchor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AActorAnchor::InitAnchor(FVector Location, AActor* ActorToAttachTo)
{
	UpdateAnchorLocation(Location);
	AttachToActor(ActorToAttachTo, FAttachmentTransformRules::KeepWorldTransform);
}

void AActorAnchor::UpdateAnchorLocation(FVector NewLocation)
{
	SetActorLocation(NewLocation);
}

// Called when the game starts or when spawned
void AActorAnchor::BeginPlay()
{
	Super::BeginPlay();
	GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Blue, TEXT("Beep"));
}

// Called every frame
void AActorAnchor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

