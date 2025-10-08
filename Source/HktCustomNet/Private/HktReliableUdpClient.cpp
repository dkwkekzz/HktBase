#include "HktReliableUdpClient.h"
#include "Common/UdpSocketBuilder.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktCustomNetClient, Log, All);

FHktReliableUdpClient::FHktReliableUdpClient()
    : bIsStopping(false)
    , bIsConnected(false)
{
}

FHktReliableUdpClient::~FHktReliableUdpClient()
{
    Disconnect();
}

bool FHktReliableUdpClient::Connect(const FString& ServerIp, uint16 ServerPort, uint16 ClientPort/*= HktReliableUdp::ClientPort*/)
{
    // 1. 서버 주소 객체 생성 및 설정
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    ServerAddr = SocketSubsystem->CreateInternetAddr();

    bool bIsValid;
    ServerAddr->SetIp(*ServerIp, bIsValid);
    ServerAddr->SetPort(ServerPort);

    if (!bIsValid)
    {
        UE_LOG(LogHktCustomNetClient, Error, TEXT("Invalid server IP: %s"), *ServerIp);
        return false;
    }

    // 2. UDP 소켓 생성
    ClientSocket = FUdpSocketBuilder(TEXT("UdpClientSocket"))
        .AsNonBlocking()
        .BoundToPort(ClientPort);

    if (ClientSocket)
    {
        // 3. 수신 스레드 시작
        ReceiverThread = FRunnableThread::Create(this, TEXT("UdpClientReceiverThread"));

        // 4. 서버에 연결 요청 패킷 전송 (Handshake 시작)
        SendPacket(TArray<uint8>(), EPacketType::Connect);
        UE_LOG(LogHktCustomNetClient, Log, TEXT("Socket created. Sent [Connect] request to %s:%d"), *ServerIp, ServerPort);
        return true;
    }

    UE_LOG(LogHktCustomNetClient, Error, TEXT("Failed to create client socket."));
    return false;
}

void FHktReliableUdpClient::Disconnect()
{
    if (bIsConnected)
    {
        // 서버에 연결 해제를 알림
        SendPacket(TArray<uint8>(), EPacketType::Disconnect);
        UE_LOG(LogHktCustomNetClient, Log, TEXT("Sent [Disconnect] message to server."));
    }

    // 스레드와 소켓 정리
    bIsStopping = true;
    if (ReceiverThread)
    {
        ReceiverThread->WaitForCompletion();
        delete ReceiverThread;
        ReceiverThread = nullptr;
    }

    if (ClientSocket)
    {
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        ClientSocket = nullptr;
    }

    if (bIsConnected)
    {
        UE_LOG(LogHktCustomNetClient, Log, TEXT("Client disconnected."));
    }
    bIsConnected = false;
}

void FHktReliableUdpClient::Send(const TArray<uint8>& Data)
{
    if (!bIsConnected)
    {
        UE_LOG(LogHktCustomNetClient, Warning, TEXT("Cannot send data. Not connected to server."));
        return;
    }
    SendPacket(Data, EPacketType::Data);
}

void FHktReliableUdpClient::JoinGroup(int32 GroupId)
{
    if (!bIsConnected)
    {
        UE_LOG(LogHktCustomNetClient, Warning, TEXT("Cannot request join group. Not connected to server."));
        return;
    }

    // 페이로드에 GroupId를 직렬화하여 담음
    TArray<uint8> Payload;
    Payload.SetNum(sizeof(int32));
    FMemory::Memcpy(Payload.GetData(), &GroupId, sizeof(int32));

    // JoinGroupRequest 타입으로 패킷 전송
    SendPacket(Payload, EPacketType::JoinGroup);
    UE_LOG(LogHktCustomNetClient, Log, TEXT("=> Sent [JoinGroupRequest] for Group ID: %d"), GroupId);
}

void FHktReliableUdpClient::LeaveGroup()
{
    if (!bIsConnected)
    {
        UE_LOG(LogHktCustomNetClient, Warning, TEXT("Cannot request leave group. Not connected to server."));
        return;
    }

    // 그룹 탈퇴 요청은 별도의 페이로드가 필요 없음
    SendPacket(TArray<uint8>(), EPacketType::LeaveGroup);
    UE_LOG(LogHktCustomNetClient, Log, TEXT("=> Sent [LeaveGroupRequest] to the server."));
}


void FHktReliableUdpClient::SendPacket(const TArray<uint8>& Data, EPacketType Type)
{
    if (!ClientSocket || !ServerAddr.IsValid()) return;

    FPacketHeader Header;
    Header.Type = Type;

    {
        FScopeLock Lock(&StateMutex);
        // 'Data' 타입의 패킷일 경우에만 새로운 시퀀스 번호 부여
        if (Type == EPacketType::Data)
        {
            SentSequence++;
            Header.Sequence = SentSequence;
        }
        // 내가 서버로부터 마지막으로 받은 패킷 정보를 헤더에 담아 보냄 (Piggybacking Ack)
        Header.LastAckedSequence = ReceivedSequence;
        Header.AckBitfield = ReceivedAckBitfield;
    }

    TArray<uint8> PacketData;
    PacketData.Append((uint8*)&Header, sizeof(FPacketHeader));
    PacketData.Append(Data);

    int32 BytesSent = 0;
    ClientSocket->SendTo(PacketData.GetData(), PacketData.Num(), BytesSent, *ServerAddr);

    // 로그 출력: 어떤 정보를 담아 패킷을 보내는지 상세히 기록
    if (Type == EPacketType::Data)
    {
        UE_LOG(LogHktCustomNetClient, Verbose, TEXT("=> Sent [Data]. Seq: %u, Ack: %u, AckBits: %u"), Header.Sequence, Header.LastAckedSequence, Header.AckBitfield);
        FScopeLock Lock(&StateMutex);
        // 재전송을 위해 보낸 패킷 정보 저장
        PendingAckPackets.Add(Header.Sequence, FPendingPacket(MoveTemp(PacketData), FPlatformTime::Seconds()));
    }
}


void FHktReliableUdpClient::Tick()
{
    // 메인 스레드에서 매 프레임 수신된 패킷 처리
    ProcessReceivedPackets();

    // 연결된 상태라면, Ack를 받지 못한 패킷이 있는지 검사하여 재전송
    if (IsConnected())
    {
        CheckForResends();
    }
}

bool FHktReliableUdpClient::Poll(TArray<uint8>& OutData)
{
    // 게임 로직에서 사용할 수 있도록 처리된 데이터 페이로드를 큐에서 꺼냄
    return ReceivedDataPackets.Dequeue(OutData);
}

bool FHktReliableUdpClient::Init()
{
    bIsStopping = false;
    return true;
}

uint32 FHktReliableUdpClient::Run()
{
    // 이 함수는 'UdpClientReceiverThread' 스레드에서 실행됩니다.
    TArray<uint8> ReceiveBuffer;
    ReceiveBuffer.SetNumUninitialized(65535);

    while (!bIsStopping)
    {
        // 100ms 타임아웃으로 소켓에 읽을 데이터가 있는지 확인
        if (ClientSocket && ClientSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(100)))
        {
            int32 BytesRead = 0;
            if (ClientSocket->Recv(ReceiveBuffer.GetData(), ReceiveBuffer.Num(), BytesRead))
            {
                if (BytesRead > 0)
                {
                    // 수신된 데이터를 복사하여 처리 큐(IncomingPackets)에 넣음
                    TArray<uint8> Data;
                    Data.Append(ReceiveBuffer.GetData(), BytesRead);
                    IncomingPackets.Enqueue(MoveTemp(Data));
                    UE_LOG(LogHktCustomNetClient, Verbose, TEXT("Socket received %d bytes from server."), BytesRead);
                }
            }
        }
    }
    return 0;
}

void FHktReliableUdpClient::Exit()
{
    UE_LOG(LogHktCustomNetClient, Log, TEXT("Receiver thread finished."));
}

void FHktReliableUdpClient::ProcessReceivedPackets()
{
    // 이 함수는 메인 스레드의 Tick에서 호출됩니다.
    TArray<uint8> PacketData;
    while (IncomingPackets.Dequeue(PacketData))
    {
        if (PacketData.Num() < sizeof(FPacketHeader))
        {
            UE_LOG(LogHktCustomNetClient, Warning, TEXT("Received a packet smaller than header size. Dropping."));
            continue;
        }

        FPacketHeader Header;
        FMemory::Memcpy(&Header, PacketData.GetData(), sizeof(FPacketHeader));

        UE_LOG(LogHktCustomNetClient, Verbose, TEXT("<= Rcvd Packet Type: %d, Seq: %u, Ack: %u, AckBits: %u"), (int)Header.Type, Header.Sequence, Header.LastAckedSequence, Header.AckBitfield);

        // 연결 수립 과정: 서버가 보낸 Connect에 대한 첫 Ack를 받으면 연결된 것으로 간주
        if (!bIsConnected && Header.Type == EPacketType::Ack && Header.LastAckedSequence == 0)
        {
            bIsConnected = true;
            UE_LOG(LogHktCustomNetClient, Log, TEXT("Handshake complete. Connection to server established."));
        }

        // 서버가 내가 보낸 패킷들을 잘 받았다고 알려주는 Ack 정보 처리
        ProcessAck(Header);

        // 서버가 보낸 '데이터' 패킷 처리
        if (Header.Type == EPacketType::Data)
        {
            // 내가 어떤 패킷까지 받았는지 수신 상태 갱신
            UpdateReceivedState(Header.Sequence);

            // 헤더를 제외한 순수 데이터(Payload)를 게임 로직 큐에 넣음
            TArray<uint8> Payload;
            Payload.Append(PacketData.GetData() + sizeof(FPacketHeader), PacketData.Num() - sizeof(FPacketHeader));
            ReceivedDataPackets.Enqueue(MoveTemp(Payload));

            UE_LOG(LogHktCustomNetClient, Verbose, TEXT("Data packet (Seq: %u) processed and enqueued for game logic."), Header.Sequence);
        }
    }
}

void FHktReliableUdpClient::ProcessAck(const FPacketHeader& Header)
{
    FScopeLock Lock(&StateMutex);

    // 1. LastAckedSequence로 가장 최근 패킷이 도착했음을 확인하고 Pending 큐에서 제거
    if (PendingAckPackets.Contains(Header.LastAckedSequence))
    {
        PendingAckPackets.Remove(Header.LastAckedSequence);
        UE_LOG(LogHktCustomNetClient, Verbose, TEXT("Ack confirmed for sequence %u."), Header.LastAckedSequence);
    }

    // 2. AckBitfield를 순회하며 그 이전 패킷들의 도착 여부도 확인하고 제거
    for (int32 i = 0; i < 32; ++i)
    {
        if ((Header.AckBitfield >> i) & 1)
        {
            uint32 AckedSequence = Header.LastAckedSequence - (i + 1);
            if (PendingAckPackets.Contains(AckedSequence))
            {
                PendingAckPackets.Remove(AckedSequence);
                UE_LOG(LogHktCustomNetClient, Verbose, TEXT("Ack confirmed for sequence %u via bitfield."), AckedSequence);
            }
        }
    }
}

void FHktReliableUdpClient::UpdateReceivedState(uint32 IncomingSequence)
{
    FScopeLock Lock(&StateMutex);

    // 너무 오래된, 이미 처리된 시퀀스 번호는 무시
    if (IncomingSequence <= ReceivedSequence - 32) return;

    // 받은 시퀀스 번호가 내가 기록한 마지막 번호보다 큰 경우 (정상적인 순서)
    if (IncomingSequence > ReceivedSequence)
    {
        uint32 Diff = IncomingSequence - ReceivedSequence;
        // 비트필드를 왼쪽으로 밀어서 지난 패킷 정보를 갱신
        ReceivedAckBitfield = (Diff >= 32) ? 1 : (ReceivedAckBitfield << Diff) | 1;
        // 마지막 수신 번호 갱신
        ReceivedSequence = IncomingSequence;
    }
    else // 순서가 뒤바뀌어 도착한 패킷 (Out-of-order)
    {
        uint32 Diff = ReceivedSequence - IncomingSequence;
        // 비트필드의 해당 위치에 1을 설정하여 수신했음을 표시
        ReceivedAckBitfield |= (1 << (Diff - 1));
    }
    UE_LOG(LogHktCustomNetClient, Verbose, TEXT("Receive state updated. Last Rcvd Seq: %u, Rcvd Bits: %u"), ReceivedSequence, ReceivedAckBitfield);
}

void FHktReliableUdpClient::CheckForResends()
{
    double CurrentTime = FPlatformTime::Seconds();

    FScopeLock Lock(&StateMutex);

    // Ack를 기다리는 모든 패킷을 순회
    for (auto& Elem : PendingAckPackets)
    {
        FPendingPacket& PendingPacket = Elem.Value;
        // 보낸 지 일정 시간이 지났는지 확인
        if (CurrentTime - PendingPacket.SentTime > ResendTimeout)
        {
            // 최대 재시도 횟수를 초과했다면 연결 끊김으로 간주
            if (PendingPacket.Retries >= MaxRetries)
            {
                UE_LOG(LogHktCustomNetClient, Error, TEXT("Server not responding after %d retries. Disconnecting."), MaxRetries);
                // 메인 스레드에서 Disconnect를 직접 호출하면 데드락 위험이 있으므로,
                // 실제 프로젝트에서는 플래그를 설정하고 Tick의 다음 프레임에서 처리하는 것이 더 안전합니다.
                // 여기서는 설명을 위해 즉시 호출합니다.
                Disconnect();
                return;
            }

            // 패킷 재전송
            int32 BytesSent = 0;
            ClientSocket->SendTo(PendingPacket.PacketData.GetData(), PendingPacket.PacketData.Num(), BytesSent, *ServerAddr);

            PendingPacket.SentTime = CurrentTime;
            PendingPacket.Retries++;
            UE_LOG(LogHktCustomNetClient, Warning, TEXT("Packet timeout. Resending (Seq:%u), Retry: %d/%d"), Elem.Key, PendingPacket.Retries, MaxRetries);
        }
    }
}



