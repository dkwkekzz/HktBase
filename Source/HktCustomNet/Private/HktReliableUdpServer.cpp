#include "HktReliableUdpServer.h"
#include "Common/UdpSocketBuilder.h"
#include "SocketSubsystem.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktCustomNetServer, Log, All);

FHktReliableUdpServer::FHktReliableUdpServer(uint16 InPort)
    : Port(InPort)
    , bIsStopping(false)
{
}

FHktReliableUdpServer::~FHktReliableUdpServer()
{
    Stop();
}

void FHktReliableUdpServer::Start()
{
    // 서버의 패킷 수신을 전담할 스레드 생성 및 시작
    ReceiverThread = FRunnableThread::Create(this, TEXT("UdpServerReceiverThread"), 0, TPri_Normal);
}

void FHktReliableUdpServer::Stop()
{
    if (bIsStopping)
    {
        return;
    }

    bIsStopping = true;

    if (ReceiverThread)
    {
        ReceiverThread->WaitForCompletion();
        delete ReceiverThread;
        ReceiverThread = nullptr;
    }

    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
    UE_LOG(LogHktCustomNetServer, Log, TEXT("Server stopped."));
}

void FHktReliableUdpServer::Tick()
{
    // 메인 스레드에서 매 프레임 다음 작업 수행:
    // 1. 수신 큐에 쌓인 패킷들을 처리
    ProcessReceivedPackets();
    // 2. Ack를 받지 못한 패킷이 있다면 재전송
    CheckForResends();
    // 3. 일정 시간 응답 없는 클라이언트 타임아웃 처리
    CheckForTimeouts();
}

bool FHktReliableUdpServer::Init()
{
    FString SocketName = FString::Printf(TEXT("UdpServerListenSocket_%d"), Port);
    // 지정된 포트에 바인딩되는 논블로킹 UDP 소켓 생성
    ListenSocket = FUdpSocketBuilder(*SocketName).AsNonBlocking().BoundToPort(Port);

    if (ListenSocket)
    {
        // 소켓 버퍼 크기를 넉넉하게 설정
        int32 BufferSize = 2 * 1024 * 1024;
        ListenSocket->SetReceiveBufferSize(BufferSize, BufferSize);
        ListenSocket->SetSendBufferSize(BufferSize, BufferSize);
        UE_LOG(LogHktCustomNetServer, Log, TEXT("UDP Server socket created and listening on port %d"), Port);
        return true;
    }

    UE_LOG(LogHktCustomNetServer, Error, TEXT("Failed to create UDP Server socket on port %d"), Port);
    return false;
}

uint32 FHktReliableUdpServer::Run()
{
    // 이 함수는 'UdpServerReceiverThread' 스레드에서 실행됩니다.
    TArray<uint8> ReceiveBuffer;
    ReceiveBuffer.SetNumUninitialized(65535);

    while (!bIsStopping)
    {
        uint32 PendingDataSize = 0;
        // 읽을 데이터가 있는지 확인
        if (ListenSocket->HasPendingData(PendingDataSize))
        {
            TSharedRef<FInternetAddr> PeerAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
            int32 BytesRead = 0;

            if (ListenSocket->RecvFrom(ReceiveBuffer.GetData(), ReceiveBuffer.Num(), BytesRead, *PeerAddr))
            {
                if (BytesRead > 0)
                {
                    // 수신된 데이터를 복사하여 메인 스레드가 처리할 큐에 넣음
                    TArray<uint8> ReceivedData;
                    ReceivedData.Append(ReceiveBuffer.GetData(), BytesRead);
                    ReceivedPackets.Enqueue(FReceivedPacket(PeerAddr, MoveTemp(ReceivedData)));
                    UE_LOG(LogHktCustomNetServer, Verbose, TEXT("Socket received %d bytes from %s."), BytesRead, *PeerAddr->ToString(true));
                }
            }
        }
        // CPU 사용량을 줄이기 위해 잠시 대기
        FPlatformProcess::Sleep(0.001f);
    }
    UE_LOG(LogHktCustomNetServer, Log, TEXT("Server receiver thread finished."));
    return 0;
}

void FHktReliableUdpServer::Exit()
{
    Stop();
}

void FHktReliableUdpServer::ProcessReceivedPackets()
{
    // 이 함수는 메인 스레드의 Tick에서 호출됩니다.
    FReceivedPacket Packet;
    while (ReceivedPackets.Dequeue(Packet))
    {
        if (Packet.Data.Num() < sizeof(FPacketHeader)) continue;

        FPacketHeader Header;
        FMemory::Memcpy(&Header, Packet.Data.GetData(), sizeof(FPacketHeader));

        FString ClientAddrStr = Packet.PeerAddress->ToString(true);
        TSharedPtr<FClientConnection> Connection;

        {
            FScopeLock Lock(&ConnectionMutex);
            Connection = Connections.FindRef(ClientAddrStr);
        }

        UE_LOG(LogHktCustomNetServer, Verbose, TEXT("<= Rcvd Packet from %s. Type: %d, Seq: %u, Ack: %u, AckBits: %u"), *ClientAddrStr, (int)Header.Type, Header.Sequence, Header.LastAckedSequence, Header.AckBitfield);

        // 등록되지 않은 클라이언트 처리
        if (!Connection)
        {
            // 'Connect' 타입의 패킷일 경우에만 새로운 연결로 처리
            if (Header.Type == EPacketType::Connect)
            {
                HandleNewConnection(Packet.PeerAddress);
            }
            else
            {
                UE_LOG(LogHktCustomNetServer, Warning, TEXT("Received a packet from an unknown client %s. Ignoring."), *ClientAddrStr);
            }
            continue;
        }

        // 마지막 통신 시간 갱신 (타임아웃 방지)
        Connection->LastReceiveTime = FPlatformTime::Seconds();

        // 클라이언트가 보낸 Ack 정보를 먼저 처리하여 내가 보낸 패킷이 잘 도착했는지 확인
        ProcessAck(Header, Connection);

        switch (Header.Type)
        {
        case EPacketType::Data:
        {
            // 내가 어떤 패킷까지 받았는지 수신 상태 갱신
            UpdateReceivedState(Header.Sequence, Connection);
            // "당신이 보낸 데이터 잘 받았다"는 의미로 즉시 Ack 전송
            SendAck(Connection);
            UE_LOG(LogHktCustomNetServer, Verbose, TEXT("Processed [Data] packet (Seq: %u) from %s."), Header.Sequence, *ClientAddrStr);
            // TODO: 페이로드(Header 이후의 데이터)를 파싱하여 게임 로직 큐로 전달
            break;
        }
        case EPacketType::Ack:
            // Ack 패킷은 ProcessAck에서 이미 모든 처리가 끝났으므로 별도 작업 없음
            break;
        case EPacketType::Disconnect:
            DisconnectClient(ClientAddrStr, TEXT("Client requested disconnect."));
            break;
        case EPacketType::JoinGroup:
        {
            // 페이로드 크기가 유효한지 확인
            if (Packet.Data.Num() - sizeof(FPacketHeader) == sizeof(int32))
            {
                int32 RequestedGroupId;
                // 페이로드에서 GroupId를 역직렬화
                FMemory::Memcpy(&RequestedGroupId, Packet.Data.GetData() + sizeof(FPacketHeader), sizeof(int32));

                UE_LOG(LogHktCustomNetServer, Log, TEXT("Client %s requested to join group %d."), *ClientAddrStr, RequestedGroupId);

                // 해당 클라이언트를 요청된 그룹에 추가
                JoinGroup(Packet.PeerAddress, RequestedGroupId);
            }
            else
            {
                UE_LOG(LogHktCustomNetServer, Warning, TEXT("Received malformed [JoinGroupRequest] from %s."), *ClientAddrStr);
            }
            break;
        }
        case EPacketType::LeaveGroup:
        {
            if (Packet.Data.Num() - sizeof(FPacketHeader) == sizeof(int32))
            {
                int32 GroupIdToLeave;
                FMemory::Memcpy(&GroupIdToLeave, Packet.Data.GetData() + sizeof(FPacketHeader), sizeof(int32));
                LeaveGroup(Packet.PeerAddress, GroupIdToLeave);
            }
            else
            {
                UE_LOG(LogHktCustomNetServer, Warning, TEXT("Received malformed [LeaveGroup] from %s."), *ClientAddrStr);
            }
            break;
        }
        default:
            break;
        }
    }
}

void FHktReliableUdpServer::SendTo(const TSharedPtr<FInternetAddr>& DstAddr, const TArray<uint8>& Data)
{
    if (!ListenSocket || !DstAddr.IsValid()) return;

    FString AddrStr = DstAddr->ToString(true);
    TSharedPtr<FClientConnection> Connection;
    {
        FScopeLock Lock(&ConnectionMutex);
        Connection = Connections.FindRef(AddrStr);
    }

    if (!Connection)
    {
        UE_LOG(LogHktCustomNetServer, Warning, TEXT("Attempted to send data to an unknown client %s."), *AddrStr);
        return;
    }

    FPacketHeader Header;
    Header.Type = EPacketType::Data;

    {
        FScopeLock Lock(&ConnectionMutex);
        // 이 클라이언트에게 보낼 다음 시퀀스 번호
        Connection->SentSequence++;
        Header.Sequence = Connection->SentSequence;
        // 내가 이 클라이언트로부터 마지막으로 받은 패킷 정보를 헤더에 담음 (Piggybacking Ack)
        Header.LastAckedSequence = Connection->ReceivedSequence;
        Header.AckBitfield = Connection->ReceivedAckBitfield;
    }

    TArray<uint8> PacketData;
    PacketData.Append((uint8*)&Header, sizeof(FPacketHeader));
    PacketData.Append(Data);

    int32 BytesSent = 0;
    ListenSocket->SendTo(PacketData.GetData(), PacketData.Num(), BytesSent, *DstAddr);
    UE_LOG(LogHktCustomNetServer, Verbose, TEXT("=> Sent [Data] to %s. Seq: %u, Ack: %u, AckBits: %u"), *AddrStr, Header.Sequence, Header.LastAckedSequence, Header.AckBitfield);

    {
        FScopeLock Lock(&ConnectionMutex);
        // 재전송을 위해 보낸 패킷 정보 저장
        Connection->PendingAckPackets.Add(Header.Sequence, FPendingPacket(MoveTemp(PacketData), FPlatformTime::Seconds()));
    }
}

void FHktReliableUdpServer::BroadcastToGroup(int32 GroupId, const TArray<uint8>& Data, const TSharedPtr<FInternetAddr>& ExcludeAddr)
{
    FScopeLock Lock(&ConnectionMutex);
    if (const TMap<FString, TSharedPtr<FInternetAddr>>* GroupMembers = Groups.Find(GroupId))
    {
        UE_LOG(LogHktCustomNetServer, Log, TEXT("Broadcasting to group %d (%d members)."), GroupId, GroupMembers->Num());
        for (const auto& Elem : *GroupMembers)
        {
            const TSharedPtr<FInternetAddr>& MemberAddr = Elem.Value;
            if (MemberAddr.IsValid() && (!ExcludeAddr.IsValid() || !MemberAddr->CompareEndpoints(*ExcludeAddr)))
            {
                SendTo(MemberAddr, Data);
            }
        }
    }
}

void FHktReliableUdpServer::ProcessAck(const FPacketHeader& Header, TSharedPtr<FClientConnection> Connection)
{
    FScopeLock Lock(&ConnectionMutex);

    // 1. LastAckedSequence로 가장 최근 패킷이 도착했음을 확인하고 Pending 큐에서 제거
    if (Connection->PendingAckPackets.Contains(Header.LastAckedSequence))
    {
        Connection->PendingAckPackets.Remove(Header.LastAckedSequence);
        UE_LOG(LogHktCustomNetServer, Verbose, TEXT("Ack confirmed for sequence %u from %s."), Header.LastAckedSequence, *Connection->Address->ToString(true));
    }

    // 2. AckBitfield를 순회하며 그 이전 패킷들의 도착 여부도 확인하고 제거
    for (int32 i = 0; i < 32; ++i)
    {
        if ((Header.AckBitfield >> i) & 1)
        {
            uint32 AckedSequence = Header.LastAckedSequence - (i + 1);
            if (Connection->PendingAckPackets.Contains(AckedSequence))
            {
                Connection->PendingAckPackets.Remove(AckedSequence);
                UE_LOG(LogHktCustomNetServer, Verbose, TEXT("Ack confirmed for sequence %u from %s via bitfield."), AckedSequence, *Connection->Address->ToString(true));
            }
        }
    }
}

void FHktReliableUdpServer::UpdateReceivedState(uint32 IncomingSequence, TSharedPtr<FClientConnection> Connection)
{
    FScopeLock Lock(&ConnectionMutex);

    if (IncomingSequence <= Connection->ReceivedSequence - 32)
    {
        return; // 너무 오래된 패킷은 무시
    }

    if (IncomingSequence > Connection->ReceivedSequence)
    {
        // 정상 순서로 패킷 도착
        uint32 Diff = IncomingSequence - Connection->ReceivedSequence;
        Connection->ReceivedAckBitfield = (Diff >= 32) ? 1 : (Connection->ReceivedAckBitfield << Diff) | 1;
        Connection->ReceivedSequence = IncomingSequence;
    }
    else
    {
        // 순서가 뒤바뀌어 패킷 도착 (Out-of-order)
        uint32 Diff = Connection->ReceivedSequence - IncomingSequence;
        Connection->ReceivedAckBitfield |= (1 << (Diff - 1));
    }
    UE_LOG(LogHktCustomNetServer, Verbose, TEXT("Receive state updated for %s. Last Rcvd Seq: %u, Rcvd Bits: %u"), *Connection->Address->ToString(true), Connection->ReceivedSequence, Connection->ReceivedAckBitfield);
}


void FHktReliableUdpServer::CheckForResends()
{
    double CurrentTime = FPlatformTime::Seconds();
    TArray<FString> ClientsToDisconnect;

    FScopeLock Lock(&ConnectionMutex);
    // 모든 연결된 클라이언트를 순회
    for (auto& Elem : Connections)
    {
        TSharedPtr<FClientConnection> Connection = Elem.Value;
        // 해당 클라이언트의 Ack 대기 중인 패킷들을 순회
        for (auto& PacketElem : Connection->PendingAckPackets)
        {
            FPendingPacket& PendingPacket = PacketElem.Value;
            if (CurrentTime - PendingPacket.SentTime > ResendTimeout)
            {
                // 최대 재전송 횟수 초과 시 연결 종료 목록에 추가
                if (PendingPacket.Retries >= MaxRetries)
                {
                    ClientsToDisconnect.Add(Elem.Key);
                    break;
                }

                // 패킷 재전송
                int32 BytesSent = 0;
                ListenSocket->SendTo(PendingPacket.PacketData.GetData(), PendingPacket.PacketData.Num(), BytesSent, *Connection->Address);

                PendingPacket.SentTime = CurrentTime;
                PendingPacket.Retries++;
                UE_LOG(LogHktCustomNetServer, Warning, TEXT("Packet timeout. Resending (Seq:%u) to %s, Retry: %d/%d"), PacketElem.Key, *Elem.Key, PendingPacket.Retries, MaxRetries);
            }
        }
    }

    // 재전송 초과로 연결을 끊어야 할 클라이언트가 있다면 처리
    if (ClientsToDisconnect.Num() > 0)
    {
        for (const FString& AddrStr : ClientsToDisconnect)
        {
            DisconnectClient(AddrStr, TEXT("Packet resend limit exceeded. Not responding."));
        }
    }
}

void FHktReliableUdpServer::CheckForTimeouts()
{
    const double CurrentTime = FPlatformTime::Seconds();
    TArray<FString> TimedOutClients;

    {
        FScopeLock Lock(&ConnectionMutex);
        // 모든 연결된 클라이언트를 순회하며 타임아웃 검사
        for (const auto& Elem : Connections)
        {
            const FString& ClientAddrStr = Elem.Key;
            const TSharedPtr<FClientConnection>& Connection = Elem.Value;

            if (CurrentTime - Connection->LastReceiveTime > ClientTimeoutDuration)
            {
                TimedOutClients.Add(ClientAddrStr);
                UE_LOG(LogHktCustomNetServer, Log, TEXT("Client %s timed out."), *ClientAddrStr);
            }
        }
    }

    // 타임아웃된 클라이언트들의 연결을 종료
    for (const FString& ClientAddrStr : TimedOutClients)
    {
        DisconnectClient(ClientAddrStr, TEXT("Connection timed out."));
    }
}


void FHktReliableUdpServer::HandleNewConnection(const TSharedPtr<FInternetAddr>& NewAddr)
{
    FScopeLock Lock(&ConnectionMutex);
    FString AddrStr = NewAddr->ToString(true);
    if (!Connections.Contains(AddrStr))
    {
        // 새로운 클라이언트를 위한 Connection 정보 생성
        TSharedPtr<FClientConnection> NewConnection = MakeShared<FClientConnection>();
        NewConnection->Address = NewAddr;
        NewConnection->LastReceiveTime = FPlatformTime::Seconds();
        // Connections 맵에 등록
        Connections.Add(AddrStr, NewConnection);
        UE_LOG(LogHktCustomNetServer, Log, TEXT("New client connected: %s. Total clients: %d"), *AddrStr, Connections.Num());

        // 연결 수락 의미로 Ack 전송 (Handshake 완료)
        SendAck(NewConnection);
    }
}

void FHktReliableUdpServer::DisconnectClient(const FString& ClientAddrStr, const FString& Reason)
{
    FScopeLock Lock(&ConnectionMutex);
    TSharedPtr<FClientConnection> Connection;
    if (Connections.RemoveAndCopyValue(ClientAddrStr, Connection))
    {
        UE_LOG(LogHktCustomNetServer, Log, TEXT("Client %s disconnected. Reason: %s. Total clients: %d"), *ClientAddrStr, *Reason, Connections.Num());
        
        // 클라이언트가 속해있던 모든 그룹에서 제거
        for (int32 GroupId : Connection->GroupIds)
        {
            if (TMap<FString, TSharedPtr<FInternetAddr>>* GroupMembers = Groups.Find(GroupId))
            {
                GroupMembers->Remove(ClientAddrStr);

                if (GroupMembers->Num() == 0)
                {
                    Groups.Remove(GroupId);
                }
            }
        }
    }
}


void FHktReliableUdpServer::JoinGroup(const TSharedPtr<FInternetAddr>& ClientAddr, int32 GroupId)
{
    FScopeLock Lock(&ConnectionMutex);
    FString AddrStr = ClientAddr->ToString(true);
    if (TSharedPtr<FClientConnection> Connection = Connections.FindRef(AddrStr))
    {
        // 이미 그룹에 속해있는지 확인
        if (Connection->GroupIds.Contains(GroupId))
        {
            UE_LOG(LogHktCustomNetServer, Log, TEXT("Client %s is already in group %d"), *AddrStr, GroupId);
            return;
        }

        // 클라이언트의 그룹 목록에 추가
        Connection->GroupIds.Add(GroupId);

        // 전체 그룹 목록에 클라이언트 추가
        TMap<FString, TSharedPtr<FInternetAddr>>& GroupMembers = Groups.FindOrAdd(GroupId);
        GroupMembers.Add(AddrStr, ClientAddr);

        UE_LOG(LogHktCustomNetServer, Log, TEXT("Client %s joined group %d. Group now has %d members."), *AddrStr, GroupId, GroupMembers.Num());
    }
    else
    {
        UE_LOG(LogHktCustomNetServer, Warning, TEXT("Attempted to join group for unknown client %s"), *AddrStr);
    }
}

void FHktReliableUdpServer::LeaveGroup(const TSharedPtr<FInternetAddr>& ClientAddr, int32 GroupId)
{
    FScopeLock Lock(&ConnectionMutex);
    FString AddrStr = ClientAddr->ToString(true);
    if (TSharedPtr<FClientConnection> Connection = Connections.FindRef(AddrStr))
    {
        // 클라이언트가 해당 그룹에 속해있는지 확인
        if (!Connection->GroupIds.Contains(GroupId))
        {
            UE_LOG(LogHktCustomNetServer, Warning, TEXT("Client %s is not in group %d"), *AddrStr, GroupId);
            return;
        }

        // 클라이언트의 그룹 목록에서 제거
        Connection->GroupIds.Remove(GroupId);

        // 전체 그룹 목록에서 클라이언트 제거
        if (TMap<FString, TSharedPtr<FInternetAddr>>* GroupMembers = Groups.Find(GroupId))
        {
            GroupMembers->Remove(AddrStr);

            UE_LOG(LogHktCustomNetServer, Log, TEXT("Client %s left group %d. Group now has %d members."), *AddrStr, GroupId, GroupMembers->Num());

            // 그룹이 비었다면 맵에서 제거
            if (GroupMembers->Num() == 0)
            {
                Groups.Remove(GroupId);
                UE_LOG(LogHktCustomNetServer, Log, TEXT("Group %d is now empty and has been removed."), GroupId);
            }
        }
    }
     else
    {
        UE_LOG(LogHktCustomNetServer, Warning, TEXT("Attempted to leave group for unknown client %s"), *AddrStr);
    }
}

void FHktReliableUdpServer::SendAck(TSharedPtr<FClientConnection> Connection)
{
    FScopeLock Lock(&ConnectionMutex);
    FPacketHeader AckHeader;
    AckHeader.Type = EPacketType::Ack;
    AckHeader.Sequence = 0; // Ack 패킷 자체는 시퀀스 번호가 필요 없음
    AckHeader.LastAckedSequence = Connection->ReceivedSequence;
    AckHeader.AckBitfield = Connection->ReceivedAckBitfield;

    int32 BytesSent = 0;
    ListenSocket->SendTo((uint8*)&AckHeader, sizeof(FPacketHeader), BytesSent, *Connection->Address);
    UE_LOG(LogHktCustomNetServer, Verbose, TEXT("=> Sent [Ack] to %s. Ack: %u, AckBits: %u"), *Connection->Address->ToString(true), AckHeader.LastAckedSequence, AckHeader.AckBitfield);
}



