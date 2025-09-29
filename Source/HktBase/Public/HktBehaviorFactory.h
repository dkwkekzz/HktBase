#pragma once

#include "HktDef.h"
#include "HktGrpc.h"

// 전방 선언
class IHktBehavior;

/**
 * @brief hkt::BehaviorPacket을 받아 IHktBehavior 객체를 생성하는 등록 기반 팩토리 클래스.
 * 새로운 Behavior 타입을 추가할 때 이 클래스의 코드를 수정할 필요가 없습니다.
 */
class FHktBehaviorFactory
{
public:
    // Behavior 생성 함수의 타입을 정의합니다.
    using TCreatorFunc = TFunction<TUniquePtr<IHktBehavior>(const hkt::BehaviorPacket&)>;

    /**
     * @brief 등록된 생성 함수를 사용해 BehaviorPacket으로부터 실제 Behavior 객체를 생성합니다.
     * @param Packet 서버로부터 동기화된 Behavior 패킷
     * @return 생성된 Behavior 객체의 TUniquePtr. 등록되지 않은 타입이면 nullptr을 반환합니다.
     */
    static TUniquePtr<IHktBehavior> CreateBehavior(const hkt::BehaviorPacket& Packet);

    /**
     * @brief 특정 Packet Case에 대한 Behavior 생성 함수를 팩토리에 등록합니다.
     * @param PacketCase 등록할 패킷의 enum 값
     * @param Func 생성자 람다 함수
     */
    static void Register(hkt::BehaviorPacket::PacketCase PacketCase, TCreatorFunc Func);

private:
    // 생성 함수들을 저장하는 TMap에 대한 정적 접근자를 제공합니다.
    static TMap<hkt::BehaviorPacket::PacketCase, TCreatorFunc>& GetCreators();
};

/**
 * @brief 컴파일 타임에 Behavior 생성 함수를 자동으로 팩토리에 등록하는 헬퍼 클래스.
 * @tparam TBehaviorTrait 등록할 Behavior의 Trait
 */
template <typename TBehaviorTrait>
class FBehaviorRegistrar
{
public:
    FBehaviorRegistrar()
    {
        // Trait 정보를 사용하여 생성자 람다 함수를 만들고 팩토리에 등록합니다.
        auto Creator = [](const hkt::BehaviorPacket& Packet) -> TUniquePtr<IHktBehavior> {
            const int64 BehaviorId = Packet.behavior_id();
            const int64 OwnerPlayerId = Packet.owner_player_id();

            // Trait에 정의된 GetPacketFrom 함수를 통해 구체적인 패킷을 가져옵니다.
            const auto& SpecificPacket = TBehaviorTrait::GetPacketFrom(Packet);

            // Trait에 정의된 Behavior 타입을 사용하여 객체를 생성합니다.
            return MakeUnique<typename TBehaviorTrait::Behavior>(BehaviorId, OwnerPlayerId, SpecificPacket);
            };

        FHktBehaviorFactory::Register(TBehaviorTrait::CaseEnum, Creator);
    }
};
