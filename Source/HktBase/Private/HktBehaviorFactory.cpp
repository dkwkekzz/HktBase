#include "HktBehaviorFactory.h"
#include "HktBehavior.h"
#include "HktRpcTraits.h"

// ���� TMap �ν��Ͻ��� �����ϴ� �Լ�
TMap<hkt::BehaviorPacket::PacketCase, FHktBehaviorFactory::TCreatorFunc>& FHktBehaviorFactory::GetCreators()
{
    static TMap<hkt::BehaviorPacket::PacketCase, TCreatorFunc> Creators;
    return Creators;
}

// ���丮�� ���� �Լ��� ����ϴ� �Լ�
void FHktBehaviorFactory::Register(hkt::BehaviorPacket::PacketCase PacketCase, TCreatorFunc Func)
{
    // �̹� ��ϵ��� �ʾ��� ��쿡�� �߰��մϴ�.
    if (!GetCreators().Contains(PacketCase))
    {
        GetCreators().Add(PacketCase, MoveTemp(Func));
    }
}

// ��ϵ� ���� �Լ��� ã�� Behavior ��ü�� �����ϴ� ���� �Լ�
TUniquePtr<IHktBehavior> FHktBehaviorFactory::CreateBehavior(const hkt::BehaviorPacket& Packet)
{
    const hkt::BehaviorPacket::PacketCase PacketCase = Packet.packet_case();

    // �ʿ��� �ش� PacketCase�� �´� ���� �Լ��� ã���ϴ�.
    const TCreatorFunc* Creator = GetCreators().Find(PacketCase);

    if (Creator && *Creator)
    {
        // ���� �Լ��� �����ϸ� ȣ���Ͽ� ��ü�� �����մϴ�.
        return (*Creator)(Packet);
    }

    UE_LOG(LogTemp, Warning, TEXT("Attempted to create behavior from an unregistered or empty packet case: %d"), PacketCase);
    return nullptr;
}


// FBehaviorRegistrar�� ���� �ν��Ͻ��� �����Ͽ� �� Behavior Ÿ���� �ڵ����� ���丮�� ����մϴ�.
// �� �ڵ�� ���α׷� ���� �� �ڵ����� ����˴ϴ�.
namespace
{
    FBehaviorRegistrar<FMoveBehaviorTrait> MoveRegistrar;
    FBehaviorRegistrar<FJumpBehaviorTrait> JumpRegistrar;
    FBehaviorRegistrar<FAttackBehaviorTrait> AttackRegistrar;
    FBehaviorRegistrar<FDestroyBehaviorTrait> DestroyRegistrar;
} // namespace
