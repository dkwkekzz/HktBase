#include "HktRpcService.h"

// --- FHktRpcService Implementation ---

FHktRpcService::~FHktRpcService()
{
	UE_LOG(LogHktRpc, Log, TEXT("FHktRpcService shutting down."));
	Server->Shutdown();
	for (auto& cq : CQs)
	{
		cq->Shutdown();
	}
	for (auto& thread : WorkerThreads)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}
}

void FHktRpcService::Run(const std::string& ServerAddress)
{
	grpc::ServerBuilder Builder;
	Builder.AddListeningPort(ServerAddress, grpc::InsecureServerCredentials());
	Builder.RegisterService(&Service);

	const int NumCores = std::thread::hardware_concurrency();
	for (int i = 0; i < NumCores; ++i)
	{
		CQs.emplace_back(Builder.AddCompletionQueue());
	}

	Server = Builder.BuildAndStart();
	UE_LOG(LogHktRpc, Log, TEXT("FHktRpcService running on %s"), *FString(ServerAddress.c_str()));
	
	for (int i = 0; i < NumCores; ++i)
	{
		// 단항 RPC 핸들러를 생성합니다.
		new FUnaryCallHandler<HktRpc::FExecuteBehaviorRpcTrait>(&Service, CQs[i].get());

		// 템플릿으로 변경된 스트리밍 세션 핸들러를 trait와 함께 생성합니다.
		new FStreamSessionHandler<HktRpc::FSyncGroupRpcTrait>(&Service, CQs[i].get());

		WorkerThreads.emplace_back([this, i]() { HandleRpcs(i); });
	}
}

void FHktRpcService::HandleRpcs(int CQIndex)
{
	void* Tag;
	bool bOk;
	while (CQs[CQIndex]->Next(&Tag, &bOk))
	{
		UE_LOG(LogHktRpc, Verbose, TEXT("CQ %d: Handling RPC tag."), CQIndex);
		auto* BaseHandler = static_cast<FRpcCallHandlerBase*>(Tag);
		BaseHandler->Proceed(bOk);
	}
}

