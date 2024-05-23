// Copyright Epic Games, Inc. All Rights Reserved.

#include "IslandAdventureGameGameMode.h"
#include "IslandAdventureGameCharacter.h"
#include "UObject/ConstructorHelpers.h"

AIslandAdventureGameGameMode::AIslandAdventureGameGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
