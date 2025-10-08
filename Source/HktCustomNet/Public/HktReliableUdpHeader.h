#pragma once

#include "CoreMinimal.h"
#include "SocketTypes.h"

// 통신 관련 로그를 위한 커스텀 로그 카테고리 선언
DECLARE_LOG_CATEGORY_EXTERN(LogHktCustomNet, Log, All);

// 패킷의 종류를 정의하는 열거형
enum class EPacketType : uint8
{
    // 데이터 전송
    Data,
    // 수신 확인 응답
    Ack,
    // 연결 요청
    Connect,
    // 연결 해제
    Disconnect,
    // 연결 유지 (Heartbeat)
    Ping,
    // Ping에 대한 응답
    Pong,
    // 클라이언트가 그룹 참가를 요청
    JoinGroup,
    // 클라이언트가 그룹 탈퇴를 요청
    LeaveGroup 
};

// pragma pack을 사용하여 구조체 패딩을 방지합니다.
// 네트워크로 전송될 데이터는 크기가 정확히 일치해야 합니다.
#pragma pack(push, 1)
struct FPacketHeader
{
    // 패킷의 종류
    EPacketType Type;
    // 패킷의 고유 시퀀스 번호
    uint32 Sequence;
    // 마지막으로 정상 수신한 패킷의 시퀀스 번호
    uint32 LastAckedSequence;
    // Ack 비트필드. LastAckedSequence 이전 32개의 패킷 수신 여부를 나타냄
    uint32 AckBitfield;

    FPacketHeader()
        : Type(EPacketType::Data)
        , Sequence(0)
        , LastAckedSequence(0)
        , AckBitfield(0)
    {
    }
};
#pragma pack(pop)

// 네트워크를 통해 받은 패킷 데이터를 담을 구조체
struct FReceivedPacket
{
    TSharedPtr<FInternetAddr> PeerAddress;
    TArray<uint8> Data;

    FReceivedPacket() = default;
    FReceivedPacket(TSharedPtr<FInternetAddr> InAddr, TArray<uint8>&& InData)
        : PeerAddress(InAddr)
        , Data(MoveTemp(InData))
    {
    }
};

namespace HktReliableUdp
{
    constexpr uint16 ServerPort = 7777;
    constexpr uint16 ClientPort = 7778;
}