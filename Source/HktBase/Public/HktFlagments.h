#pragma once

#include "CoreMinimal.h"
#include "HktFlagments.generated.h"
  
// 모든 패킷 구조체의 기반이 될 기본 구조체 (선택사항이지만 좋은 습관입니다)
USTRUCT(BlueprintType)
struct FHktFlagmentBase
{
    GENERATED_BODY()

    virtual ~FHktFlagmentBase() = default;

    virtual FPrimaryAssetId GetViewAssetId() const 
    {
        //return FPrimaryAssetId(FPrimaryAssetType("HktDataAsset"), FName(TEXT("Unknown")));
        return FPrimaryAssetId();
    }
};

// --- 실제 게임 로직 패킷들을 USTRUCT로 정의 ---

USTRUCT(BlueprintType)
struct FDestroyFlagment : public FHktFlagmentBase
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FSampleFlagment : public FHktFlagmentBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString Message;
};

USTRUCT(BlueprintType)
struct FMoveFlagment : public FHktFlagmentBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FVector TargetLocation;

    UPROPERTY(BlueprintReadWrite)
    float Speed;
};

USTRUCT(BlueprintType)
struct FJumpFlagment : public FHktFlagmentBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float JumpHeight;
};

USTRUCT(BlueprintType)
struct FAttackFlagment : public FHktFlagmentBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 SkillId;

    UPROPERTY(BlueprintReadWrite)
    int64 TargetActorId;
};