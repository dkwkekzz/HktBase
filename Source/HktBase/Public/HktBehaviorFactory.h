#pragma once

#include "HktDef.h"
#include "HktGrpc.h"
#include "HktBehavior.h"
#include "HktStructSerializer.h"


/**
 * @brief hkt::BehaviorPacket�� �޾� IHktBehavior ��ü�� �����ϴ� ��� ��� ���丮 Ŭ����.
 * ���ο� Behavior Ÿ���� �߰��� �� �� Ŭ������ �ڵ带 ������ �ʿ䰡 �����ϴ�.
 */
class FHktBehaviorFactory
{
public:
    // Behavior 생성 함수의 타입을 정의합니다.
    using TCreatorFunc = TFunction<TUniquePtr<IHktBehavior>(const hkt::BehaviorPacket&)>;

    /**
     * @brief 등록된 생성 함수를 사용하여 BehaviorPacket으로부터 특정 Behavior 객체를 생성합니다.
     * @param Packet 서버로부터 수신한 Behavior 패킷
     * @return 생성된 Behavior 객체의 TUniquePtr. 등록되지 않은 타입이면 nullptr을 반환합니다.
     */
    static TUniquePtr<IHktBehavior> CreateBehavior(const hkt::BehaviorPacket& Packet);

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
template <typename TPacket>
class FBehaviorRegistrar
{
public:
    FBehaviorRegistrar()
    {
        // Trait 정보를 사용하여 생성자 함수를 만들고 레지스트리에 등록합니다.
        auto Creator = [](const hkt::BehaviorPacket& Packet) -> TUniquePtr<IHktBehavior>
            {
                const int64 BehaviorId = Packet.behavior_id();
                const int64 OwnerPlayerId = Packet.owner_player_id();

                const std::string& Payload = Packet.payload();
                TArray<uint8> Bytes(reinterpret_cast<const uint8*>(Payload.data()), Payload.size());

				TPacket PacketInstance;
                if (FHktStructSerializer::DeserializeStructFromBytes(Bytes, PacketInstance) == false)
                {
                    return nullptr;
                }

                // Trait에 정의된 Behavior 타입을 사용하여 객체를 생성합니다.
                return MakeUnique<THktBehavior<TPacket>>(BehaviorId, OwnerPlayerId, PacketInstance);
            };

        // 이 코드는 컴파일 타임에 각 패킷 타입에 대한 고유 ID를 가져옵니다.
        FHktBehaviorFactory::Register(GetBehaviorTypeId<TPacket>(), Creator);
    }
};