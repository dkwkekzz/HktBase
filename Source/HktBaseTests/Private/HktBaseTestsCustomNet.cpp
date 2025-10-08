#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "HktReliableUdpServer.h"
#include "HktReliableUdpClient.h"
#include "Misc/AutomationTest.h"

// 간단한 서버-클라 연결 테스트
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHktCustomNetConnectionTest, "HktCustomNet.Connection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FHktCustomNetConnectionTest::RunTest(const FString& Parameters)
{
    const uint16 Port = 12345;
    const FString ServerIp = TEXT("127.0.0.1");

    // 1. 서버 생성 및 시작
    TUniquePtr<FHktReliableUdpServer> Server = MakeUnique<FHktReliableUdpServer>(Port);
    Server->Start();

    // 2. 클라이언트 생성 및 연결
    TUniquePtr<FHktReliableUdpClient> Client = MakeUnique<FHktReliableUdpClient>();
    TestTrue("Client Connect call should succeed", Client->Connect(ServerIp, Port));

    // 3. 연결이 설정될 때까지 잠시 대기
    const float ConnectionTimeout = 5.0f;
    float ElapsedTime = 0.0f;
    const float TickRate = 0.01f;

    while (ElapsedTime < ConnectionTimeout && !Client->IsConnected())
    {
        Server->Tick();
        Client->Tick();
        FPlatformProcess::Sleep(TickRate);
        ElapsedTime += TickRate;
    }

    // 4. 연결 확인
    TestTrue("Client should be connected", Client->IsConnected());

    // 5. 정리
    Client->Disconnect();
    Server->Stop();

    // Give sockets time to close
    FPlatformProcess::Sleep(0.1f);

    return true;
}

// 패킷 송수신 테스트
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHktCustomNetDataTransmissionTest, "HktCustomNet.Transmission", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FHktCustomNetDataTransmissionTest::RunTest(const FString& Parameters)
{
    const uint16 Port = 12346; // Use a different port to avoid conflicts
    const FString ServerIp = TEXT("127.0.0.1");

    // 1. 서버 생성 및 시작
    TUniquePtr<FHktReliableUdpServer> Server = MakeUnique<FHktReliableUdpServer>(Port);
    Server->Start();

    // 2. 클라이언트 A, B 생성 및 연결
    TUniquePtr<FHktReliableUdpClient> ClientA = MakeUnique<FHktReliableUdpClient>();
    TestTrue("ClientA Connect call", ClientA->Connect(ServerIp, Port));

    TUniquePtr<FHktReliableUdpClient> ClientB = MakeUnique<FHktReliableUdpClient>();
    TestTrue("ClientB Connect call", ClientB->Connect(ServerIp, Port, HktReliableUdp::ClientPort + 1));

    // 3. 연결 대기
    const float ConnectionTimeout = 5.0f;
    float ElapsedTime = 0.0f;
    const float TickRate = 0.01f;

    while (ElapsedTime < ConnectionTimeout && (!ClientA->IsConnected() || !ClientB->IsConnected()))
    {
        Server->Tick();
        ClientA->Tick();
        ClientB->Tick();
        FPlatformProcess::Sleep(TickRate);
        ElapsedTime += TickRate;
    }
    TestTrue("ClientA should be connected", ClientA->IsConnected());
    TestTrue("ClientB should be connected", ClientB->IsConnected());

    if (!ClientA->IsConnected() || !ClientB->IsConnected())
    {
        ClientA->Disconnect();
        ClientB->Disconnect();
        Server->Stop();
        FPlatformProcess::Sleep(0.1f);
        return false;
    }

    // 4. 그룹 참여
    const int32 GroupId = 1;
    ClientA->JoinGroup(GroupId);
    ClientB->JoinGroup(GroupId);
    
    // 그룹 참여 메시지 처리를 위한 시간
    ElapsedTime = 0.0f;
    const float GroupJoinTime = 1.0f;
    while(ElapsedTime < GroupJoinTime)
    {
        Server->Tick();
        ClientA->Tick();
        ClientB->Tick();
        FPlatformProcess::Sleep(TickRate);
        ElapsedTime += TickRate;
    }

    // 5. 클라이언트 A -> 서버 데이터 전송 테스트 (ACK 수신을 통한 간접 확인)
    TArray<uint8> ClientAData;
    FString ClientAString = TEXT("Data from A");
    FTCHARToUTF8 ConverterA(*ClientAString);
    ClientAData.Append((uint8*)ConverterA.Get(), ConverterA.Length());
    ClientA->Send(ClientAData);
    
    // 6. 서버 -> 클라이언트 그룹 브로드캐스트 테스트
    TArray<uint8> BroadcastData;
    FString BroadcastString = TEXT("Broadcast from Server");
    FTCHARToUTF8 ConverterB(*BroadcastString);
    BroadcastData.Append((uint8*)ConverterB.Get(), ConverterB.Length());
    Server->BroadcastToGroup(GroupId, BroadcastData);

    // 7. 클라이언트에서 데이터 수신 확인
    ElapsedTime = 0.0f;
    const float ReceiveTimeout = 5.0f;
    TArray<uint8> ReceivedDataA, ReceivedDataB;
    bool bReceivedA = false;
    bool bReceivedB = false;

    while (ElapsedTime < ReceiveTimeout && (!bReceivedA || !bReceivedB))
    {
        Server->Tick();
        ClientA->Tick();
        ClientB->Tick();

        if (!bReceivedA)
        {
            bReceivedA = ClientA->Poll(ReceivedDataA);
        }
        if (!bReceivedB)
        {
            bReceivedB = ClientB->Poll(ReceivedDataB);
        }

        FPlatformProcess::Sleep(TickRate);
        ElapsedTime += TickRate;
    }

    TestTrue("ClientA received broadcast data", bReceivedA);
    if(bReceivedA)
    {
        TestEqual("ClientA received correct data", ReceivedDataA, BroadcastData);
    }
    
    TestTrue("ClientB received broadcast data", bReceivedB);
    if(bReceivedB)
    {
        TestEqual("ClientB received correct data", ReceivedDataB, BroadcastData);
    }

    // 클라이언트 A가 보낸 데이터에 대한 ACK를 받고, 연결이 유지되었는지 확인
    TestTrue("ClientA should still be connected after sending data", ClientA->IsConnected());

    // 8. 정리
    ClientA->Disconnect();
    ClientB->Disconnect();
    Server->Stop();

    // Give sockets time to close
    FPlatformProcess::Sleep(0.1f);

    return true;
}
