#pragma once

#include "HktDef.h"
#include "HktGrpc.h"

// ���� ����
class IHktBehavior;

/**
 * @brief hkt::BehaviorPacket�� �޾� IHktBehavior ��ü�� �����ϴ� ��� ��� ���丮 Ŭ����.
 * ���ο� Behavior Ÿ���� �߰��� �� �� Ŭ������ �ڵ带 ������ �ʿ䰡 �����ϴ�.
 */
class FHktBehaviorFactory
{
public:
    // Behavior ���� �Լ��� Ÿ���� �����մϴ�.
    using TCreatorFunc = TFunction<TUniquePtr<IHktBehavior>(const hkt::BehaviorPacket&)>;

    /**
     * @brief ��ϵ� ���� �Լ��� ����� BehaviorPacket���κ��� ���� Behavior ��ü�� �����մϴ�.
     * @param Packet �����κ��� ����ȭ�� Behavior ��Ŷ
     * @return ������ Behavior ��ü�� TUniquePtr. ��ϵ��� ���� Ÿ���̸� nullptr�� ��ȯ�մϴ�.
     */
    static TUniquePtr<IHktBehavior> CreateBehavior(const hkt::BehaviorPacket& Packet);

    /**
     * @brief Ư�� Packet Case�� ���� Behavior ���� �Լ��� ���丮�� ����մϴ�.
     * @param PacketCase ����� ��Ŷ�� enum ��
     * @param Func ������ ���� �Լ�
     */
    static void Register(hkt::BehaviorPacket::PacketCase PacketCase, TCreatorFunc Func);

private:
    // ���� �Լ����� �����ϴ� TMap�� ���� ���� �����ڸ� �����մϴ�.
    static TMap<hkt::BehaviorPacket::PacketCase, TCreatorFunc>& GetCreators();
};

/**
 * @brief ������ Ÿ�ӿ� Behavior ���� �Լ��� �ڵ����� ���丮�� ����ϴ� ���� Ŭ����.
 * @tparam TBehaviorTrait ����� Behavior�� Trait
 */
template <typename TBehaviorTrait>
class FBehaviorRegistrar
{
public:
    FBehaviorRegistrar()
    {
        // Trait ������ ����Ͽ� ������ ���� �Լ��� ����� ���丮�� ����մϴ�.
        auto Creator = [](const hkt::BehaviorPacket& Packet) -> TUniquePtr<IHktBehavior> {
            const int64 BehaviorId = Packet.behavior_id();
            const int64 OwnerPlayerId = Packet.owner_player_id();

            // Trait�� ���ǵ� GetPacketFrom �Լ��� ���� ��ü���� ��Ŷ�� �����ɴϴ�.
            const auto& SpecificPacket = TBehaviorTrait::GetPacketFrom(Packet);

            // Trait�� ���ǵ� Behavior Ÿ���� ����Ͽ� ��ü�� �����մϴ�.
            return MakeUnique<typename TBehaviorTrait::Behavior>(BehaviorId, OwnerPlayerId, SpecificPacket);
            };

        FHktBehaviorFactory::Register(TBehaviorTrait::CaseEnum, Creator);
    }
};
