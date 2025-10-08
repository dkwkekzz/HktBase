#include "HktBehaviorFactory.h"
#include "HktBehaviorHeader.h"


// TMap 인스턴스를 반환하는 함수
TMap<int32, FHktBehaviorFactory::TCreatorFunc>& FHktBehaviorFactory::GetCreators()
{
    static TMap<int32, TCreatorFunc> Creators;
    return Creators;
}

// 레지스트리에 생성 함수를 등록하는 함수
void FHktBehaviorFactory::Register(int32 BehaviorTypeId, TCreatorFunc Func)
{
    // 이미 등록되지 않았을 경우에만 추가합니다.
    if (!GetCreators().Contains(BehaviorTypeId))
    {
        GetCreators().Add(BehaviorTypeId, MoveTemp(Func));
    }
}

// 등록된 생성 함수를 찾아 Behavior 객체를 생성하는 정적 함수
TUniquePtr<IHktBehavior> FHktBehaviorFactory::CreateBehavior(const FHktBehaviorResponseHeader& Header)
{
    // 필요한 경우 해당 BehaviorTypeId에 맞는 생성 함수를 찾습니다.
    const TCreatorFunc* Creator = GetCreators().Find(Header.FlagmentTypeId);
    if (Creator && *Creator)
    {
        // 생성 함수가 유효하면 호출하여 객체를 생성합니다.
        return (*Creator)(Header);
    }

    UE_LOG(LogTemp, Warning, TEXT("Attempted to create behavior from an unregistered or empty behavior type id: %d"), Header.FlagmentTypeId);
    return nullptr;
}
