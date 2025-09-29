#include "HktRpcProxy.h"

FHktRpcProxy::FHktRpcProxy(const FString& ServerAddress)
	: Stub(hkt::HktRpcService::NewStub(grpc::CreateChannel(TCHAR_TO_UTF8(*ServerAddress), grpc::InsecureChannelCredentials())))
{
	// CompletionQueue를 폴링할 워커 스레드를 시작합니다.
	WorkerThread = std::thread([this]() { ProcessResponses(); });
}

FHktRpcProxy::~FHktRpcProxy()
{
	// 소멸 시 CompletionQueue를 종료하고 워커 스레드를 대기합니다.
	CompletionQueue.Shutdown();
	if (WorkerThread.joinable())
	{
		WorkerThread.join();
	}
}

void FHktRpcProxy::ProcessResponses()
{
	void* Tag;
	bool bOk;
	while (CompletionQueue.Next(&Tag, &bOk))
	{
		// Tag를 IAsyncCall 포인터로 캐스팅합니다.
		// 모든 비동기 호출은 IAsyncCall을 구현한 객체를 Tag로 사용합니다.
		IAsyncCall* Call = static_cast<IAsyncCall*>(Tag);
		// 가상 함수 호출을 통해 각 RPC 타입에 맞는 로직을 실행합니다.
		Call->Proceed(bOk);
	}
}
