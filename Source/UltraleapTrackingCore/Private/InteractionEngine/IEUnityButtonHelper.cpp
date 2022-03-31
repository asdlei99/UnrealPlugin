/******************************************************************************
 * Copyright (C) Ultraleap, Inc. 2011-2021.                                   *
 *                                                                            *
 * Use subject to the terms of the Apache License 2.0 available at            *
 * http://www.apache.org/licenses/LICENSE-2.0, or another agreement           *
 * between Ultraleap and you, your company or other organization.             *
 ******************************************************************************/


#include "InteractionEngine/IEUnityButtonHelper.h"

// Sets default values for this component's properties
UIEUnityButtonHelper::UIEUnityButtonHelper()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	InterpFinalLocation = false;
	InterpSpeed = 1;
	FrictionCoefficient = 30;
	DragCoefficient = 60;
	SweepOnMove = true;
	UseSeparateTick = false;
	UsePhysicsCallback = false;

	if (UseSeparateTick)
	{
		PrimaryComponentTick.bCanEverTick = true;
		SetTickGroup(TG_PrePhysics);
	}
	else
	{
		PrimaryComponentTick.bCanEverTick = false;
	}
	
}


FVector UIEUnityButtonHelper::ConstrainDepressedLocalPosition(const FVector& InitialLocalPosition, const FVector& LocalPosition)
{
	// Buttons are only allowed to move along their Z axis.
	return FVector(LocalPhysicsPosition.X + LocalPosition.X, InitialLocalPosition.Y, InitialLocalPosition.Z);
}
// when simulating physics, SetRelativePosition is actually in world coords as attachment is broken.
void UIEUnityButtonHelper::SetRelativeLocationAsWorld(
	USceneComponent* Rigidbody, const FVector& RelativeLocation, const FTransform& WorldTransform)
{
	FVector WorldLocation = WorldTransform.TransformPosition(RelativeLocation);
	Rigidbody->SetRelativeLocation(WorldLocation, SweepOnMove);
}
void UIEUnityButtonHelper::SetRelativeRotationAsWorld(
	USceneComponent* Rigidbody, const FRotator& RelativeRotation, const FTransform& WorldTransform)
{
	FRotator WorldRotation = WorldTransform.TransformRotation(FQuat(RelativeRotation)).Rotator();
	Rigidbody->SetRelativeRotation(WorldRotation, SweepOnMove);
}
void UIEUnityButtonHelper::SetPhysicsTickablePrimitive(UIEPhysicsTickStaticMeshComponent* Primitive)
{
	PhysicsTickablePrimitive = Primitive;

	if (Primitive)
	{
		UsePhysicsCallback = true;
		Primitive->IEPhysicsTickNotify.AddDynamic(this, &UIEUnityButtonHelper::OnIEPhysicsNotify);
	}
}
// this can be called outside of the game thread
void UIEUnityButtonHelper::OnIEPhysicsNotify(float DeltaTime, FBodyInstance& BodyInstance)
{
	FixedUpdate(IsGraspedCache, RigidbodyCache, InitialLocalPositionCache, MinMaxHeightCache, RestingHeightCache,
		ParentWorldTransformCache, DeltaTime);
	FixedUpdateCalled = true;
}
void UIEUnityButtonHelper::Update(UPARAM(Ref) bool& IgnoreGrasping, UPARAM(Ref) bool& InitialIgnoreGrasping,
	const bool& IsPrimaryHovered, const bool& IsGrasped, const bool& ControlEnabled, UPARAM(Ref) bool& IgnoreContact,
	UPrimitiveComponent* Rigidbody, const FRotator& InitialLocalRotation, const float PrimaryHoverDistance, const float SpringForce,
	const FVector2D& MinMaxHeight, const float RestingHeight, const float WorldDelta, const FVector& InitialLocalPosition,
	UPARAM(Ref) float& PressedAmount, USceneComponent* PrimaryHoveringController,const FTransform& ParentWorldTransform, const FVector& ContactPoint )
{
	if (!Rigidbody)
	{
		return;
	}
	IsGraspedCache = IsGrasped;
	RigidbodyCache = Rigidbody;
	InitialLocalPositionCache = InitialLocalPosition;
	MinMaxHeightCache = MinMaxHeight;
	RestingHeightCache = RestingHeight;
	ParentWorldTransformCache = ParentWorldTransform;

	if (!FixedUpdateCalled && (UseSeparateTick || UsePhysicsCallback))
	{
		return;
	}
	FixedUpdateCalled = false;
	// Reset our convenience state variables.
	PressedThisFrame = false;
	UnpressedThisFrame = false;

	// Disable collision on this button if it is not the primary hover.
	IgnoreGrasping = InitialIgnoreGrasping ? true : !IsPrimaryHovered && !IsGrasped;
	IgnoreContact = (!IsPrimaryHovered || IsGrasped) || !ControlEnabled;

	// Enforce local rotation (if button is child of non-kinematic rigidbody,
	// this is necessary).
	SetRelativeRotationAsWorld(Rigidbody, InitialLocalRotation, ParentWorldTransform);
	
	// Record and enforce the sliding state from the previous frame.
	if (PrimaryHoverDistance < 0.005f || IsGrasped || IsPressed)
	{
		LocalPhysicsPosition = ConstrainDepressedLocalPosition(InitialLocalPosition,
			ParentWorldTransform.InverseTransformPosition(Rigidbody->GetComponentLocation()) - LocalPhysicsPosition);
	}
	else
	{
		FVector2D LocalSlidePosition = FVector2D(LocalPhysicsPosition.Z, LocalPhysicsPosition.Y);

		LocalPhysicsPosition = ParentWorldTransform.InverseTransformPosition(Rigidbody->GetComponentLocation());

		LocalPhysicsPosition = FVector(LocalPhysicsPosition.X, LocalSlidePosition.Y, LocalSlidePosition.X);
	}
	bool HasVelocity = false;
	// Calculate the physical kinematics of the button in local space
	FVector LocalPhysicsVelocity = ParentWorldTransform.InverseTransformVector(Rigidbody->GetPhysicsLinearVelocity());
	if (IsPressed && IsPrimaryHovered && LastDepressor != nullptr)
	{
		FVector CurLocalDepressorPos = ParentWorldTransform.InverseTransformPosition(ContactPoint/* LastDepressor->GetComponentLocation()*/);
		FVector OrigLocalDepressorPos = ParentWorldTransform.InverseTransformPosition(
			Rigidbody->GetComponentTransform().TransformPosition(LocalDepressorPosition));
		LocalPhysicsVelocity = FVector::BackwardVector * 0.05f;
		LocalPhysicsPosition = ConstrainDepressedLocalPosition(InitialLocalPosition, CurLocalDepressorPos - OrigLocalDepressorPos);
	}
	else if (IsGrasped)
	{
		// Do nothing!
	}
	else
	{
		FVector OriginalLocalVelocity = LocalPhysicsVelocity;
		float Force = FMath::Clamp(
			SpringForce * 10000.0f *
				(InitialLocalPosition.X - FMath::Lerp(MinMaxHeight.X, MinMaxHeight.Y, RestingHeight) - LocalPhysicsPosition.X),
			-100.0f / ParentWorldTransform.GetScale3D().Z, 100.0f / ParentWorldTransform.GetScale3D().Z);
		// Spring force
		LocalPhysicsVelocity +=
			Force *
			WorldDelta * FVector::ForwardVector;

		if (FMath::Abs(LocalPhysicsVelocity.Size()) > 0.001)
		{
			//Debug.Log("LocalPhysicsVelocity step 1 " + localPhysicsVelocity.ToString("F3"));
		}
		// Friction & Drag
		float VelMag = OriginalLocalVelocity.Size();
		float FrictionDragVelocityChangeAmt = 0;
		if (VelMag > 0)
		{
			// Friction force
			float FrictionForceAmt = VelMag * FrictionCoefficient;
			FrictionDragVelocityChangeAmt += WorldDelta * ParentWorldTransform.GetScale3D().Z * FrictionForceAmt;

			// Drag force
			float VelSqrMag = VelMag * VelMag;
			float DragForceAmt = VelSqrMag * DragCoefficient;
			FrictionDragVelocityChangeAmt += WorldDelta * ParentWorldTransform.GetScale3D().Z * DragForceAmt;

			// Apply velocity change, but don't let friction or drag let velocity
			// magnitude cross zero.
			float NewVelMag = FMath::Max<float>(0, VelMag - FrictionDragVelocityChangeAmt);
			LocalPhysicsVelocity = LocalPhysicsVelocity / VelMag * NewVelMag;
		}

		if (FMath::Abs(LocalPhysicsVelocity.Size()) > 0.001)
		{
			//Debug.Log("LocalPhysicsVelocity step 2" + localPhysicsVelocity.ToString("F3"));
			HasVelocity = true;
		}
	}

	// Transform the local physics back into world space
	PhysicsPosition = ParentWorldTransform.TransformPosition(LocalPhysicsPosition);
	PhysicsVelocity = ParentWorldTransform.TransformVector(LocalPhysicsVelocity);

	// Calculate the Depression State of the Button from its Physical Position
	// Set its Graphical Position to be Constrained Physically
	bool OldDepressed = IsPressed;

	// Normalized depression amount.
	PressedAmount = FMath::GetMappedRangeValueClamped(
		FVector2D(InitialLocalPosition.X - MinMaxHeight.X,
										  InitialLocalPosition.X - FMath::Lerp(MinMaxHeight.X, MinMaxHeight.Y, RestingHeight)),
		FVector2D(1, 0), LocalPhysicsPosition.X);

	if (HasVelocity)
	{
		//Debug.Log("localPhysicsPosition " + localPhysicsPosition.ToString("F3") + " " + _pressedAmount.ToString("F3"));
	}
	// If the button is depressed past its limit...
	if (LocalPhysicsPosition.X > InitialLocalPosition.X - MinMaxHeight.X)
	{
		SetRelativeLocationAsWorld(Rigidbody, 
			FVector(InitialLocalPosition.X - MinMaxHeight.X,
			LocalPhysicsPosition.Y, LocalPhysicsPosition.Z), ParentWorldTransform);
		if ((IsPrimaryHovered /* && LastDepressor != nullptr*/) || IsGrasped)
		{
			IsPressed = true;
			LastDepressor = PrimaryHoveringController;
		}
		else
		{
			PhysicsPosition = ParentWorldTransform.TransformPosition(
				FVector(InitialLocalPosition.X - MinMaxHeight.X, LocalPhysicsPosition.Y, LocalPhysicsPosition.Z));
			PhysicsVelocity = PhysicsVelocity * 0.1f;
			IsPressed = false;
			LastDepressor = nullptr;
		}
		// Else if the button is extended past its limit...
	}
	else if (LocalPhysicsPosition.X < InitialLocalPosition.X - MinMaxHeight.Y)
	{
		SetRelativeLocationAsWorld(Rigidbody,
			FVector(InitialLocalPosition.X - MinMaxHeight.Y, LocalPhysicsPosition.Y, LocalPhysicsPosition.Z), ParentWorldTransform);
		PhysicsPosition = Rigidbody->GetComponentLocation();
		IsPressed = false;
		LastDepressor = nullptr;
	}
	else
	{
		// Else, just make the physical and graphical motion of the button match
		SetRelativeLocationAsWorld(Rigidbody, LocalPhysicsPosition, ParentWorldTransform);

		// Allow some hysteresis before setting isDepressed to false.
		if (!IsPressed || !(LocalPhysicsPosition.X > InitialLocalPosition.X - (MinMaxHeight.Y - MinMaxHeight.X) * 0.1F))
		{
			IsPressed = false;
			LastDepressor = nullptr;
		}
	}

	// If our depression state has changed since last time...
	if (IsPressed && !OldDepressed)
	{
		//primaryHoveringController.primaryHoverLocked = true;
		LockedInteractingController = PrimaryHoveringController;

		OnPress();
		PressedThisFrame = true;
	}
	else if (!IsPressed && OldDepressed)
	{
		UnpressedThisFrame = true;
		OnUnpress();

	/* if (!(isGrasped && graspingController == _lockedInteractingController))
		{
			_lockedInteractingController.primaryHoverLocked = false;
		}
			*/
		LastDepressor = nullptr;
	}

	LocalPhysicsPositionConstrained = ParentWorldTransform.InverseTransformPosition(PhysicsPosition);

	if (!UseSeparateTick && !UsePhysicsCallback)
	{
		FixedUpdate(IsGraspedCache, RigidbodyCache, InitialLocalPositionCache, MinMaxHeightCache, RestingHeightCache,
			ParentWorldTransformCache, WorldDelta);
	}

	
}

void UIEUnityButtonHelper::FixedUpdate(const bool IsGrasped, UPrimitiveComponent* Rigidbody, const FVector& InitialLocalPosition,
	const FVector2D& MinMaxHeight, const float RestingHeight, const FTransform& ParentWorldTransform, const float DeltaSeconds)
{
	if (!Rigidbody)
	{
		return;
	}
	if (!IsGrasped && Rigidbody->IsAnyRigidBodyAwake())
	{
		float LocalPhysicsDisplacementPercentage =
			FMath::GetMappedRangeValueClamped(FVector2D(MinMaxHeight.X, MinMaxHeight.Y), FVector2D(0,100), InitialLocalPosition.X - LocalPhysicsPosition.X);

		// Sleep the rigidbody if it's not really moving.
		if (Rigidbody->GetComponentLocation() == PhysicsPosition && PhysicsVelocity == FVector::ZeroVector &&
			FMath::Abs(LocalPhysicsDisplacementPercentage - RestingHeight) < 0.01F)
		{
			Rigidbody->PutAllRigidBodiesToSleep();
		}
		else
		{
			// Otherwise reset the body's position to where it was last time PhysX
			// looked at it.
			if (PhysicsVelocity.ContainsNaN())
			{
				PhysicsVelocity = FVector::ZeroVector;
			}
			FVector WorldLocation = ParentWorldTransform.TransformPosition(LocalPhysicsPositionConstrained) + AdditionalDelta;
			// when constraining, we don't want to sweep as this allows the hand to push the button off axis.
			FVector CurrentWorldLocation = Rigidbody->GetComponentLocation();
			if (InterpFinalLocation)
			{
				WorldLocation = FMath::VInterpTo(CurrentWorldLocation, WorldLocation, DeltaSeconds, InterpSpeed);
			}
			Rigidbody->SetWorldLocation(WorldLocation, false);
			Rigidbody->SetPhysicsLinearVelocity(PhysicsVelocity);
		}
	}
}
void UIEUnityButtonHelper::TickComponent(
	float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!RigidbodyCache)
	{
		return;
	}
	
	FixedUpdate(IsGraspedCache, RigidbodyCache, InitialLocalPositionCache, MinMaxHeightCache, RestingHeightCache,
				ParentWorldTransformCache, GetWorld()->DeltaTimeSeconds);
	FixedUpdateCalled = true;
}
void UIEUnityButtonHelper::SubstepTick(float DeltaTime, FBodyInstance* BodyInstance)
{

}
void UIEUnityButtonHelper::DoPhysics(float DeltaTime, bool InSubstep)
{

}