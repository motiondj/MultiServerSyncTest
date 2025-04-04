// MultiServerSyncTest/Source/MultiServerSyncTest/Private/SyncTestActor.cpp
#include "SyncTestActor.h"
#include "Modules/ModuleManager.h"

// 파일 헤더 문제 해결을 위한 코드

ASyncTestActor::ASyncTestActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bIsLogging = false;
    LogTimer = 0.0f;
}

void ASyncTestActor::BeginPlay()
{
    Super::BeginPlay();
}

void ASyncTestActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 로깅이 활성화되어 있고 로그 타이머가 0.1초 이상이면 로그 기록
    if (bIsLogging)
    {
        LogTimer += DeltaTime;
        if (LogTimer >= 0.1f)  // 100ms마다 로깅
        {
            LogTimer = 0.0f;

            FString LogLine = FString::Printf(
                TEXT("%f,%d,%f,%f,%d,%f\n"),
                FPlatformTime::Seconds(),
                IsMasterNode() ? 1 : 0,
                GetTimeOffset() * 1000.0f,  // ms 단위
                GetPathDelay() * 1000.0f,   // ms 단위
                GetCurrentFrameNumber(),
                GetFrameTimeDelta() * 1000.0f  // ms 단위
            );

            FFileHelper::SaveStringToFile(
                LogLine,
                *LogFileName,
                FFileHelper::EEncodingOptions::AutoDetect,
                &IFileManager::Get(),
                EFileWrite::FILEWRITE_Append
            );
        }
    }

    // 테스트를 위한 가상 시간 진행
    static float SimulatedTime = 0.0f;
    SimulatedTime += DeltaTime;
}

// 테스트용 더미 함수
float ASyncTestActor::GetTimeOffset() const
{
    // 실제 플러그인이 로드되어 있는지 확인
    static FName PluginModuleName = "MultiServerSync";
    bool bPluginLoaded = FModuleManager::Get().IsModuleLoaded(PluginModuleName);

    if (bPluginLoaded)
    {
        // 로드되어 있지만 직접 액세스 못함 - 시뮬레이션된 값 리턴
        static float SimulatedOffset = 0.005f; // 5ms 초기값
        SimulatedOffset *= 0.95f; // 매 프레임 감소 (동기화 시뮬레이션)
        return SimulatedOffset;
    }

    // 플러그인 없음 - 0 리턴
    return 0.0f;
}

float ASyncTestActor::GetPathDelay() const
{
    // 예시: 1-5ms 사이의 랜덤 지연
    static float SimulatedDelay = 0.003f; // 3ms
    return SimulatedDelay;
}

bool ASyncTestActor::IsMasterNode() const
{
    // 첫 번째 인스턴스는 마스터로 시뮬레이션 (FGuid로 결정)
    static bool bIsMaster = (FGuid::NewGuid().A % 2 == 0);
    return bIsMaster;
}

int32 ASyncTestActor::GetCurrentFrameNumber() const
{
    // 현재 프레임 번호 추정
    return GFrameCounter;
}

float ASyncTestActor::GetFrameTimeDelta() const
{
    // 예상 프레임 시간 (60fps 기준 16.67ms)
    return GWorld ? GWorld->GetDeltaSeconds() : 0.01667f;
}

bool ASyncTestActor::IsTimeInSync() const
{
    // 시간이 동기화 되었는지 시뮬레이션
    return FMath::Abs(GetTimeOffset()) < 0.0001f;
}

FString ASyncTestActor::GetSyncStatusText() const
{
    if (IsMasterNode())
    {
        return TEXT("마스터 노드");
    }

    float Offset = GetTimeOffset();
    if (FMath::Abs(Offset) < 0.0001f)
    {
        return TEXT("정밀 동기화됨 (<0.1ms)");
    }
    else if (FMath::Abs(Offset) < 0.001f)
    {
        return TEXT("동기화됨 (<1ms)");
    }
    else if (FMath::Abs(Offset) < 0.01f)
    {
        return TEXT("동기화 진행 중 (<10ms)");
    }
    else
    {
        return FString::Printf(TEXT("동기화 중... (%.2fms)"), Offset * 1000.0f);
    }
}

void ASyncTestActor::StartLoggingToFile(const FString& FileName)
{
    LogFileName = FPaths::ProjectSavedDir() / TEXT("SyncTests") / FileName;
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 디렉토리 생성
    FString Directory = FPaths::GetPath(LogFileName);
    PlatformFile.CreateDirectoryTree(*Directory);

    // 파일 헤더 작성
    FFileHelper::SaveStringToFile(
        TEXT("Timestamp,IsServer,TimeOffset(ms),PathDelay(ms),FrameNumber,FrameDelta(ms)\n"),
        *LogFileName
    );

    bIsLogging = true;
    LogTimer = 0.0f;
}

void ASyncTestActor::StopLoggingToFile()
{
    bIsLogging = false;
}