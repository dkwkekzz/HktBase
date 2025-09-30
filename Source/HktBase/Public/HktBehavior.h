#pragma once

#include "HktDef.h"


class IHktBehavior
{
public:
	virtual ~IHktBehavior() {}
	virtual uint32 GetTypeId() const = 0;
	virtual FHktId GetSubjectId() const = 0;
	virtual FHktId GetBehaviorId() const = 0;
	virtual FHktTagContainer GetTags() const = 0;
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
 * @brief 템플릿을 사용하여 각 Behavior 구조체 타입에 고유한 타입 ID를 컴파일 타임에 할당합니다.
 * @tparam T USTRUCT으로 정의된 구조체 타입
 * @return 해당 타입의 고유 ID
 */
template<typename T>
inline uint32 GetBehaviorTypeId()
{
    static const uint32 Id = FBehaviorTypeIdGenerator::GetNextId();
    return Id;
}

/**
 * @brief Behavior Trait의 정의를 통해 구체적인 Behavior를 생성하는 템플릿 클래스
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

    virtual FHktTagContainer GetTags() const override
    {
		return FHktTagContainer();
    }

    const PacketType& GetPacket() const { return Packet; }

protected:
    FHktId BehaviorId;
    FHktId SubjectId;
    PacketType Packet;
};
