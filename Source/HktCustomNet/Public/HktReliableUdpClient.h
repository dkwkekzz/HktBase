#pragma once

#include "HktReliableUdpHeader.h"
#include "HAL/Runnable.h"
#include "HktReliableUdpServer.h" // For FPendingPacket

class FSocket;
class FRunnableThread;
class FInternetAddr;

class HKTCUSTOMNET_API FHktReliableUdpClient : public FRunnable
{
public:
    FHktReliableUdpClient();
    virtual ~FHktReliableUdpClient();
    
    // 서버에 연결 시도
    bool Connect(const FString& ServerIp, uint16 ServerPort, uint16 ClientPort = HktReliableUdp::ClientPort);
    // 연결 해제
    void Disconnect();
    
    // 서버로 데이터 전송
    void Send(const TArray<uint8>& Data);
    
    // 매 프레임 호출될 함수
    void Tick();

    // 서버로부터 받은 패킷이 있는지 확인하고 가져옴
    bool Poll(TArray<uint8>& OutData);

    // 서버에 특정 그룹 참여를 요청
    void JoinGroup(int32 GroupId);

    // 서버에 현재 그룹 탈퇴를 요청
    void LeaveGroup();

    bool IsConnected() const { return bIsConnected; }

protected:
    // FRunnable 인터페이스 구현
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Exit() override;

private:
    void ProcessReceivedPackets();
    void CheckForResends();
    void ProcessAck(const FPacketHeader& Header);
    void UpdateReceivedState(uint32 IncomingSequence);
    void SendPacket(const TArray<uint8>& Data, EPacketType Type);

    FSocket* ClientSocket = nullptr;
    TSharedPtr<FInternetAddr> ServerAddr;

    FRunnableThread* ReceiverThread = nullptr;
    FThreadSafeBool bIsStopping;
    FThreadSafeBool bIsConnected;
    
    // 수신된 '데이터' 패킷만 담는 큐
    TQueue<TArray<uint8>, EQueueMode::Mpsc> ReceivedDataPackets;
    // 수신된 모든 패킷을 담는 큐 (처리를 위해)
    TQueue<TArray<uint8>, EQueueMode::Mpsc> IncomingPackets;

    // 신뢰성 보장을 위한 상태 변수
    uint32 SentSequence = 0;
    uint32 ReceivedSequence = 0;
    uint32 ReceivedAckBitfield = 0;
    TMap<uint32, FPendingPacket> PendingAckPackets;
    FCriticalSection StateMutex;

    // 재전송 관련 상수
    const float ResendTimeout = 0.2f; // 200ms
    const int32 MaxRetries = 10;
};

