#pragma once

#include "HktGrpc.h"

// 이 파일은 proto에 정의된 각 rpc에 대한 메타데이터(타입, 함수 포인터 등)를 정의합니다.
// 새로운 RPC가 추가될 때마다 이 파일에 해당 RPC의 Trait만 추가하면 됩니다.

namespace HktRpc
{
	// ExecuteBehavior (단항 RPC)를 위한 Trait
	struct FExecuteBehaviorRpcTrait
	{
		// 요청 및 응답 메시지 타입 정의
		using TRequest = hkt::BehaviorRequest;
		using TResponse = google::protobuf::Empty;
		using TPacket = hkt::BehaviorPacket; // 스트림에서 실제로 전송될 패킷 타입

		// gRPC Stub에서 비동기 호출을 준비하는 함수의 포인터
		// 클라이언트 측에서 사용됩니다.
		static constexpr auto PrepareAsyncFunc = &hkt::HktRpcService::Stub::PrepareAsyncExecuteBehavior;
		
		// gRPC AsyncService에 요청을 등록하는 함수의 포인터
		// 서버 측에서 사용됩니다.
		static constexpr auto RequestRpcFunc = &hkt::HktRpcService::AsyncService::RequestExecuteBehavior;
	};

	// SyncGroup (서버 스트리밍 RPC)를 위한 Trait
	struct FSyncGroupRpcTrait
	{
		// 요청 및 응답 메시지 타입 정의
		using TRequest = hkt::SyncRequest;
		using TResponse = hkt::SyncResponse; // 스트림을 통해 반복적으로 전달될 메시지 타입
		using TPacket = hkt::BehaviorPacket; // 스트림에서 실제로 전송될 패킷 타입

		// gRPC Stub에서 비동기 스트림 호출을 준비하는 함수의 포인터
		// 클라이언트 측에서 사용됩니다.
		static constexpr auto PrepareAsyncFunc = &hkt::HktRpcService::Stub::PrepareAsyncSyncGroup;

		// gRPC AsyncService에 스트림 요청을 등록하는 함수의 포인터
		// 서버 측에서 사용됩니다.
		static constexpr auto RequestRpcFunc = &hkt::HktRpcService::AsyncService::RequestSyncGroup;
	};
}
