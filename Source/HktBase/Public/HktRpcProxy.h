#pragma once

#include "HktGrpc.h"
#include "HktRpcTraits.h"
#include <memory>
#include <functional>
#include <thread>
#include <unordered_map>

// 클라이언트 측 RPC 프록시 클래스입니다.
// 템플릿과 Trait를 사용하여 모든 RPC 호출을 일반화된 방식으로 처리합니다.
class HKTBASE_API FHktRpcProxy
{
private:
	// 모든 비동기 RPC 호출을 일반화하기 위한 인터페이스입니다.
	// CompletionQueue의 Tag로 이 인터페이스의 포인터가 사용됩니다.
	class IAsyncCall
	{
	public:
		virtual ~IAsyncCall() = default;
		// CompletionQueue에서 이벤트가 발생했을 때 호출될 가상 함수입니다.
		// bOk는 작업의 성공 여부를 나타냅니다.
		virtual void Proceed(bool bOk) = 0;
	};

public:
	FHktRpcProxy(const FString& ServerAddress);
	~FHktRpcProxy();

	// 단항 RPC를 호출하는 템플릿 함수
	template<typename TRpcTrait>
	void Call(const typename TRpcTrait::TRequest& Request, std::function<void(const grpc::Status&, const typename TRpcTrait::TResponse&)> Callback)
	{
		// 단항 호출을 처리하는 클래스입니다. IAsyncCall 인터페이스를 구현합니다.
		class FUnaryCall : public IAsyncCall
		{
		public:
			FUnaryCall(FHktRpcProxy* InProxy, const typename TRpcTrait::TRequest& InRequest, std::function<void(const grpc::Status&, const typename TRpcTrait::TResponse&)> InCallback)
				: Proxy(InProxy), Cb(InCallback)
			{
				// Trait에 정의된 함수 포인터를 사용하여 비동기 호출을 준비합니다.
				ResponseReader = (Proxy->Stub.get()->*TRpcTrait::PrepareAsyncFunc)(&Context, InRequest, &Proxy->CompletionQueue);
				ResponseReader->StartCall();
				
				// 응답이 오면 실행될 로직을 정의하고 CompletionQueue에 태그로 this를 등록합니다.
				ResponseReader->Finish(&Response, &Status, this);
			}

			void Proceed(bool bOk) override
			{
				// bOk가 false이면 RPC가 실패했음을 의미합니다.
				if (!bOk)
				{
					Status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "RPC failed or cancelled");
				}
				
				// 등록된 콜백을 실행합니다.
				Cb(Status, Response);

				// 작업이 완료되었으므로 객체를 삭제합니다.
				delete this;
			}

		private:
			FHktRpcProxy* Proxy;
			grpc::ClientContext Context;
			grpc::Status Status;
			typename TRpcTrait::TResponse Response;
			std::unique_ptr<grpc::ClientAsyncResponseReader<typename TRpcTrait::TResponse>> ResponseReader;
			std::function<void(const grpc::Status&, const typename TRpcTrait::TResponse&)> Cb;
		};

		// FUnaryCall 인스턴스를 생성합니다. 이 인스턴스는 Proceed가 호출될 때 스스로 삭제됩니다.
		new FUnaryCall(this, Request, Callback);
	}

	// 서버 스트리밍 RPC를 호출하는 템플릿 함수
	template<typename TRpcTrait>
	void CallStream(const typename TRpcTrait::TRequest& Request, std::function<void(const grpc::Status&, const typename TRpcTrait::TResponse&)> Callback)
	{
		// 서버 스트리밍 호출을 상태 머신으로 관리하는 클래스입니다.
		class FStreamCall : public IAsyncCall
		{
		public:
			FStreamCall(FHktRpcProxy* InProxy, const typename TRpcTrait::TRequest& InRequest, std::function<void(const grpc::Status&, const typename TRpcTrait::TResponse&)> InCallback)
				: Proxy(InProxy), Cb(InCallback), State(EState::READING)
			{
				// Trait에 정의된 함수 포인터를 사용하여 비동기 스트림 호출을 준비합니다.
				Reader = (Proxy->Stub.get()->*TRpcTrait::PrepareAsyncFunc)(&Context, InRequest, &Proxy->CompletionQueue);
				Reader->StartCall(this); // StartCall 태그는 스트림이 준비되었음을 알립니다.

				// StartCall이 완료되면 Proceed가 호출되고, 그때 첫 Read를 시작합니다.
			}

			void Proceed(bool bOk) override
			{
				if (State == EState::STARTING)
				{
					if(bOk)
					{
						// 스트림이 성공적으로 시작되었습니다. 첫 메시지를 읽습니다.
						State = EState::READING;
						Reader->Read(&Response, this);
					}
					else
					{
						// 스트림 시작 실패. 종료 상태로 넘어갑니다.
						State = EState::FINISHING;
						Reader->Finish(&Status, this);
					}
				}
				else if (State == EState::READING)
				{
					if (bOk)
					{
						// 메시지를 성공적으로 읽었습니다. 콜백을 호출합니다.
						Cb(grpc::Status::OK, Response);
						// 다음 메시지를 읽습니다.
						Reader->Read(&Response, this);
					}
					else
					{
						// 스트림이 끝났습니다 (bOk == false). 최종 상태를 얻기 위해 Finish를 호출합니다.
						State = EState::FINISHING;
						Reader->Finish(&Status, this);
					}
				}
				else if (State == EState::FINISHING)
				{
					// 최종 상태를 받았습니다. 마지막으로 콜백을 호출하고 객체를 삭제합니다.
					Cb(Status, Response);
					delete this;
				}
			}

		private:
			FHktRpcProxy* Proxy;
			grpc::ClientContext Context;
			grpc::Status Status;
			typename TRpcTrait::TResponse Response;
			std::unique_ptr<grpc::ClientAsyncReader<typename TRpcTrait::TResponse>> Reader;
			std::function<void(const grpc::Status&, const typename TRpcTrait::TResponse&)> Cb;
			
			enum class EState { STARTING, READING, FINISHING };
			EState State = EState::STARTING;
		};
		
		// FStreamCall 인스턴스를 생성합니다. 이 인스턴스는 스트림이 끝날 때 스스로 삭제됩니다.
		new FStreamCall(this, Request, Callback);
	}


private:
	// CompletionQueue를 폴링하고 콜백을 실행하는 워커 스레드 함수
	void ProcessResponses();

	std::unique_ptr<hkt::HktRpcService::Stub> Stub;
	grpc::CompletionQueue CompletionQueue;
	std::thread WorkerThread;
};

