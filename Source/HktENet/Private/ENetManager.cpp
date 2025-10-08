#include "ENetManager.h"

// FENetManager의 구현
FENetManager::FENetManager()
{
    // ENet 라이브러리 초기화
    if (enet_initialize() != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("An error occurred while initializing ENet."));
        bIsInitialized = false;
        return;
    }
    bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("ENet Initialized successfully."));
}

FENetManager::~FENetManager()
{
    // 소멸 시 ENet 관련 리소스 정리
    Stop();
    if (bIsInitialized)
    {
        enet_deinitialize();
        UE_LOG(LogTemp, Log, TEXT("ENet Deinitialized."));
    }
}

void FENetManager::Tick()
{
    if (!Host)
    {
        return;
    }

    ENetEvent Event;
    // non-blocking 방식으로 네트워크 이벤트 처리
    while (enet_host_service(Host, &Event, 0) > 0)
    {
        switch (Event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            {
                UE_LOG(LogTemp, Log, TEXT("A new client connected from %x:%u."), Event.peer->address.host, Event.peer->address.port);
                // 연결 이벤트가 발생하면 델리게이트 호출
                if (OnClientConnected.IsBound())
                {
                    OnClientConnected.Broadcast(Event.peer);
                }
            }
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                // 패킷 수신 시 TArray로 데이터 복사 후 델리게이트 호출
                TArray<uint8> ReceivedData;
                ReceivedData.Append(Event.packet->data, Event.packet->dataLength);
                
                if (OnPacketReceived.IsBound())
                {
                    OnPacketReceived.Broadcast(Event.peer, ReceivedData);
                }
                // 패킷 리소스 해제
                enet_packet_destroy(Event.packet);
            }
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            {
                UE_LOG(LogTemp, Log, TEXT("%x:%u disconnected."), Event.peer->address.host, Event.peer->address.port);
                
                // [수정] 연결이 끊긴 클라이언트를 모든 그룹에서 자동으로 제거
                RemovePeerFromAllGroups(Event.peer);

                // 연결 종료 이벤트가 발생하면 델리게이트 호출
                if (OnClientDisconnected.IsBound())
                {
                    OnClientDisconnected.Broadcast(Event.peer);
                }
                if (ServerPeer == Event.peer)
                {
                    ServerPeer = nullptr;
                }
            }
            break;
        case ENET_EVENT_TYPE_NONE:
            break;
        }
    }
}

bool FENetManager::StartServer(uint16 Port, int32 MaxClients)
{
    if (!bIsInitialized || Host) return false;

    ENetAddress Address;
    Address.host = ENET_HOST_ANY;
    Address.port = Port;

    // 서버 호스트 생성 (2개 채널)
    Host = enet_host_create(&Address, MaxClients, 2, 0, 0);

    if (Host == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("An error occurred while trying to create an ENet server host."));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("ENet Server started on port %d"), Port);
    return true;
}

bool FENetManager::StartClient(const FString& HostName, uint16 Port)
{
    if (!bIsInitialized || Host) return false;

    // 클라이언트 호스트 생성
    Host = enet_host_create(NULL, 1, 2, 0, 0);
    if (Host == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("An error occurred while trying to create an ENet client host."));
        return false;
    }

    ENetAddress Address;
    enet_address_set_host(&Address, TCHAR_TO_ANSI(*HostName));
    Address.port = Port;

    // 서버에 연결 시도
    ServerPeer = enet_host_connect(Host, &Address, 2, 0);
    if (ServerPeer == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("No available peers for initiating an ENet connection."));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("ENet Client connecting to %s:%d"), *HostName, Port);
    return true;
}

void FENetManager::Stop()
{
    if (Host)
    {
        // 서버/클라이언트 종료 시 그룹 정보 초기화
        ClientGroups.Empty();
        enet_host_destroy(Host);
        Host = nullptr;
        ServerPeer = nullptr;
        UE_LOG(LogTemp, Log, TEXT("ENet Host stopped."));
    }
}

void FENetManager::SendPacketToPeer(ENetPeer* Peer, const TArray<uint8>& Data, ENetPacketFlag Flags)
{
    if (!Peer) return;
    // TArray의 데이터를 기반으로 ENet 패킷 생성
    ENetPacket* Packet = enet_packet_create(Data.GetData(), Data.Num(), Flags);
    // 0번 채널로 패킷 전송
    enet_peer_send(Peer, 0, Packet);
}

void FENetManager::BroadcastPacketToClients(const TArray<uint8>& Data, ENetPacketFlag Flags)
{
    if (!Host) return;
    ENetPacket* Packet = enet_packet_create(Data.GetData(), Data.Num(), Flags);
    enet_host_broadcast(Host, 0, Packet);
}

void FENetManager::SendPacketToServer(const TArray<uint8>& Data, ENetPacketFlag Flags)
{
    if (!ServerPeer) return;
    SendPacketToPeer(ServerPeer, Data, Flags);
}

// [추가] 특정 그룹에 속한 모든 클라이언트에게 데이터 전송
void FENetManager::SendPacketToGroup(const FName& GroupName, const TArray<uint8>& Data, ENetPacketFlag Flags)
{
    if (const TArray<ENetPeer*>* PeersInGroup = ClientGroups.Find(GroupName))
    {
        for (ENetPeer* Peer : *PeersInGroup)
        {
            SendPacketToPeer(Peer, Data, Flags);
        }
    }
}

// [추가] 특정 Peer를 그룹에 추가
void FENetManager::AddPeerToGroup(ENetPeer* Peer, const FName& GroupName)
{
    if (!Peer) return;
    // FindOrAdd: GroupName에 해당하는 배열이 있으면 반환하고, 없으면 새로 생성해서 반환
    TArray<ENetPeer*>& PeersInGroup = ClientGroups.FindOrAdd(GroupName);
    // AddUnique: 배열에 해당 Peer가 없는 경우에만 추가
    PeersInGroup.AddUnique(Peer);
}

// [추가] 특정 Peer를 그룹에서 제거
void FENetManager::RemovePeerFromGroup(ENetPeer* Peer, const FName& GroupName)
{
    if (!Peer) return;
    if (TArray<ENetPeer*>* PeersInGroup = ClientGroups.Find(GroupName))
    {
        PeersInGroup->Remove(Peer);
        // 그룹이 비었다면 맵에서 해당 그룹 키를 제거할 수도 있습니다 (선택사항)
        if (PeersInGroup->Num() == 0)
        {
            ClientGroups.Remove(GroupName);
        }
    }
}

// [추가] 특정 Peer를 모든 그룹에서 제거 (클라이언트 연결 종료 시 호출)
void FENetManager::RemovePeerFromAllGroups(ENetPeer* Peer)
{
    if (!Peer) return;
    // 모든 그룹을 순회
    for (auto It = ClientGroups.CreateIterator(); It; ++It)
    {
        It.Value().Remove(Peer);
        // 만약 그룹이 비게 되면 맵에서 제거
        if (It.Value().Num() == 0)
        {
            It.RemoveCurrent();
        }
    }
}

