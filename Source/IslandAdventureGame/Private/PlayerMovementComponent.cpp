// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerMovementComponent.h"
#include <Kismet/KismetSystemLibrary.h>
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_Climbing      UMETA(DisplayName = "Climbing"),
	CMOVE_Grappling		UMETA(DisplayName = "Grappling"),
	CMOVE_MAX			UMETA(Hidden),
};

void UPlayerMovementComponent::TryGrapple()
{
	if (!bCanGrapple)
		return;

	CurrentAnchor = GetWorld()->SpawnActor<AActorAnchor>(Anchor);
	CurrentAnchor->InitAnchor(LastValidGrapplePoint, ActorToGrapple);
}

void UPlayerMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	//ignores the character for the sweep check
	ClimbingQueryParameters.AddIgnoredActor(GetOwner());
	RaycastLocations = CharacterOwner->GetCapsuleComponent()->GetAttachChildren();
	while (RaycastLocations.Num() > 4)
	{
		RaycastLocations.RemoveAt(0);
	}
	/*for (USceneComponent* component : RaycastLocations)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10, FColor::White, TEXT(""+(component->GetName())+ " " + (component->GetComponentLocation().ToString())));
	}*/
}

void UPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SweepAndStoreWallHits();
	CheckForGrapplePoint();
}

void UPlayerMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	if (bWantsToClimb)
	{
		SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_Climbing);
	}

	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

void UPlayerMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		Capsule->SetCapsuleHalfHeight(Capsule->GetUnscaledCapsuleHalfHeight() - ClimbingCollisionShrinkAmount);

		StopMovementImmediately();
	}

	const bool bWasClimbing = PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Climbing;
	if (bWasClimbing)
	{
		bOrientRotationToMovement = true;

		const FRotator StandRotation = FRotator(0, UpdatedComponent->GetComponentRotation().Yaw, 0);
		UpdatedComponent->SetRelativeRotation(StandRotation);

		UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		Capsule->SetCapsuleHalfHeight(Capsule->GetUnscaledCapsuleHalfHeight() + ClimbingCollisionShrinkAmount);

		StopMovementImmediately();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UPlayerMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (CustomMovementMode == ECustomMovementMode::CMOVE_Climbing)
	{
		PhysClimbing(deltaTime, Iterations);
	}

	Super::PhysCustom(deltaTime, Iterations);
}

float UPlayerMovementComponent::GetMaxSpeed() const
{
	return IsClimbing() ? MaxClimbingSpeed : Super::GetMaxSpeed();
}

float UPlayerMovementComponent::GetMaxAcceleration() const
{
	return IsClimbing() ? MaxClimbingAcceleration : Super::GetMaxAcceleration();
}

void UPlayerMovementComponent::TryClimbing()
{
	bWantsToClimb = CanStartClimbing();
}

void UPlayerMovementComponent::CancelClimbing()
{
	bWantsToClimb = false;
}

bool UPlayerMovementComponent::IsClimbing() const
{
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_Climbing;
}

FVector UPlayerMovementComponent::GetClimbSurfaceNormal() const
{
	return CurrentClimbingNormal;
}

void UPlayerMovementComponent::TryClimbDashing()
{
	if (!IsClimbing())
		return;

	if (!(ClimbDashCurve && !bWantsToClimbDash))
	{
		return;
	}

	bWantsToClimbDash = true;
	bIsClimbDashing = true;
	CurrentClimbDashTime = 0;
	StoreClimbDashDirection();
}

void UPlayerMovementComponent::SweepAndStoreWallHits()
{
	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(CollisionCapsuleRadius, CollisionCapsuleHalfHeight);

	//gets the current forward vector of the current component which is called UpdatedComponent for some reason
	const FVector StartOffset = (UpdatedComponent->GetForwardVector() * 30);

	//Avoid using the same Start/End location for a Sweep, as it doesn't trigger hits on landscapes
	const FVector SweepStartPosition = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector SweepEndPosition = SweepStartPosition + UpdatedComponent->GetForwardVector() * 50;

	//do a sweep and store them
	TArray<FHitResult> Hits;
	const bool HitWall = GetWorld()->SweepMultiByChannel(Hits, SweepStartPosition, SweepEndPosition, CharacterOwner->GetActorQuat(), ECC_WorldStatic, CollisionShape, ClimbingQueryParameters);
	if (HitWall)
	{
		CurrentWallHits = Hits;
		//draws debug hits
		UKismetSystemLibrary::DrawDebugCapsule(GetWorld(), SweepStartPosition, CollisionCapsuleHalfHeight, CollisionCapsuleRadius, CharacterOwner->GetActorQuat().Rotator());
		for (FHitResult& Hit : CurrentWallHits)
		{
			UKismetSystemLibrary::DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 5.f, 12, FLinearColor::Blue, 0, 10.f);
		}
	}
	else
	{
		CurrentWallHits.Reset();
	}
}

bool UPlayerMovementComponent::CanStartClimbing()
{
	for (FHitResult& Hit : CurrentWallHits)
	{
		const FVector WallNormal = Hit.Normal.GetSafeNormal2D();

		//gets the horizontal normal and flattens it on the z axis in order to get the angle of the surface
		const float HorizontalDot = FVector::DotProduct(UpdatedComponent->GetForwardVector(), -WallNormal);//checks to see if the player is looking at wall
		const float VerticalDot = FVector::DotProduct(Hit.Normal, WallNormal);//checks how steep the wall is to make the eye trace longer for steeper inclines

		const float angle = FMath::RadiansToDegrees(FMath::Acos(HorizontalDot));//the angle from where the player is facing and the wall

		if (angle <= MinSurfaceNormalAngle && IsClimbableSurface(WallNormal) && IsFacingSurface(VerticalDot))
		{
			LedgeTraceDistance = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius() * 3.5f;
			return true;
		}

	}

	return false;
}

bool UPlayerMovementComponent::EyeHeightTrace(const float TraceDistance, FVector& TraceHitLocation) const
{
	FHitResult UpperEdgeHit;

	const float BaseEyeHeight = GetCharacterOwner()->BaseEyeHeight;
	const float EyeHeightOffset = IsClimbing() ? BaseEyeHeight - (ClimbingCollisionShrinkAmount/3.0f) : BaseEyeHeight;	

	const FVector StartingPosition = UpdatedComponent->GetComponentLocation() + (UpdatedComponent->GetUpVector() * EyeHeightOffset);
	const FVector EndPosition = StartingPosition + (UpdatedComponent->GetForwardVector() * TraceDistance);

	bool bHitSomething = GetWorld()->LineTraceSingleByChannel(UpperEdgeHit, StartingPosition, EndPosition, ECC_WorldStatic, ClimbingQueryParameters);
	//UKismetSystemLibrary::DrawDebugLine(GetWorld(), StartingPosition, EndPosition, FLinearColor::Yellow);

	if (bHitSomething)
	{
		if (IsClimbing())
		{
			TraceHitLocation = UpperEdgeHit.ImpactPoint;
		}
		//UKismetSystemLibrary::DrawDebugSphere(GetWorld(), UpperEdgeHit.ImpactPoint, 5, 12, FLinearColor::Green);
	}

	return bHitSomething;
}

bool UPlayerMovementComponent::IsFacingSurface(const float Steepness) const
{
	constexpr float BaseLength = 80;
	const float SteepnessMultiplier = 1 + (1 - Steepness) * 4;
	FVector _;
	return EyeHeightTrace(BaseLength * SteepnessMultiplier, _);
}

bool UPlayerMovementComponent::IsClimbableSurface(const FVector WallNormal) const
{
	float WallDotProduct = FVector::DotProduct(FVector::UpVector, WallNormal);
	float WallAngle = FMath::RadiansToDegrees(FMath::Acos(WallDotProduct));

	/*GEngine->AddOnScreenDebugMessage(-1, 30, FColor::Red, TEXT("------------------------------------------------------"));
	GEngine->AddOnScreenDebugMessage(-1, 30, FColor::Red, FString::Printf(TEXT("Wall Angle: %f"), WallAngle));
	GEngine->AddOnScreenDebugMessage(-1, 30, FColor::Red, FString::Printf(TEXT("Min Angle: %f\n Max Angle:%f"), MinClimbingAngle,MaxClimbingAngle));*/
	bool bIsClimbableSurface = WallAngle > MinClimbingAngle && WallAngle < MaxClimbingAngle;
	//GEngine->AddOnScreenDebugMessage(-1, 30, FColor::Red, TEXT("IsClimbable?" + bIsClimbableSurface ? "true" : "false"));

	return bIsClimbableSurface;
}

void UPlayerMovementComponent::PhysClimbing(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
		return;

	ComputeSurfaceInfo();

	if (ShouldStopClimbing() || ClimbDownToFloor())
	{
		StopClimbing(deltaTime, Iterations);
		return;
	}

	UpdateClimbDashState(deltaTime);
	ComputeClimbingVelocity(deltaTime);

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();

	MoveAlongClimbingSurface(deltaTime);

	if (TryClimbUpLedge(deltaTime, Iterations))
	{
		return;
	}
	

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	SnapToClimbingSurface(deltaTime);
}

void UPlayerMovementComponent::ComputeSurfaceInfo()
{
	CurrentClimbingNormal = FVector::ZeroVector;
	CurrentClimbingPosition = FVector::ZeroVector;

	if (CurrentWallHits.IsEmpty())
		return;

	const FVector StartPosition = UpdatedComponent->GetComponentLocation();
	const FCollisionShape CollisionSphere = FCollisionShape::MakeSphere(10);
	TArray<FVector> Normals;
	for (const FHitResult& WallHit : CurrentWallHits)
	{
		const FVector EndPosition = StartPosition + (WallHit.ImpactPoint - StartPosition).GetSafeNormal() * 120;

		FHitResult AssistHit;
		GetWorld()->SweepSingleByChannel(AssistHit, StartPosition, EndPosition, FQuat::Identity, ECC_WorldStatic, CollisionSphere, ClimbingQueryParameters);
		CurrentClimbingPosition += AssistHit.ImpactPoint;
		Normals.Add(AssistHit.Normal);
	}
	CurrentClimbingPosition /= CurrentWallHits.Num();
	if (CurrentAnchor)
	{
		CurrentAnchor->UpdateAnchorLocation(CurrentClimbingPosition);
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Red, TEXT("CurrentAnchor not set while climbing, is this intended?"));
	}
	GetAverageSurfaceNormals(Normals);
}

void UPlayerMovementComponent::GetAverageSurfaceNormals(const TArray<FVector>& Normals)
{
	
	for (int count = 0; count < Normals.Num(); count++)
	{
		CurrentClimbingNormal += Normals[count];
	}

	//get the additional 4 triangles from the 4 sphere points in Raycast locations
	TArray<FVector> HitPoints;
	for (int count = 0; count < RaycastLocations.Num(); count++)
	{
		FHitResult Hit;
		const FVector StartLocation = RaycastLocations[count]->GetComponentLocation();
		const FVector EndLocation = StartLocation + UpdatedComponent->GetForwardVector() * 100;
		const FCollisionShape CollisionSphere = FCollisionShape::MakeSphere(10);
		if (GetWorld()->SweepSingleByChannel(Hit,StartLocation,EndLocation, FQuat::Identity,ECC_WorldStatic,CollisionSphere,ClimbingQueryParameters))
		{
			HitPoints.Add(Hit.ImpactPoint);
			UKismetSystemLibrary::DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::White);
			UKismetSystemLibrary::DrawDebugPoint(GetWorld(), Hit.ImpactPoint,10,FColor::Red);
			UKismetSystemLibrary::DrawDebugSphere(GetWorld(), Hit.ImpactPoint + Hit.ImpactNormal, 10);
		}
	}

	if (HitPoints.Num() == 4)
	{
		FVector PointA = HitPoints[0];
		FVector PointB = HitPoints[1];
		FVector PointC = HitPoints[2];
		FVector PointD = HitPoints[3];

		//Triangle ABC
		FVector FirstTriangleNormal;
		FVector FirstSideA = PointB - PointA;
		FVector FirstSideB = PointC - PointA;
		FirstTriangleNormal = FVector::CrossProduct(FirstSideA, FirstSideB);
		FVector CenterPoint = (PointA + PointB + PointC) / 3;
		UKismetSystemLibrary::DrawDebugLine(GetWorld(), CenterPoint, CenterPoint + (FirstTriangleNormal * 10), FColor::Blue);

		//Triangle ABD
		FVector SecondTriangleNormal;
		FVector SecondSideA = PointB - PointA;
		FVector SecondSideB = PointD - PointA;
		SecondTriangleNormal = FVector::CrossProduct(FirstSideA, FirstSideB);
		CenterPoint = (PointA + PointB + PointD) / 3;
		UKismetSystemLibrary::DrawDebugLine(GetWorld(), CenterPoint, CenterPoint + (SecondTriangleNormal * 10), FColor::Blue);

		//Triangle ADC
		FVector ThirdTriangleNormal;
		FVector ThirdSideA = PointD - PointA;
		FVector ThirdSideB = PointC - PointA;
		ThirdTriangleNormal = FVector::CrossProduct(ThirdSideA, ThirdSideB);
		CenterPoint = (PointA + PointD + PointC) / 3;
		UKismetSystemLibrary::DrawDebugLine(GetWorld(), CenterPoint, CenterPoint + (ThirdTriangleNormal * 10), FColor::Blue);

		//Triangle BDC
		FVector FourthTriangleNormal;
		FVector FourthSideA = PointD - PointB;
		FVector FourthSideB = PointC - PointB;
		FourthTriangleNormal = FVector::CrossProduct(FirstSideA, FirstSideB);
		CenterPoint = (PointB + PointD + PointC) / 3;
		UKismetSystemLibrary::DrawDebugLine(GetWorld(), CenterPoint, CenterPoint + (FourthTriangleNormal * 10), FColor::Blue);

		CurrentClimbingNormal += FirstTriangleNormal + SecondTriangleNormal;
	}

	CurrentClimbingNormal = CurrentClimbingNormal.GetSafeNormal();
	
}

void UPlayerMovementComponent::ComputeClimbingVelocity(float deltaTime)
{
	RestorePreAdditiveRootMotionVelocity();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		if (bWantsToClimbDash)
		{
			AlignClimbDashDirection();

			const float CurrentCurveSpeed = ClimbDashCurve->GetFloatValue(CurrentClimbDashTime);
			UE_LOG(LogTemp, Log, TEXT("CurrentCurveSpeed: %f"),CurrentCurveSpeed)
			Velocity = ClimbDashDirection * CurrentCurveSpeed;
			UE_LOG(LogTemp, Log, TEXT("Velocity: %s"), *(Velocity.ToString()));
		}
		else
		{
			constexpr float Friction = 0.f;
			constexpr bool bFluid = false;
			CalcVelocity(deltaTime, Friction, bFluid, ClimbingDeceleration);
		}
	}

	ApplyRootMotionToVelocity(deltaTime);
}

bool UPlayerMovementComponent::ShouldStopClimbing()
{
	return !IsClimbableSurface(CurrentClimbingNormal) || !bWantsToClimb;
}

void UPlayerMovementComponent::StopClimbing(float deltaTime, int32 Iterations)
{
	StopClimbDashing();
	bWantsToClimb = false;
	SetMovementMode(EMovementMode::MOVE_Falling);
	StartNewPhysics(deltaTime, Iterations);
}

void UPlayerMovementComponent::MoveAlongClimbingSurface(float deltaTime)
{
	const FVector NormalizedVelocity = Velocity * deltaTime;

	FHitResult Hit(1.f);

	SafeMoveUpdatedComponent(NormalizedVelocity, GetClimbingRotation(deltaTime), true, Hit);

	if (Hit.Time < 1.f)//time is just the distance of the raycast or movement normalized
	{
		HandleImpact(Hit, deltaTime, NormalizedVelocity);
		SlideAlongSurface(NormalizedVelocity, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}
}

void UPlayerMovementComponent::SnapToClimbingSurface(float deltaTime) const
{
	const FVector Forward = UpdatedComponent->GetForwardVector();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();

	const FVector ForwardDifference = (CurrentClimbingPosition - Location).ProjectOnTo(Forward);
	const FVector Offset = -CurrentClimbingNormal * (ForwardDifference.Length() - DistanceFromSurface);

	constexpr bool bSweep = true;
	const float SnapSpeed = ClimbingSnapSpeed * FMath::Max(1, Velocity.Length() / MaxClimbingSpeed);
	UpdatedComponent->MoveComponent(Offset * SnapSpeed * deltaTime, Rotation, bSweep);
}

FQuat UPlayerMovementComponent::GetClimbingRotation(float deltaTime) const
{
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const FQuat TargetRotation = FRotationMatrix::MakeFromX(-CurrentClimbingNormal).ToQuat();

	const float RotationSpeed = ClimbingRotationSpeed * FMath::Max(1,Velocity.Length()/MaxClimbingSpeed);

	return FMath::QInterpTo(CurrentRotation, TargetRotation, deltaTime, RotationSpeed);
}

bool UPlayerMovementComponent::ClimbDownToFloor() const
{
	FHitResult FloorHit;
	//checks to see if there is anything there
	if (!CheckFloor(FloorHit))
	{
		return false;
	}

	const bool bOnWalkableFloor = FloorHit.Normal.Z > GetWalkableFloorZ();

	const float DownSpeed = FVector::DotProduct(Velocity, -FloorHit.Normal);
	const bool bIsMovingTowardsFloor = DownSpeed >= MinClimbDownThreshold && bOnWalkableFloor;
	const bool bIsClimbingFloor = CurrentClimbingNormal.Z > GetWalkableFloorZ();

	return bIsMovingTowardsFloor || (bIsClimbingFloor && bOnWalkableFloor);
}

bool UPlayerMovementComponent::CheckFloor(FHitResult& FloorHit) const
{
	const FVector StartLocation = UpdatedComponent->GetComponentLocation();
	const FVector EndLocation = StartLocation + FVector::DownVector * FloorCheckDistance;

	return GetWorld()->LineTraceSingleByChannel(FloorHit,StartLocation, EndLocation, ECC_WorldStatic, ClimbingQueryParameters);
}

bool UPlayerMovementComponent::TryClimbUpLedge(float deltaTime, int32 Iterations)
{
	const float UpSpeed = FVector::DotProduct(Velocity, UpdatedComponent->GetUpVector());
	const bool bIsMovingUp = UpSpeed >= MinClimbLedgeThreshold;

	//store checks here
	FVector CharacterStandingLocation;

	bool bHasReachedEdge = HasReachedEdge(LastEdgeLocation);
	bool bCanMoveToLedgeClimbLocation = CanMoveToLedgeClimbLocation(CharacterStandingLocation);
	if (bIsMovingUp && bCanMoveToLedgeClimbLocation && bHasReachedEdge)	
	{
		//place character upright and on the location that we checked
		StopClimbing(deltaTime,Iterations);
		const FRotator StandRotation = FRotator(0, UpdatedComponent->GetComponentRotation().Yaw, 0);
		UpdatedComponent->SetRelativeRotation(StandRotation);
		UpdatedComponent->SetRelativeLocation(CharacterStandingLocation);
		return true;
	}

	return false;
}

bool UPlayerMovementComponent::HasReachedEdge(FVector& EdgeLocation) const
{
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float TraceDistance = LedgeTraceDistance;
	const bool bHasHitEdge = !EyeHeightTrace(TraceDistance,EdgeLocation);
	
	return bHasHitEdge;
}

bool UPlayerMovementComponent::IsLocationWalkable(const FVector& CheckLocation) const
{
	const FVector CheckEnd = CheckLocation + (FVector::DownVector * 350.f);

	FHitResult LedgeHit;
	const bool bHitLedgeGround = GetWorld()->LineTraceSingleByChannel(LedgeHit, CheckLocation, CheckEnd, ECC_WorldStatic, ClimbingQueryParameters);

	FColor ResultColor = bHitLedgeGround && LedgeHit.Normal.Z >= GetWalkableFloorZ() ? FColor::Green : FColor::Red;
	UKismetSystemLibrary::DrawDebugLine(GetWorld(), CheckLocation, CheckEnd, ResultColor);
	if (bHitLedgeGround)
	{
		UKismetSystemLibrary::DrawDebugSphere(GetWorld(), LedgeHit.ImpactPoint, 10);
	}

	return bHitLedgeGround && LedgeHit.Normal.Z >= GetWalkableFloorZ();
}

bool UPlayerMovementComponent::CanMoveToLedgeClimbLocation(FVector& CharacterStandingLocation) const
{
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const FVector VerticalOffset = FVector::UpVector * (Capsule->GetUnscaledCapsuleHalfHeight() + 10);
	const FVector HorizontalOffset = UpdatedComponent->GetForwardVector() * (Capsule->GetUnscaledCapsuleRadius()+ 20);
	
	CharacterStandingLocation = LastEdgeLocation + HorizontalOffset + VerticalOffset;

	if (!IsLocationWalkable(CharacterStandingLocation))
	{
		return false;
	}

	FHitResult CapsuleHit;

	const FVector CapsuleStartLocation = CharacterStandingLocation - HorizontalOffset;
	const bool bClimbingLocationBlocked = GetWorld()->SweepSingleByChannel(CapsuleHit, CapsuleStartLocation, CharacterStandingLocation, FQuat::Identity, ECC_WorldStatic, Capsule->GetCollisionShape(), ClimbingQueryParameters);

	//Debug Drawing Capsule Cast
	FColor ResultColor = bClimbingLocationBlocked ? FColor::Red : FColor::Green;
	UKismetSystemLibrary::DrawDebugCapsule(GetWorld(), CharacterStandingLocation, Capsule->GetScaledCapsuleHalfHeight(), Capsule->GetScaledCapsuleRadius(), FRotator::ZeroRotator, ResultColor);

	return !bClimbingLocationBlocked;
}

void UPlayerMovementComponent::StoreClimbDashDirection()
{
	ClimbDashDirection = UpdatedComponent->GetUpVector();

	const float AccelerationThreshold = MaxClimbingAcceleration / 10;
	if (Acceleration.Length() > AccelerationThreshold)
	{
		ClimbDashDirection = Acceleration.GetSafeNormal();
	}
}

void UPlayerMovementComponent::UpdateClimbDashState(float deltaTime)
{
	if (!bWantsToClimbDash)
		return;

	CurrentClimbDashTime += deltaTime;

	//better to cache it when dash starts
	float MinTime, MaxTime;
	ClimbDashCurve->GetTimeRange(MinTime, MaxTime);

	if (CurrentClimbDashTime >= MaxTime)
	{
		StopClimbDashing();
	}
}

void UPlayerMovementComponent::StopClimbDashing()
{
	bWantsToClimbDash = false;
	bIsClimbDashing = false;
	CurrentClimbDashTime = 0;
}

void UPlayerMovementComponent::AlignClimbDashDirection()
{
	const FVector HorizontalSurfaceNormal = GetClimbSurfaceNormal().GetSafeNormal2D();

	ClimbDashDirection = FVector::VectorPlaneProject(ClimbDashDirection, HorizontalSurfaceNormal);
}

void UPlayerMovementComponent::CheckForGrapplePoint()
{
	bCanGrapple = false;
	//do two casts, a line cast first to see if the player is directly aiming at something
	//second, a sphere cast to give a little assistance in case they miss directly
	APlayerCameraManager* Camera = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	FVector RaycastDirection = Camera->GetActorForwardVector();
	FVector RaycastStartingPoint = Camera->GetCameraLocation() + (RaycastDirection * 600);
	FVector RaycastEndingPoint = RaycastStartingPoint + (RaycastDirection * GrappleDistance);
	
	
	//this takes advantage of short circuiting so if the line trace fails it will try the sphere trace.
	//if the line trace succeeds the sphere trace doesnt' happen
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, RaycastStartingPoint, RaycastEndingPoint, ECC_WorldStatic, ClimbingQueryParameters))
	{
		bCanGrapple = true;
	}

	if (!bCanGrapple)
	{
		for (float currentRadius = 0.1f; currentRadius < MaxGrappleAssistRadius; currentRadius += GrappleAssistPrecision)
		{
			const FCollisionShape CollisionSphere = FCollisionShape::MakeSphere(currentRadius);
			if (GetWorld()->SweepSingleByChannel(Hit, RaycastStartingPoint, RaycastEndingPoint - (RaycastDirection * currentRadius), FQuat::Identity, ECC_WorldStatic, CollisionSphere, ClimbingQueryParameters))
			{
				bCanGrapple = true;
				break;
			}
		}
	}
	if (!bCanGrapple)
		return;

	LastValidGrapplePoint = Hit.ImpactPoint;
	ActorToGrapple = Hit.GetActor();
	UKismetSystemLibrary::DrawDebugLine(GetWorld(), UpdatedComponent->GetComponentLocation(), Hit.ImpactPoint, FColor::Green);
	
	//if all fails, get out
	
}
