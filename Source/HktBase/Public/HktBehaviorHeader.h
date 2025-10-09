#pragma once

#include "CoreMinimal.h"
#include "HktBehaviorHeader.generated.h"

namespace HktBehaviorHeader
{
    constexpr int32 DestroyBehaviorTypeId = -1;
}

USTRUCT(BlueprintType)
struct FHktBehaviorRequestHeader
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int64 SubjectId;

    UPROPERTY(BlueprintReadWrite)
    int64 SyncGroupId;

    UPROPERTY(BlueprintReadWrite)
    int32 FlagmentTypeId;

    UPROPERTY(BlueprintReadWrite)
    TArray<uint8> FlagmentPayload;
};

USTRUCT(BlueprintType)
struct FHktBehaviorResponseHeader : public FHktBehaviorRequestHeader
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int64 BehaviorInstanceId;
};
