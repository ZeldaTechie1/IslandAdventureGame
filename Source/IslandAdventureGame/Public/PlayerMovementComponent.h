// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ActorAnchor.h"
#include "PlayerMovementComponent.generated.h"


/**
 *
 */
UCLASS()
class ISLANDADVENTUREGAME_API UPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	void TryClimbing();
	void CancelClimbing();
	UFUNCTION(BlueprintPure)
		bool IsClimbing() const;
	UFUNCTION(BlueprintPure)
		FVector GetClimbSurfaceNormal() const;
	UFUNCTION(BlueprintCallable)
		void TryClimbDashing();
	UFUNCTION(BlueprintPure)
		bool IsClimbDashing() const { return IsClimbing() && bIsClimbDashing; }
	UFUNCTION(BlueprintPure)
		FVector GetClimbDashDirection() const { return ClimbDashDirection; }
	UFUNCTION(BlueprintCallable)
		void TryGrapple();

private:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;

	//TODO:Put all of these into a state
	//Climbing Functions
	void SweepAndStoreWallHits();
	bool CanStartClimbing();
	bool EyeHeightTrace(const float TraceDistance, FVector& TraceHitLocation) const;
	bool IsFacingSurface(const float Steepness) const;
	bool IsClimbableSurface(const FVector WallNormal) const;
	void PhysClimbing(float deltaTime, int32 Iterations);
	void ComputeSurfaceInfo();
	void GetAverageSurfaceNormals(const TArray<FVector>& Normals);
	void ComputeClimbingVelocity(float deltaTime);
	bool ShouldStopClimbing();
	void StopClimbing(float deltaTime, int32 Iterations);
	void MoveAlongClimbingSurface(float deltaTime);
	void SnapToClimbingSurface(float deltaTime) const;
	FQuat GetClimbingRotation(float deltaTime) const;
	bool ClimbDownToFloor() const;
	bool CheckFloor(FHitResult& FloorHit) const;
	bool TryClimbUpLedge(float deltaTime, int32 Iterations);
	bool HasReachedEdge(FVector& EdgeLocation) const;
	bool IsLocationWalkable(const FVector& CheckLocation) const;
	bool CanMoveToLedgeClimbLocation(FVector& CharacterStandingLocation) const;
	void StoreClimbDashDirection();
	void UpdateClimbDashState(float deltaTime);
	void StopClimbDashing();
	void AlignClimbDashDirection();

	//Grapple Functions
	void CheckForGrapplePoint();

	//climbing variables
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
		int CollisionCapsuleRadius = 50;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
		int CollisionCapsuleHalfHeight = 72;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "75.0"))
		float MinSurfaceNormalAngle = 25;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "180.0"))
		float MinClimbingAngle = 70;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "180.0"))
		float MaxClimbingAngle = 120;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
		float ClimbingCollisionShrinkAmount = 30;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "300.0"))
		float MaxClimbingSpeed = 120;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "500.0"))
		float MaxClimbingAcceleration = 500;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "600.0"))
		float ClimbingDeceleration = 600;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "20.0"))
		float ClimbingRotationSpeed = 6;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "60.0"))
		float ClimbingSnapSpeed = 4;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
		float DistanceFromSurface = 45;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "500.0"))
		float FloorCheckDistance = 100;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
		float MinClimbDownThreshold = 40;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
		float MinClimbLedgeThreshold = 20;	
	UPROPERTY(Category = "Character Movement: Climbing", EditDefaultsOnly)
		UCurveFloat* ClimbDashCurve;

	//Grapple Variables
	UPROPERTY(Category = "Character Movement: Grappling", EditAnywhere)
		float GrappleDistance = 20;
	UPROPERTY(Category = "Character Movement: Grappling", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1000.0"))
		float MaxGrappleAssistRadius = 1;
	UPROPERTY(Category = "Character Movement: Grappling", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "3.0"))
		float GrappleAssistPrecision = 1;
	UPROPERTY(Category = "Character Movement: Grappling", EditDefaultsOnly)
		TSubclassOf<AActorAnchor> Anchor;

	TArray<FHitResult> CurrentWallHits;
	TArray<USceneComponent*> RaycastLocations;
	FCollisionQueryParams ClimbingQueryParameters;
	bool bWantsToClimb = false;
	FVector CurrentClimbingNormal;
	FVector CurrentClimbingPosition;
	FVector LastEdgeLocation;
	float LedgeTraceDistance = 0;
	FVector ClimbDashDirection;
	bool bWantsToClimbDash = false;
	bool bIsClimbDashing = false;
	float CurrentClimbDashTime;

	bool bCanGrapple = false;
	FVector LastValidGrapplePoint;
	AActor* ActorToGrapple;
	AActorAnchor* CurrentAnchor;
};
