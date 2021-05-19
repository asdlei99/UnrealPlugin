// Copyright 1998-2020 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BodyStateEnums.h"
#include "Runtime/Engine/Classes/Animation/AnimInstance.h"
#include "Skeleton/BodyStateSkeleton.h"

#include "BodyStateAnimInstance.generated.h"

USTRUCT(BlueprintType)
struct BODYSTATE_API FBodyStateIndexedBone
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Indexed Bone")
	FName BoneName;

	UPROPERTY(BlueprintReadWrite, Category = "Indexed Bone")
	int32 ParentIndex;

	UPROPERTY(BlueprintReadWrite, Category = "Indexed Bone")
	int32 Index;

	UPROPERTY(BlueprintReadWrite, Category = "Indexed Bone")
	TArray<int32> Children;

	FBodyStateIndexedBone()
	{
		ParentIndex = -1;
		Index = -1;
		Children.Empty();
	}
	FORCEINLINE bool operator<(const FBodyStateIndexedBone& other) const;
};

struct FBodyStateIndexedBoneList
{
	TArray<FBodyStateIndexedBone> Bones;
	TArray<FBodyStateIndexedBone> SortedBones;
	int32 RootBoneIndex;

	// run after filling our index
	TArray<int32> FindBoneWithChildCount(int32 Count);
	void SetFromRefSkeleton(const FReferenceSkeleton& RefSkeleton, bool SortBones);
	int32 LongestChildTraverseForBone(int32 Bone);

	FBodyStateIndexedBoneList()
	{
		RootBoneIndex = 0;
	}
	int32 TreeIndexFromSortedIndex(int32 SortedIndex);
};

// C++ only struct used for cached bone lookup
struct CachedBoneLink
{
	FBoneReference MeshBone;
	UBodyStateBone* BSBone;
};

/** Required struct since 4.17 to expose hotlinked mesh bone references*/
USTRUCT(BlueprintType)
struct FBPBoneReference
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = BoneName)
	FBoneReference MeshBone;
};

USTRUCT(BlueprintType)
struct FMappedBoneAnimData
{
	GENERATED_BODY()

	/** Whether the mesh should deform to match the tracked data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Anim Struct")
	bool bShouldDeformMesh;

	/** List of tags required by the tracking solution for this animation to use that data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Anim Struct")
	TArray<FString> TrackingTagLimit;

	/** Offset rotation base applied before given rotation (will rotate input) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance")
	FRotator PreBaseRotation;

	/** Transform applied after rotation changes to all bones in map. Consider this an offset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance")
	FTransform OffsetTransform;

	/** Matching list of body state bone keys mapped to local mesh bone names */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bone Anim Struct")
	TMap<EBodyStateBasicBoneType, FBPBoneReference> BoneMap;

	/** Skeleton driving mapped data */
	UPROPERTY(BlueprintReadWrite, Category = "Bone Anim Struct")
	class UBodyStateSkeleton* BodyStateSkeleton;

	/** Skeleton driving mapped data */
	UPROPERTY(BlueprintReadWrite, Category = "Bone Anim Struct")
	float ElbowLength;

	UPROPERTY(BlueprintReadWrite, Category = "Bone Anim Struct")
	bool IsFlippedByScale;

	/** auto calculated rotation to correct/normalize model rotation*/
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "BS Anim Instance - Auto Map")
	FQuat AutoCorrectRotation;
	// Data structure containing a parent -> child ordered bone list
	TArray<CachedBoneLink> CachedBoneList;

	FMappedBoneAnimData()
	{
		bShouldDeformMesh = true;
		OffsetTransform.SetScale3D(FVector(1.f));
		PreBaseRotation = FRotator(ForceInitToZero);
		TrackingTagLimit.Empty();
		AutoCorrectRotation = FQuat(FRotator(ForceInitToZero));
	}

	void SyncCachedList(const USkeleton* LinkedSkeleton);

	bool BoneHasValidTags(const UBodyStateBone* QueryBone);
	bool SkeletonHasValidTags();
};
USTRUCT(BlueprintType)
struct FBoneSearchNames
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	TArray<FString> ArmNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	TArray<FString> WristNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	TArray<FString> ThumbNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	TArray<FString> IndexNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	TArray<FString> MiddleNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	TArray<FString> RingNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	TArray<FString> PinkyNames;

	FBoneSearchNames()
	{
		ArmNames = {"elbow", "upperArm"};
		WristNames = {"wrist", "hand", "palm"};
		ThumbNames = {"thumb"};
		IndexNames = {"index"};
		MiddleNames = {"middle"};
		RingNames = {"ring"};
		PinkyNames = {"pinky", "little"};
	}
};
UCLASS(transient, Blueprintable, hideCategories = AnimInstance, BlueprintType)
class UBodyStateAnimInstance : public UAnimInstance
{
public:
	GENERATED_UCLASS_BODY()

	/** Toggle to freeze the tracking at current state. Useful for debugging your anim instance*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BS Anim Instance - Debug")
	bool bFreezeTracking;

	/** Whether the anim instance should autodetect and fill the bonemap on anim init*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	bool bAutoDetectBoneMapAtInit;

	/** Whether the anim instance should map the skeleton rotation on auto map*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	bool bDetectHandRotationDuringAutoMapping;

	/** Whether the anim instance should map the skeleton rotation on auto map*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	bool bIncludeMetaCarpels;

	/** Sort the bone names alphabetically when auto mapping rather than by bone order*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	bool bUseSortedBoneNames;

	/** Auto detection names (e.g. index thumb etc.)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	FBoneSearchNames SearchNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance - Auto Map")
	EBodyStateAutoRigType AutoMapTarget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance")
	int32 DefaultBodyStateIndex;

	/** Skeleton driving our data */
	UPROPERTY(BlueprintReadWrite, Category = "Bone Anim Struct")
	class UBodyStateSkeleton* BodyStateSkeleton;

	// UFUNCTION(BlueprintCallable, Category = "BS Anim Instance")
	TMap<EBodyStateBasicBoneType, FBPBoneReference> AutoDetectHandBones(
		USkeletalMeshComponent* Component, EBodyStateAutoRigType RigTargetType = EBodyStateAutoRigType::HAND_LEFT);

	/** Adjust rotation by currently defines offset base rotators */
	UFUNCTION(BlueprintPure, Category = "BS Anim Instance")
	FRotator AdjustRotationByMapBasis(const FRotator& InRotator, const FMappedBoneAnimData& ForMap);

	UFUNCTION(BlueprintPure, Category = "BS Anim Instance")
	FVector AdjustPositionByMapBasis(const FVector& InPosition, const FMappedBoneAnimData& ForMap);

	/** Link given mesh bone with body state bone enum. */
	UFUNCTION(BlueprintCallable, Category = "BS Anim Instance")
	void AddBSBoneToMeshBoneLink(UPARAM(ref) FMappedBoneAnimData& InMappedBoneData, EBodyStateBasicBoneType BSBone, FName MeshBone);

	/** Remove a link. Useful when e.g. autorigging gets 80% there but you need to remove a bone. */
	UFUNCTION(BlueprintCallable, Category = "BS Anim Instance")
	void RemoveBSBoneLink(UPARAM(ref) FMappedBoneAnimData& InMappedBoneData, EBodyStateBasicBoneType BSBone);

	UFUNCTION(BlueprintCallable, Category = "BS Anim Instance")
	void SetAnimSkeleton(UBodyStateSkeleton* InSkeleton);

	/** Struct containing all variables needed at anim node time */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "BS Anim Instance")
	TArray<FMappedBoneAnimData> MappedBoneList;

	UFUNCTION(BlueprintPure, Category = "BS Anim Instance")
	FString BoneMapSummary();

	// Manual sync
	UFUNCTION(BlueprintCallable, Category = "BS Anim Instance")
	void SyncMappedBoneDataCache(UPARAM(ref) FMappedBoneAnimData& InMappedBoneData);

	UFUNCTION(BlueprintCallable, Category = "BS Anim Instance")
	static FName GetBoneNameFromRef(const FBPBoneReference& BoneRef);

protected:
	// traverse a bone index node until you hit -1, count the hops
	int32 TraverseLengthForIndex(int32 Index);
	void AddFingerToMap(EBodyStateBasicBoneType BoneType, int32 BoneIndex,
		TMap<EBodyStateBasicBoneType, FBodyStateIndexedBone>& BoneMap, int32 InBonesPerFinger = 3);

	static void AddEmptyFingerToMap(EBodyStateBasicBoneType BoneType, TMap<EBodyStateBasicBoneType, FBodyStateIndexedBone>& BoneMap,
		int32 InBonesPerFinger = 3);

	// Internal Map with parent information
	FBodyStateIndexedBoneList BoneLookupList;
	TMap<EBodyStateBasicBoneType, FBodyStateIndexedBone> IndexedBoneMap;
	TMap<EBodyStateBasicBoneType, FBPBoneReference> ToBoneReferenceMap(
		TMap<EBodyStateBasicBoneType, FBodyStateIndexedBone> InIndexedMap);
	TMap<EBodyStateBasicBoneType, FBodyStateIndexedBone> AutoDetectHandIndexedBones(
		USkeletalMeshComponent* Component, EBodyStateAutoRigType RigTargetType = EBodyStateAutoRigType::HAND_LEFT);

	FRotator EstimateAutoMapRotation(FMappedBoneAnimData& ForMap, const EBodyStateAutoRigType RigTargetType);
	float CalculateElbowLength(const FMappedBoneAnimData& ForMap, const EBodyStateAutoRigType RigTargetType);
	FTransform GetTransformFromBoneEnum(const FMappedBoneAnimData& ForMap, const EBodyStateBasicBoneType BoneType,
		const TArray<FName>& Names, const TArray<FNodeItem>& NodeItems);

	void AutoMapBoneDataForRigType(FMappedBoneAnimData& ForMap, EBodyStateAutoRigType RigTargetType);
	TArray<int32> SelectBones(const TArray<FString>& Definitions);
	int32 SelectFirstBone(const TArray<FString>& Definitions);

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	static void CreateEmptyBoneMap(
		TMap<EBodyStateBasicBoneType, FBodyStateIndexedBone>& AutoBoneMap, const EBodyStateAutoRigType HandType);

	static const int32 InvalidBone = -1;
	static const int32 NoMetaCarpelsFingerBoneCount = 3;
};