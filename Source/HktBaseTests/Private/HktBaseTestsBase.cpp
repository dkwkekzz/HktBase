#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "HktGraph.h"
#include "HktBehaviorFactory.h"
#include "HktEventBus.h"
#include "HktStructSerializer.h"
#include "HktFlagments.h"

// FHktBehaviorFactory의 동작을 테스트합니다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHktBaseFactoryTest, "HktBase.Factory", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FHktBaseFactoryTest::RunTest(const FString& Parameters)
{
    const int64 SubjectId = 1;
    const FString StrData = TEXT("HktBaseFactoryTest");

    FSampleFlagment Flagment;
    Flagment.StrData = StrData;

    // 1. 응답 헤더 생성 (서버에서 받았다고 가정)
    FHktBehaviorResponseHeader ResponseHeader;
    ResponseHeader.BehaviorInstanceId = 100;
    ResponseHeader.SubjectId = SubjectId;
    ResponseHeader.FlagmentTypeId = GetBehaviorTypeId<FSampleFlagment>();
    ResponseHeader.FlagmentPayload = FHktStructSerializer::SerializeStructToBytes(Flagment);

    // 2. 팩토리를 통해 Behavior 생성
    TUniquePtr<IHktBehavior> Behavior = FHktBehaviorFactory::CreateBehavior(ResponseHeader);
    if (!TestNotNull(TEXT("Behavior가 유효해야 합니다."), Behavior.Get()))
    {
        return false;
    }

    // 3. 생성된 Behavior의 타입과 데이터 검증
    const auto* TypedBehavior = static_cast<const THktBehavior<FSampleFlagment>*>(Behavior.Get());
    if (!TestNotNull(TEXT("Behavior 타입이 일치해야 합니다."), TypedBehavior))
    {
        return false;
    }

    TestEqual(TEXT("Behavior 데이터가 일치해야 합니다."), TypedBehavior->GetFlagment().StrData, StrData);
    
    return true;
}

// FHktGraph의 동작을 테스트합니다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHktBaseGraphTest, "HktBase.Graph", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FHktBaseGraphTest::RunTest(const FString& Parameters)
{
    // 테스트용 Behavior 생성
    FSampleFlagment Flagment;
    Flagment.StrData = TEXT("HktBaseGraphTest1");
    FHktBehaviorResponseHeader ResponseHeader;
    ResponseHeader.BehaviorInstanceId = 101;
    ResponseHeader.SubjectId = 2;
    ResponseHeader.FlagmentTypeId = GetBehaviorTypeId<FSampleFlagment>();
    ResponseHeader.FlagmentPayload = FHktStructSerializer::SerializeStructToBytes(Flagment);
    TUniquePtr<IHktBehavior> Behavior = FHktBehaviorFactory::CreateBehavior(ResponseHeader);
    if (!TestNotNull(TEXT("테스트용 Behavior가 유효해야 합니다."), Behavior.Get()))
    {
        return false;
    }
    
    FHktGraph Graph;
    const FHktId BehaviorId = Behavior->GetBehaviorId();

    // 1. 그래프에 Behavior 추가 및 조회
    Graph.AddBehavior(MoveTemp(Behavior));
    const IHktBehavior* FoundBehavior = Graph.FindBehavior(BehaviorId);
    if (!TestNotNull(TEXT("그래프에서 Behavior를 찾을 수 있어야 합니다."), FoundBehavior))
    {
        return false;
    }

    // 2. 그래프에서 Behavior 제거 및 확인
    Graph.RemoveBehavior(BehaviorId);
    FoundBehavior = Graph.FindBehavior(BehaviorId);
    TestNull(TEXT("그래프에서 Behavior가 제거되어야 합니다."), FoundBehavior);

    return true;
}

// THktEventBus의 동작을 테스트합니다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHktBaseEventBusTest, "HktBase.EventBus", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FHktBaseEventBusTest::RunTest(const FString& Parameters)
{
    const FHktId TestNodeId = 1;
    const FString TestPayloadData = TEXT("HktBaseEventBusTest1");
    bool bEventReceived = false;
    FString ReceivedData;

    // 1. 이벤트 구독
    FDelegateHandle Handle = THktEventBus<FSampleFlagment>::Get().GetEvent(TestNodeId).AddLambda(
        [&](const FSampleFlagment& Payload)
        {
            bEventReceived = true;
            ReceivedData = Payload.StrData;
        });

    // 2. 이벤트 발행
    FSampleFlagment PayloadToSend;
    PayloadToSend.StrData = TestPayloadData;
    THktEventBus<FSampleFlagment>::Get().Broadcast(TestNodeId, FHktTag(), PayloadToSend);

    // 3. 이벤트 수신 확인
    TestTrue(TEXT("이벤트가 수신되어야 합니다."), bEventReceived);
    TestEqual(TEXT("페이로드 데이터가 일치해야 합니다."), ReceivedData, TestPayloadData);

    // 4. 구독 해제
    THktEventBus<FSampleFlagment>::Get().GetEvent(TestNodeId).Remove(Handle);

    return true;
}
