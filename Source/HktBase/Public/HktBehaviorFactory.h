#pragma once

#include "HktDef.h"
#include "HktBehavior.h"
#include "HktBehaviorHeader.h"
#include "HktStructSerializer.h"


/**
 * @brief hkt::BehaviorPacket�� �޾� IHktBehavior ��ü�� �����ϴ� ��� ��� ���丮 Ŭ����.
 * ���ο� Behavior Ÿ���� �߰��� �� �� Ŭ������ �ڵ带 ������ �ʿ䰡 �����ϴ�.
 */
class HKTBASE_API FHktBehaviorFactory
{
public:
    template<typename TFlagment>
    inline FHktBehaviorRequestHeader CreateBehaviorRequest(int64 SubjectId, int64 SyncGroupId, const TFlagment& FlagmentPayload)
    {
        FHktBehaviorRequestHeader Header;
        Header.SubjectId = SubjectId;
        Header.SyncGroupId = SyncGroupId;
        Header.FlagmentTypeId = GetBehaviorTypeId<TFlagment>();
        Header.FlagmentPayload = FHktStructSerializer::SerializeStructToBytes(FlagmentPayload);
        return Header;
    }

    // Behavior 생성 함수의 타입을 정의합니다.
    using TCreatorFunc = TFunction<TUniquePtr<IHktBehavior>(const FHktBehaviorResponseHeader&)>;

    /**
     * @brief 등록된 생성 함수를 사용하여 BehaviorPacket으로부터 특정 Behavior 객체를 생성합니다.
     * @param Packet 서버로부터 수신한 Behavior 패킷
     * @return 생성된 Behavior 객체의 TUniquePtr. 등록되지 않은 타입이면 nullptr을 반환합니다.
     */
    static TUniquePtr<IHktBehavior> CreateBehavior(const FHktBehaviorResponseHeader& Header);

    /**
     * @brief 특정 Behavior 타입 ID에 대해 Behavior 생성 함수를 레지스트리에 등록합니다.
     * @param BehaviorTypeId 등록할 패킷의 타입 ID
     * @param Func 생성자 함수
     */
    static void Register(int32 BehaviorTypeId, TCreatorFunc Func);

private:
    // 생성 함수들을 저장하는 TMap에 대한 정적 접근자를 제공합니다.
    static TMap<int32, TCreatorFunc>& GetCreators();
};


/**
 * @brief 템플릿을 통해 Behavior 생성 함수를 자동적으로 레지스트리에 등록하는 헬퍼 클래스.
 * @tparam TPacket 등록할 Behavior의 USTRUCT 패킷 타입
 */
template <typename TFlagment>
class FBehaviorRegistrar
{
public:
    FBehaviorRegistrar()
    {
        // Trait 정보를 사용하여 생성자 함수를 만들고 레지스트리에 등록합니다.
        auto Creator = [](const FHktBehaviorResponseHeader& Header) -> TUniquePtr<IHktBehavior>
            {
                TFlagment Flagment;
                if (FHktStructSerializer::DeserializeStructFromBytes(Header.FlagmentPayload, Flagment) == false)
                {
                    return nullptr;
                }

                // Trait에 정의된 Behavior 타입을 사용하여 객체를 생성합니다.
                return MakeUnique<THktBehavior<TFlagment>>(Header.BehaviorInstanceId, Header.SubjectId, Flagment);
            };

        // 이 코드는 컴파일 타임에 각 패킷 타입에 대한 고유 ID를 가져옵니다.
        FHktBehaviorFactory::Register(GetBehaviorTypeId<TFlagment>(), Creator);
    }
};
