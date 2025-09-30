#include "HktRpcProxy.h"
#include "HktBehaviorFactory.h"
#include "HktPacketTypes.h"
#include "HktStructSerializer.h"

FHktRpcProxy::FHktRpcProxy(const FString& ServerAddress)
	: Stub(hkt::HktRpcService::NewStub(grpc::CreateChannel(TCHAR_TO_UTF8(*ServerAddress), grpc::InsecureChannelCredentials())))
{
	UE_LOG(LogHktRpc, Log, TEXT("FHktRpcProxy created for address: %s"), *ServerAddress);
	// CompletionQueue를 폴링할 워커 스레드를 시작합니다.
	WorkerThread = std::thread([this]() { ProcessResponses(); });
}

FHktRpcProxy::~FHktRpcProxy()
{
	UE_LOG(LogHktRpc, Log, TEXT("FHktRpcProxy destroyed."));
	// 소멸 시 CompletionQueue를 종료하고 워커 스레드를 대기합니다.
	CompletionQueue.Shutdown();
	if (WorkerThread.joinable())
	{
		WorkerThread.join();
	}
}

void FHktRpcProxy::ExecuteBehavior(int64 GroupId, int64 SubjectId, int32 BehaviorTypeId, const TArray<uint8>& Bytes)
{
	UE_LOG(LogHktRpc, Log, TEXT("Calling ExecuteBehavior for GroupId: %lld"), GroupId);

	hkt::BehaviorPacket PbPacket;
	PbPacket.set_owner_player_id(SubjectId);
	PbPacket.set_behavior_id(0);
	PbPacket.set_behavior_type_id(BehaviorTypeId);
	PbPacket.set_payload(Bytes.GetData(), Bytes.Num());

	hkt::BehaviorRequest Request;
	Request.set_group_id(GroupId);
	*Request.mutable_packet() = PbPacket;

	Call<HktRpc::FExecuteBehaviorRpcTrait>(Request,
		[](const grpc::Status& Status, const google::protobuf::Empty& Response)
		{
		});
}

void FHktRpcProxy::SyncGroup(int64 SubjectId, int64 GroupId, TFunction<void(TUniquePtr<IHktBehavior>)> Callback)
{
	UE_LOG(LogHktRpc, Log, TEXT("Calling SyncGroup for PlayerId: %lld, GroupId: %lld"), SubjectId, GroupId);
	hkt::SyncRequest Request;
	Request.set_player_id(SubjectId);
	Request.set_group_id(GroupId);
    CallStream<HktRpc::FSyncGroupRpcTrait>(Request,
        [Callback](const grpc::Status& Status, const hkt::SyncResponse& Response)
        {
            Callback(FHktBehaviorFactory::CreateBehavior(Response.packet()));
        });
}

void FHktRpcProxy::ProcessResponses()
{
	void* Tag;
	bool bOk;
	while (CompletionQueue.Next(&Tag, &bOk))
	{
		UE_LOG(LogHktRpc, Verbose, TEXT("Processing RPC response."));
		// Tag를 IAsyncCall 포인터로 캐스팅합니다.
		// 모든 비동기 호출은 IAsyncCall을 구현한 객체를 Tag로 사용합니다.
		IAsyncCall* Call = static_cast<IAsyncCall*>(Tag);
		// 가상 함수 호출을 통해 각 RPC 타입에 맞는 로직을 실행합니다.
		Call->Proceed(bOk);
	}
}
