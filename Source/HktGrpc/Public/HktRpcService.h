#pragma once

#include "HktBase.h"
#include "HktGrpc.h"
#include "HktRpcTraits.h"
#include "HktDef.h"
#include <memory>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <type_traits>

// --- Generic Session & Group Management ---

// 템플릿화된 세션 인터페이스입니다.
template<typename TPacket>
class ISession
{
public:
	virtual ~ISession() = default;
	// 이 세션에 특정 타입의 패킷을 비동기적으로 보냅니다.
	virtual void Write(const TPacket& Packet) = 0;
	// 세션의 고유 ID를 반환합니다.
	virtual uint64_t GetSessionId() const = 0;
};

// 특정 그룹에 속한 템플릿화된 세션들을 관리하고 메시지를 브로드캐스팅합니다.
template<typename TPacket>
class TGroup
{
public:
	void AddSession(ISession<TPacket>* Session)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		if (Session)
		{
			Sessions[Session->GetSessionId()] = Session;
		}
	}

	void RemoveSession(uint64_t SessionId)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		Sessions.erase(SessionId);
	}

	void Broadcast(const TPacket& Packet)
	{
		std::vector<ISession<TPacket>*> TempSessions;
		{
			std::lock_guard<std::mutex> Lock(Mutex);
			TempSessions.reserve(Sessions.size());
			for (const auto& Pair : Sessions)
			{
				TempSessions.push_back(Pair.second);
			}
		}

		for (const auto& Session : TempSessions)
		{
			Session->Write(Packet);
		}
	}

private:
	std::mutex Mutex;
	std::unordered_map<uint64_t, ISession<TPacket>*> Sessions;
};

// 모든 그룹을 관리하는 템플릿화된 싱글턴 클래스입니다.
template<typename TPacket>
class TSessionManager
{
public:
	static TSessionManager& Get()
	{
		static TSessionManager Instance;
		return Instance;
	}

	void RegisterSession(int32_t GroupId, ISession<TPacket>* Session)
	{
		if (Session)
		{
			GetOrCreateGroup(GroupId)->AddSession(Session);
		}
	}

	void UnregisterSession(int32_t GroupId, uint64_t SessionId)
	{
		if (auto Group = GetOrCreateGroup(GroupId))
		{
			Group->RemoveSession(SessionId);
		}
	}

	void BroadcastToGroup(int32_t GroupId, const TPacket& Packet)
	{
		std::shared_ptr<TGroup<TPacket>> Group;
		{
			std::lock_guard<std::mutex> Lock(Mutex);
			auto It = Groups.find(GroupId);
			if (It != Groups.end())
			{
				Group = It->second;
			}
		}

		if (Group)
		{
			Group->Broadcast(Packet);
		}
	}

private:
	TSessionManager() = default;
	std::shared_ptr<TGroup<TPacket>> GetOrCreateGroup(int32_t GroupId)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto It = Groups.find(GroupId);
		if (It == Groups.end())
		{
			auto NewGroup = std::make_shared<TGroup<TPacket>>();
			Groups[GroupId] = NewGroup;
			return NewGroup;
		}
		return It->second;
	}

	std::mutex Mutex;
	std::unordered_map<int32_t, std::shared_ptr<TGroup<TPacket>>> Groups;
};


// --- RPC Service ---

class HKTBASE_API FHktRpcService
{
private:
	// 모든 비동기 작업 태그가 구현해야 하는 공통 인터페이스입니다.
	class FRpcCallHandlerBase
	{
	public:
		virtual ~FRpcCallHandlerBase() = default;
		virtual void Proceed(bool bOk) = 0;
	};

	// 단항 RPC 요청을 처리하는 템플릿 핸들러
	template<typename TRpcTrait>
	class FUnaryCallHandler : public FRpcCallHandlerBase
	{
	public:
		FUnaryCallHandler(hkt::HktRpcService::AsyncService* InService, grpc::ServerCompletionQueue* InCQ)
			: Service(InService), CQ(InCQ), Responder(&Context), Status(ECallStatus::CREATE)
		{
			Proceed(true);
		}

		void Proceed(bool bOk) override
		{
			if (!bOk)
			{
				UE_LOG(LogHktRpc, Error, TEXT("UnaryCallHandler failed."));
				delete this;
				return;
			}
			if (Status == ECallStatus::CREATE)
			{
				Status = ECallStatus::PROCESS;
				(Service->*TRpcTrait::RequestRpcFunc)(&Context, &Request, &Responder, CQ, CQ, this);
			}
			else if (Status == ECallStatus::PROCESS)
			{
				new FUnaryCallHandler<TRpcTrait>(Service, CQ);
				UE_LOG(LogHktRpc, Log, TEXT("UnaryCallHandler: Processing request."));
				// Trait에 TPacket이 정의되어 있는지 확인하고, 해당 타입의 세션 매니저를 사용합니다.
				if constexpr (std::is_same_v<TRpcTrait, HktRpc::FExecuteBehaviorRpcTrait>)
				{
					static int32 NextBehaviorId = 0;
					Request.mutable_packet()->set_behavior_id(++NextBehaviorId); // 서버에서 ID 할당

					using TPacket = typename TRpcTrait::TPacket;
					TSessionManager<TPacket>::Get().BroadcastToGroup(Request.group_id(), Request.packet());
				}
				Status = ECallStatus::FINISH;
				Responder.Finish(Response, grpc::Status::OK, this);
			}
			else
			{
				UE_LOG(LogHktRpc, Log, TEXT("UnaryCallHandler: Finished."));
				delete this;
			}
		}

	private:
		hkt::HktRpcService::AsyncService* Service;
		grpc::ServerCompletionQueue* CQ;
		grpc::ServerContext Context;
		typename TRpcTrait::TRequest Request;
		typename TRpcTrait::TResponse Response;
		grpc::ServerAsyncResponseWriter<typename TRpcTrait::TResponse> Responder;
		enum class ECallStatus { CREATE, PROCESS, FINISH };
		ECallStatus Status;
	};

	// 서버 스트리밍 RPC 요청을 처리하는 템플릿 세션 핸들러
	template<typename TRpcTrait>
	class FStreamSessionHandler : public ISession<typename TRpcTrait::TPacket>
	{
	public:
		using TPacket = typename TRpcTrait::TPacket;

		FStreamSessionHandler(hkt::HktRpcService::AsyncService* InService, grpc::ServerCompletionQueue* InCQ)
			: Service(InService), CQ(InCQ), Writer(&Context)
		{
			static std::atomic<uint64_t> NextSessionId = 1;
			SessionId = NextSessionId.fetch_add(1);
			UE_LOG(LogHktRpc, Log, TEXT("StreamSessionHandler created. SessionId: %llu"), SessionId);
			Start();
		}

		~FStreamSessionHandler()
		{
			UE_LOG(LogHktRpc, Log, TEXT("StreamSessionHandler destroyed. SessionId: %llu, GroupId: %d"), SessionId, GroupId);
			if (GroupId != -1)
			{
				//TPacket DisconnectPacket;
				//// 서버가 보내는 시스템 메시지이므로 owner_player_id는 0 또는 다른 특정 ID로 설정할 수 있습니다.
				//DisconnectPacket.set_owner_player_id(0);
				//
				//// DestroyPacket을 사용하여 접속이 끊긴 플레이어의 ID를 전파합니다.
				//hkt::DestroyPacket* Destroy = DisconnectPacket.mutable_destroy_packet();
				//Destroy->set_behavior_id(PlayerId);
				//
				//TSessionManager<TPacket>::Get().BroadcastToGroup(GroupId, DisconnectPacket);

				// 해당 패킷 타입의 세션 매니저를 통해 세션을 등록 해제합니다.
				TSessionManager<TPacket>::Get().UnregisterSession(GroupId, SessionId);
			}
		}

		// Trait에서 정의된 TPacket 타입을 인자로 받습니다.
		void Write(const TPacket& Packet) override
		{
			typename TRpcTrait::TResponse Response = TRpcTrait::CreateResponseFromPacket(Packet);

			std::lock_guard<std::mutex> lock(QueueMutex);
			OutgoingMessages.push(Response);
			if (!bIsWriting)
			{
				UE_LOG(LogHktRpc, Verbose, TEXT("Writing packet to SessionId: %llu"), GetSessionId());
				bIsWriting = true;
				Writer.Write(OutgoingMessages.front(), new FWriteOperation(this));
			}
		}
		uint64_t GetSessionId() const override { return SessionId; }

	private:
		friend class FConnectOperation;
		friend class FWriteOperation;

		void Start()
		{
			(Service->*TRpcTrait::RequestRpcFunc)(&Context, &Request, &Writer, CQ, CQ,
				new FConnectOperation(this));
		}

		// 비동기 작업의 '전략'을 정의하는 기본 클래스입니다.
		class FAsyncOperation : public FRpcCallHandlerBase
		{
		public:
			explicit FAsyncOperation(FStreamSessionHandler* InSession) : Session(InSession) {}
			void Proceed(bool bOk) final
			{
				if (!bOk)
				{
					UE_LOG(LogHktRpc, Warning, TEXT("AsyncOperation failed for SessionId: %llu. Deleting session."), Session->GetSessionId());
					delete Session;
				}
				else
				{
					Execute();
				}
				delete this;
			}
		protected:
			virtual void Execute() = 0;
			FStreamSessionHandler* Session;
		};

		// 클라이언트 연결 요청을 처리하는 구체적인 작업 클래스입니다.
		class FConnectOperation : public FAsyncOperation
		{
		public:
			using FAsyncOperation::FAsyncOperation;
			void Execute() override
			{
				new FStreamSessionHandler<TRpcTrait>(this->Session->Service, this->Session->CQ);
				
				this->Session->GroupId = this->Session->Request.group_id();
				UE_LOG(LogHktRpc, Log, TEXT("New client connected. SessionId: %llu, GroupId: %d"), this->Session->GetSessionId(), this->Session->GroupId);
				// 해당 패킷 타입의 세션 매니저를 통해 세션을 등록합니다.
				TSessionManager<TPacket>::Get().RegisterSession(this->Session->GroupId, this->Session);
			}
		};

		// 메시지 쓰기 완료를 처리하는 구체적인 작업 클래스입니다.
		class FWriteOperation : public FAsyncOperation
		{
		public:
			using FAsyncOperation::FAsyncOperation;
			void Execute() override
			{
				std::lock_guard<std::mutex> lock(this->Session->QueueMutex);
				this->Session->OutgoingMessages.pop();
				if (!this->Session->OutgoingMessages.empty())
				{
					this->Session->Writer.Write(this->Session->OutgoingMessages.front(), new FWriteOperation(this->Session));
				}
				else
				{
					this->Session->bIsWriting = false;
				}
				UE_LOG(LogHktRpc, Verbose, TEXT("Write complete for SessionId: %llu"), this->Session->GetSessionId());
			}
		};

	private:
		hkt::HktRpcService::AsyncService* Service;
		grpc::ServerCompletionQueue* CQ;
		grpc::ServerContext Context;
		grpc::ServerAsyncWriter<typename TRpcTrait::TResponse> Writer;

		uint64_t SessionId;
		int32_t GroupId = -1;
		typename TRpcTrait::TRequest Request;
		
		std::mutex QueueMutex;
		std::queue<typename TRpcTrait::TResponse> OutgoingMessages;
		bool bIsWriting = false;
	};

public:
	FHktRpcService() = default;
	~FHktRpcService();

	void Run(const std::string& ServerAddress);

private:
	void HandleRpcs(int CQIndex);

	hkt::HktRpcService::AsyncService Service;
	std::unique_ptr<grpc::Server> Server;
	std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> CQs;
	std::vector<std::thread> WorkerThreads;
};

