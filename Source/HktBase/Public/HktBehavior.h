#pragma once

#include "HktDef.h"


class IHktBehavior
{
public:
	virtual ~IHktBehavior() {}
	virtual uint32 GetTypeId() const = 0;
	virtual FHktId GetSubjectId() const = 0;
	virtual FHktId GetBehaviorId() const = 0;
	virtual FGameplayTagContainer GetTags() const = 0;
};


/**
 * @brief Behavior 타입별로 고유 ID를 생성하는 클래스
 */
class FBehaviorTypeIdGenerator
{
public:
    static inline uint32 GetNextId()
    {
        // 0은 유효하지 않은 값으로 사용될 수 있으므로 1부터 시작합니다.
        static uint32 Counter = 1;
        return Counter++;
    }
};

/**
 * @brief 템플릿을 사용하여 각 Behavior 데이터 타입에 대한 고유 ID를 컴파일 타임에 할당합니다.
 * @tparam T USTRUCT로 정의된 데이터 타입
 * @return 해당 타입의 고유 ID
 */
template<typename T>
inline uint32 GetBehaviorTypeId()
{
    static const uint32 Id = FBehaviorTypeIdGenerator::GetNextId();
    return Id;
}

/**
 * @brief Behavior Trait를 기반으로 공통 로직을 구현하는 템플릿 클래스
 * @tparam TBehaviorTrait Behavior의 특성(패킷 타입 등)을 정의하는 Trait
 */
template <typename TBehaviorTrait>
class THktBehavior : public IHktBehavior
{
public:
    // Trait에 정의된 Packet 타입을 가져옵니다.
    using PacketType = typename TBehaviorTrait::Packet;

    THktBehavior(FHktId InBehaviorId, FHktId InSubjectId, const PacketType& InPacket)
        : BehaviorId(InBehaviorId), SubjectId(InSubjectId)
    {
    }

    virtual uint32 GetTypeId() const override
    {
        return GetBehaviorTypeId<TBehaviorTrait>();
    }

    virtual FHktId GetSubjectId() const override
    {
        return SubjectId;
    }

    virtual FHktId GetBehaviorId() const override
    {
        return BehaviorId;
    }

    virtual FGameplayTagContainer GetTags() const override
    {
		return FGameplayTagContainer();
    }

    const PacketType& GetPacket() const { return Packet; }

protected:
    FHktId BehaviorId;
    FHktId SubjectId;
    PacketType Packet;
};
