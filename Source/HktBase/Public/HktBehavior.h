#pragma once

#include "HktDef.h"


class IHktBehavior
{
public:
	virtual ~IHktBehavior() {}
	virtual int32 GetTypeId() const = 0;
	virtual FHktId GetSubjectId() const = 0;
	virtual FHktId GetBehaviorId() const = 0;
	virtual FHktTagContainer GetTags() const = 0;
    virtual FPrimaryAssetId GetViewAssetId() const = 0;
};


/**
 * @brief Behavior 타입별로 고유 ID를 생성하는 클래스
 */
class FBehaviorTypeIdGenerator
{
public:
    static inline int32 GetNextId()
    {
        // 0은 유효하지 않은 값으로 사용될 수 있으므로 1부터 시작합니다.
        static int32 Counter = 0;
        return Counter++;
    }
};

/**
 * @brief 템플릿을 사용하여 각 Behavior 구조체 타입에 고유한 타입 ID를 컴파일 타임에 할당합니다.
 * @tparam T USTRUCT으로 정의된 구조체 타입
 * @return 해당 타입의 고유 ID
 */
template<typename T>
inline int32 GetBehaviorTypeId()
{
    static const int32 Id = FBehaviorTypeIdGenerator::GetNextId();
    return Id;
}

/**
 * @brief Behavior Trait의 정의를 통해 구체적인 Behavior를 생성하는 템플릿 클래스
 * @tparam TBehaviorTrait Behavior의 특성(패킷 타입 등)을 정의하는 Trait
 */
template <typename TFlagment>
class HKTBASE_API THktBehavior : public IHktBehavior
{
public:
    using FlagmentType = TFlagment;

    THktBehavior(FHktId InBehaviorId, FHktId InSubjectId, const TFlagment& InFlagment)
        : BehaviorId(InBehaviorId), SubjectId(InSubjectId), Flagment(InFlagment)
    {
    }

    virtual int32 GetTypeId() const override
    {
        return GetBehaviorTypeId<TFlagment>();
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

    virtual FPrimaryAssetId GetViewAssetId() const override
    {
		return Flagment.GetViewAssetId();
    }

    const TFlagment& GetFlagment() const { return Flagment; }

protected:
    FHktId BehaviorId;
    FHktId SubjectId;
    TFlagment Flagment;
};
