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
    // 1. ���� �ּ� ��ü ���� �� ����
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

    // 2. UDP ���� ����
    ClientSocket = FUdpSocketBuilder(TEXT("UdpClientSocket"))
        .AsNonBlocking()
        .BoundToPort(ClientPort);

    if (ClientSocket)
    {
        // 3. ���� ������ ����
        ReceiverThread = FRunnableThread::Create(this, TEXT("UdpClientReceiverThread"));

        // 4. ������ ���� ��û ��Ŷ ���� (Handshake ����)
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
        // ������ ���� ������ �˸�
        SendPacket(TArray<uint8>(), EPacketType::Disconnect);
        UE_LOG(LogHktCustomNetClient, Log, TEXT("Sent [Disconnect] message to server."));
    }

    // ������� ���� ����
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

    // ���̷ε忡 GroupId�� ����ȭ�Ͽ� ����
    TArray<uint8> Payload;
    Payload.SetNum(sizeof(int32));
    FMemory::Memcpy(Payload.GetData(), &GroupId, sizeof(int32));

    // JoinGroupRequest Ÿ������ ��Ŷ ����
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

    // �׷� Ż�� ��û�� ������ ���̷ε尡 �ʿ� ����
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
        // 'Data' Ÿ���� ��Ŷ�� ��쿡�� ���ο� ������ ��ȣ �ο�
        if (Type == EPacketType::Data)
        {
            SentSequence++;
            Header.Sequence = SentSequence;
        }
        // ���� �����κ��� ���������� ���� ��Ŷ ������ ����� ��� ���� (Piggybacking Ack)
        Header.LastAckedSequence = ReceivedSequence;
        Header.AckBitfield = ReceivedAckBitfield;
    }

    TArray<uint8> PacketData;
    PacketData.Append((uint8*)&Header, sizeof(FPacketHeader));
    PacketData.Append(Data);

    int32 BytesSent = 0;
    ClientSocket->SendTo(PacketData.GetData(), PacketData.Num(), BytesSent, *ServerAddr);

    // �α� ���: � ������ ��� ��Ŷ�� �������� ���� ���
    if (Type == EPacketType::Data)
    {
        UE_LOG(LogHktCustomNetClient, Verbose, TEXT("=> Sent [Data]. Seq: %u, Ack: %u, AckBits: %u"), Header.Sequence, Header.LastAckedSequence, Header.AckBitfield);
        FScopeLock Lock(&StateMutex);
        // �������� ���� ���� ��Ŷ ���� ����
        PendingAckPackets.Add(Header.Sequence, FPendingPacket(MoveTemp(PacketData), FPlatformTime::Seconds()));
    }
}


void FHktReliableUdpClient::Tick()
{
    // ���� �����忡�� �� ������ ���ŵ� ��Ŷ ó��
    ProcessReceivedPackets();

    // ����� ���¶��, Ack�� ���� ���� ��Ŷ�� �ִ��� �˻��Ͽ� ������
    if (IsConnected())
    {
        CheckForResends();
    }
}

bool FHktReliableUdpClient::Poll(TArray<uint8>& OutData)
{
    // ���� �������� ����� �� �ֵ��� ó���� ������ ���̷ε带 ť���� ����
    return ReceivedDataPackets.Dequeue(OutData);
}

bool FHktReliableUdpClient::Init()
{
    bIsStopping = false;
    return true;
}

uint32 FHktReliableUdpClient::Run()
{
    // �� �Լ��� 'UdpClientReceiverThread' �����忡�� ����˴ϴ�.
    TArray<uint8> ReceiveBuffer;
    ReceiveBuffer.SetNumUninitialized(65535);

    while (!bIsStopping)
    {
        // 100ms Ÿ�Ӿƿ����� ���Ͽ� ���� �����Ͱ� �ִ��� Ȯ��
        if (ClientSocket && ClientSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(100)))
        {
            int32 BytesRead = 0;
            if (ClientSocket->Recv(ReceiveBuffer.GetData(), ReceiveBuffer.Num(), BytesRead))
            {
                if (BytesRead > 0)
                {
                    // ���ŵ� �����͸� �����Ͽ� ó�� ť(IncomingPackets)�� ����
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
    // �� �Լ��� ���� �������� Tick���� ȣ��˴ϴ�.
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

        // ���� ���� ����: ������ ���� Connect�� ���� ù Ack�� ������ ����� ������ ����
        if (!bIsConnected && Header.Type == EPacketType::Ack && Header.LastAckedSequence == 0)
        {
            bIsConnected = true;
            UE_LOG(LogHktCustomNetClient, Log, TEXT("Handshake complete. Connection to server established."));
        }

        // ������ ���� ���� ��Ŷ���� �� �޾Ҵٰ� �˷��ִ� Ack ���� ó��
        ProcessAck(Header);

        // ������ ���� '������' ��Ŷ ó��
        if (Header.Type == EPacketType::Data)
        {
            // ���� � ��Ŷ���� �޾Ҵ��� ���� ���� ����
            UpdateReceivedState(Header.Sequence);

            // ����� ������ ���� ������(Payload)�� ���� ���� ť�� ����
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

    // 1. LastAckedSequence�� ���� �ֱ� ��Ŷ�� ���������� Ȯ���ϰ� Pending ť���� ����
    if (PendingAckPackets.Contains(Header.LastAckedSequence))
    {
        PendingAckPackets.Remove(Header.LastAckedSequence);
        UE_LOG(LogHktCustomNetClient, Verbose, TEXT("Ack confirmed for sequence %u."), Header.LastAckedSequence);
    }

    // 2. AckBitfield�� ��ȸ�ϸ� �� ���� ��Ŷ���� ���� ���ε� Ȯ���ϰ� ����
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

    // �ʹ� ������, �̹� ó���� ������ ��ȣ�� ����
    if (IncomingSequence <= ReceivedSequence - 32) return;

    // ���� ������ ��ȣ�� ���� ����� ������ ��ȣ���� ū ��� (�������� ����)
    if (IncomingSequence > ReceivedSequence)
    {
        uint32 Diff = IncomingSequence - ReceivedSequence;
        // ��Ʈ�ʵ带 �������� �о ���� ��Ŷ ������ ����
        ReceivedAckBitfield = (Diff >= 32) ? 1 : (ReceivedAckBitfield << Diff) | 1;
        // ������ ���� ��ȣ ����
        ReceivedSequence = IncomingSequence;
    }
    else // ������ �ڹٲ�� ������ ��Ŷ (Out-of-order)
    {
        uint32 Diff = ReceivedSequence - IncomingSequence;
        // ��Ʈ�ʵ��� �ش� ��ġ�� 1�� �����Ͽ� ���������� ǥ��
        ReceivedAckBitfield |= (1 << (Diff - 1));
    }
    UE_LOG(LogHktCustomNetClient, Verbose, TEXT("Receive state updated. Last Rcvd Seq: %u, Rcvd Bits: %u"), ReceivedSequence, ReceivedAckBitfield);
}

void FHktReliableUdpClient::CheckForResends()
{
    double CurrentTime = FPlatformTime::Seconds();

    FScopeLock Lock(&StateMutex);

    // Ack�� ��ٸ��� ��� ��Ŷ�� ��ȸ
    for (auto& Elem : PendingAckPackets)
    {
        FPendingPacket& PendingPacket = Elem.Value;
        // ���� �� ���� �ð��� �������� Ȯ��
        if (CurrentTime - PendingPacket.SentTime > ResendTimeout)
        {
            // �ִ� ��õ� Ƚ���� �ʰ��ߴٸ� ���� �������� ����
            if (PendingPacket.Retries >= MaxRetries)
            {
                UE_LOG(LogHktCustomNetClient, Error, TEXT("Server not responding after %d retries. Disconnecting."), MaxRetries);
                // ���� �����忡�� Disconnect�� ���� ȣ���ϸ� ����� ������ �����Ƿ�,
                // ���� ������Ʈ������ �÷��׸� �����ϰ� Tick�� ���� �����ӿ��� ó���ϴ� ���� �� �����մϴ�.
                // ���⼭�� ������ ���� ��� ȣ���մϴ�.
                Disconnect();
                return;
            }

            // ��Ŷ ������
            int32 BytesSent = 0;
            ClientSocket->SendTo(PendingPacket.PacketData.GetData(), PendingPacket.PacketData.Num(), BytesSent, *ServerAddr);

            PendingPacket.SentTime = CurrentTime;
            PendingPacket.Retries++;
            UE_LOG(LogHktCustomNetClient, Warning, TEXT("Packet timeout. Resending (Seq:%u), Retry: %d/%d"), Elem.Key, PendingPacket.Retries, MaxRetries);
        }
    }
}



