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
 * @brief Behavior Ÿ�Ժ��� ���� ID�� �����ϴ� Ŭ����
 */
class FBehaviorTypeIdGenerator
{
public:
    static inline uint32 GetNextId()
    {
        // 0�� ��ȿ���� ���� ������ ���� �� �����Ƿ� 1���� �����մϴ�.
        static uint32 Counter = 1;
        return Counter++;
    }
};

/**
 * @brief ���ø��� ����Ͽ� �� Behavior ������ Ÿ�Կ� ���� ���� ID�� ������ Ÿ�ӿ� �Ҵ��մϴ�.
 * @tparam T USTRUCT�� ���ǵ� ������ Ÿ��
 * @return �ش� Ÿ���� ���� ID
 */
template<typename T>
inline uint32 GetBehaviorTypeId()
{
    static const uint32 Id = FBehaviorTypeIdGenerator::GetNextId();
    return Id;
}

/**
 * @brief Behavior Trait�� ������� ���� ������ �����ϴ� ���ø� Ŭ����
 * @tparam TBehaviorTrait Behavior�� Ư��(��Ŷ Ÿ�� ��)�� �����ϴ� Trait
 */
template <typename TBehaviorTrait>
class THktBehavior : public IHktBehavior
{
public:
    // Trait�� ���ǵ� Packet Ÿ���� �����ɴϴ�.
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
