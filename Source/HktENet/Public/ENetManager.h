#pragma once

#include "CoreMinimal.h" // 델리게이트, TArray, FString 등을 위해 유지

// ENet 헤더 include (컴파일 오류 방지를 위해 전처리)
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "enet/enet.h"

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// 델리게이트 선언 (유지)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnENetPacketReceived, ENetPeer* /*Peer*/, const TArray<uint8>& /*Data*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnENetClientConnected, ENetPeer* /*Peer*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnENetClientDisconnected, ENetPeer* /*Peer*/);

/**
 * Unreal Engine의 Core모듈에만 의존하는 일반 ENet 관리 클래스입니다.
 * UObject가 아니므로, 직접 생성하고 외부 루프에서 Tick()을 주기적으로 호출해주어야 합니다.
 */
class HKTENET_API FENetManager
{
public:
    // 생성자 및 소멸자
    FENetManager();
    ~FENetManager();

    // 복사 및 대입 방지
    FENetManager(const FENetManager&) = delete;
    FENetManager& operator=(const FENetManager&) = delete;

    // 매 틱(또는 주기적으로) 호출되어 네트워크 이벤트를 처리하는 함수
    void Tick();

    // 서버 시작
    bool StartServer(uint16 Port, int32 MaxClients = 32);
    // 클라이언트 시작 및 서버에 연결
    bool StartClient(const FString& HostName, uint16 Port);
    // 서버/클라이언트 종료
    void Stop();

    // --- 패킷 전송 함수 ---
    void SendPacketToPeer(ENetPeer* Peer, const TArray<uint8>& Data, ENetPacketFlag Flags = ENET_PACKET_FLAG_RELIABLE);
    void BroadcastPacketToClients(const TArray<uint8>& Data, ENetPacketFlag Flags = ENET_PACKET_FLAG_RELIABLE);
    void SendPacketToServer(const TArray<uint8>& Data, ENetPacketFlag Flags = ENET_PACKET_FLAG_RELIABLE);
    // [추가] 특정 그룹에 속한 모든 클라이언트에게 데이터 전송
    void SendPacketToGroup(const FName& GroupName, const TArray<uint8>& Data, ENetPacketFlag Flags = ENET_PACKET_FLAG_RELIABLE);

    // --- 그룹 관리 함수 ---
    // [추가] 특정 Peer를 그룹에 추가
    void AddPeerToGroup(ENetPeer* Peer, const FName& GroupName);
    // [추가] 특정 Peer를 그룹에서 제거
    void RemovePeerFromGroup(ENetPeer* Peer, const FName& GroupName);
    // [추가] 특정 Peer를 모든 그룹에서 제거
    void RemovePeerFromAllGroups(ENetPeer* Peer);

    // 델리게이트 인스턴스
    FOnENetPacketReceived OnPacketReceived;
    FOnENetClientConnected OnClientConnected;
    FOnENetClientDisconnected OnClientDisconnected;

private:
    // ENet 호스트 (서버 또는 클라이언트)
    ENetHost* Host = nullptr;
    // 클라이언트인 경우, 서버와의 연결을 나타내는 Peer
    ENetPeer* ServerPeer = nullptr;

    bool bIsInitialized = false;

    // [추가] 그룹명과 해당 그룹에 속한 Peer들의 배열을 매핑하는 TMap
    TMap<FName, TArray<ENetPeer*>> ClientGroups;
};

