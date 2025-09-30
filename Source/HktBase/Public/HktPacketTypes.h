#pragma once

#include "CoreMinimal.h"
#include "HktBehavior.h"
#include "HktPacketTypes.generated.h"

// 모든 패킷 구조체의 기반이 될 기본 구조체 (선택사항이지만 좋은 습관입니다)
USTRUCT(BlueprintType)
struct FHktPacketBase
{
    GENERATED_BODY()

    virtual ~FHktPacketBase() = default;
};

// --- 실제 게임 로직 패킷들을 USTRUCT로 정의 ---

USTRUCT(BlueprintType)
struct FMovePacket : public FHktPacketBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FVector TargetLocation;

    UPROPERTY(BlueprintReadWrite)
    float Speed;
};

USTRUCT(BlueprintType)
struct FJumpPacket : public FHktPacketBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    float JumpHeight;
};

USTRUCT(BlueprintType)
struct FAttackPacket : public FHktPacketBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 SkillId;

    UPROPERTY(BlueprintReadWrite)
    int64 TargetActorId;
};

USTRUCT(BlueprintType)
struct FDestroyPacket : public FHktPacketBase
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int64 BehaviorIdToDestroy;
};
