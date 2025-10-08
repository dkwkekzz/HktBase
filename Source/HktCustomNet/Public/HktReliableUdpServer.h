#pragma once

#include "HktReliableUdpHeader.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "Common/UdpSocketReceiver.h"
#include "Containers/Set.h"

class FSocket;
class FRunnableThread;

// Ack를 기다리는, 전송된 패킷의 정보를 담는 구조체
struct FPendingPacket
{
    // 헤더를 포함한 전체 패킷 데이터
    TArray<uint8> PacketData;
    // 마지막으로 전송된 시간
    double SentTime;
    // 재전송 횟수
    int32 Retries;

    FPendingPacket() : SentTime(0.0), Retries(0) {}
    FPendingPacket(TArray<uint8>&& InData, double InTime)
        : PacketData(MoveTemp(InData))
        , SentTime(InTime)
        , Retries(0)
    {}
};

// 클라이언트 연결 정보를 관리하는 구조체
struct FClientConnection
{
    // 클라이언트의 주소 정보
    TSharedPtr<FInternetAddr> Address;
    // 이 클라이언트에게 보낸 마지막 시퀀스 번호
    uint32 SentSequence = 0;
    // 이 클라이언트로부터 받은 마지막 시퀀스 번호
    uint32 ReceivedSequence = 0;
    // 이 클라이언트로부터 받은 패킷들의 Ack 비트필드
    uint32 ReceivedAckBitfield = 0;
    // 마지막으로 통신한 시간
    double LastReceiveTime = 0.0;
    // 소속된 그룹 ID 목록
    TSet<int32> GroupIds;

    // Ack를 기다리는 전송된 패킷들 (시퀀스 번호 -> 패킷 정보)
    TMap<uint32, FPendingPacket> PendingAckPackets;
};

class HKTCUSTOMNET_API FHktReliableUdpServer : public FRunnable
{
public:
    FHktReliableUdpServer(uint16 InPort);
    virtual ~FHktReliableUdpServer();

    // 서버 시작
    void Start();
    // 서버 중지
    void Stop();

    // 매 프레임 호출될 함수. 수신된 패킷 처리 및 재전송 검사
    void Tick();

    // 특정 클라이언트에게 데이터 전송
    void SendTo(const TSharedPtr<FInternetAddr>& DstAddr, const TArray<uint8>& Data);
    // 특정 그룹의 모든 클라이언트에게 데이터 전송 (Broadcast)
    void BroadcastToGroup(int32 GroupId, const TArray<uint8>& Data, const TSharedPtr<FInternetAddr>& ExcludeAddr = nullptr);
    
    // 클라이언트를 그룹에 추가
    void JoinGroup(const TSharedPtr<FInternetAddr>& ClientAddr, int32 GroupId);
    // 클라이언트를 그룹에서 제거
    void LeaveGroup(const TSharedPtr<FInternetAddr>& ClientAddr, int32 GroupId);

protected:
    // FRunnable 인터페이스 구현
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Exit() override;

private:
    // 수신된 패킷 처리
    void ProcessReceivedPackets();
    // Ack 및 AckBitfield 처리
    void ProcessAck(const FPacketHeader& Header, TSharedPtr<FClientConnection> Connection);
    // 수신 상태 업데이트 (ReceivedSequence, ReceivedAckBitfield)
    void UpdateReceivedState(uint32 IncomingSequence, TSharedPtr<FClientConnection> Connection);
    // 재전송이 필요한 패킷 검사 및 처리
    void CheckForResends();
    // 일정 시간 응답 없는 클라이언트 타임아웃 처리
    void CheckForTimeouts();

    // 새로운 클라이언트 연결 처리
    void HandleNewConnection(const TSharedPtr<FInternetAddr>& NewAddr);
    // 클라이언트 연결 해제 처리
    void DisconnectClient(const FString& ClientAddrStr, const FString& Reason);
    // ACK 패킷 전송
    void SendAck(TSharedPtr<FClientConnection> Connection);

    // 서버 리슨 소켓
    FSocket* ListenSocket = nullptr;
    // 서버 포트
    uint16 Port;
    
    // 수신 스레드
    FRunnableThread* ReceiverThread = nullptr;
    // 스레드 중지 플래그
    FThreadSafeBool bIsStopping;

    // 수신된 패킷들을 담는 스레드 안전 큐
    TQueue<FReceivedPacket, EQueueMode::Mpsc> ReceivedPackets;

    // 연결된 클라이언트 정보 (주소 -> 정보)
    TMap<FString, TSharedPtr<FClientConnection>> Connections;
    
    // 그룹 정보 (그룹 ID -> 클라이언트 주소 맵)
    TMap<int32, TMap<FString, TSharedPtr<FInternetAddr>>> Groups;
    
    // Connections, Groups 접근을 위한 크리티컬 섹션
    FCriticalSection ConnectionMutex;

    // 재전송 관련 상수
    const float ResendTimeout = 0.2f; // 200ms
    const int32 MaxRetries = 10;
	const float ClientTimeoutDuration = 5.0f; // 5 seconds
};

