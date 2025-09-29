#include "HktBehaviorFactory.h"
#include "HktBehavior.h"
#include "HktRpcTraits.h"

// 정적 TMap 인스턴스를 관리하는 함수
TMap<hkt::BehaviorPacket::PacketCase, FHktBehaviorFactory::TCreatorFunc>& FHktBehaviorFactory::GetCreators()
{
    static TMap<hkt::BehaviorPacket::PacketCase, TCreatorFunc> Creators;
    return Creators;
}

// 팩토리에 생성 함수를 등록하는 함수
void FHktBehaviorFactory::Register(hkt::BehaviorPacket::PacketCase PacketCase, TCreatorFunc Func)
{
    // 이미 등록되지 않았을 경우에만 추가합니다.
    if (!GetCreators().Contains(PacketCase))
    {
        GetCreators().Add(PacketCase, MoveTemp(Func));
    }
}

// 등록된 생성 함수를 찾아 Behavior 객체를 생성하는 메인 함수
TUniquePtr<IHktBehavior> FHktBehaviorFactory::CreateBehavior(const hkt::BehaviorPacket& Packet)
{
    const hkt::BehaviorPacket::PacketCase PacketCase = Packet.packet_case();

    // 맵에서 해당 PacketCase에 맞는 생성 함수를 찾습니다.
    const TCreatorFunc* Creator = GetCreators().Find(PacketCase);

    if (Creator && *Creator)
    {
        // 생성 함수가 존재하면 호출하여 객체를 생성합니다.
        return (*Creator)(Packet);
    }

    UE_LOG(LogTemp, Warning, TEXT("Attempted to create behavior from an unregistered or empty packet case: %d"), PacketCase);
    return nullptr;
}


// FBehaviorRegistrar의 정적 인스턴스를 생성하여 각 Behavior 타입을 자동으로 팩토리에 등록합니다.
// 이 코드는 프로그램 시작 시 자동으로 실행됩니다.
namespace
{
    FBehaviorRegistrar<FMoveBehaviorTrait> MoveRegistrar;
    FBehaviorRegistrar<FJumpBehaviorTrait> JumpRegistrar;
    FBehaviorRegistrar<FAttackBehaviorTrait> AttackRegistrar;
    FBehaviorRegistrar<FDestroyBehaviorTrait> DestroyRegistrar;
} // namespace
