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
 * @brief 수신된 데이터를 기반으로 Behavior 인스턴스를 생성하는 팩토리입니다.
 */
class FHktBehaviorFactory
{
public:
    // 생성자 함수 타입 정의 (USTRUCT 데이터를 받아 Behavior를 생성)
    using FBehaviorCreator = TFunction<TUniquePtr<IHktBehavior>()>;

    static inline void Register(uint32 InTypeId, FBehaviorCreator InCreator)
    {
        GetRegistry().Add(InTypeId, InCreator);
    }

    static inline TUniquePtr<IHktBehavior> Create(uint32 InTypeId)
    {
        const FBehaviorCreator* Creator = GetRegistry().Find(InTypeId);
        return Creator ? (*Creator)() : nullptr;
    }

private:
    static inline TMap<uint32, FBehaviorCreator>& GetRegistry()
    {
        static TMap<uint32, FBehaviorCreator> Registry;
        return Registry;
    }
};


/**
 * @brief 데이터(USTRUCT)와 행위(Execute)를 결합하는 CRTP 기반 템플릿 클래스입니다.
 * @tparam Derived 파생될 Behavior 클래스 타입 (예: FHktPlayerBehavior)
 * @tparam DataType 이 Behavior가 사용할 USTRUCT 데이터 타입 (예: FHktPlayerData)
 */
template<typename TraitType>
class THktBehavior : public IHktBehavior
{
public:
    using PacketType = typename TraitType::Packet;

    // 생성 시 데이터 구조체를 소유권 이전(move)을 통해 받아 멤버로 저장합니다.
    explicit THktBehavior(typename TraitType::Packet&& InData) : Data(MoveTemp(InData)) {}
    virtual ~THktBehavior() override = default;

    // 템플릿 기반 ID 생성기를 사용하여 고유 ID를 자동으로 부여합니다.
    virtual uint32 GetTypeId() const
    {
        return GetTypeIdStatic();
    }

private:
    inline static uint32 GetTypeIdStatic()
    {
        return static_cast<uint32>(TraitType::CaseEnum);
    }

    /**
     * @brief 자동 등록을 위한 헬퍼 구조체입니다.
     * 이 구조체의 static 인스턴스가 생성될 때 생성자가 호출되어 팩토리에 Behavior를 자동으로 등록합니다.
     */
    struct FAutoRegisterer
    {
        FAutoRegisterer()
        {
            FHktBehaviorFactory::Register(GetTypeIdStatic(), [](typename TraitType::Packet&& InData) -> TUniquePtr<IHktBehavior>
                {
                    return MakeUnique<THktBehavior<TraitType>>(std::move(InData));
                });
        }
    };

    // static 인스턴스를 선언하여 FAutoRegisterer의 생성자를 호출하도록 합니다.
    static FAutoRegisterer AutoRegisterer;

protected:
    // 파생 클래스가 데이터에 접근할 수 있도록 protected 멤버로 선언합니다.
    typename TraitType::Packet Data;
};

// static 멤버 변수를 클래스 외부에서 정의합니다.
template<typename TraitType>
typename THktBehavior<TraitType>::FAutoRegisterer THktBehavior<TraitType>::AutoRegisterer;
