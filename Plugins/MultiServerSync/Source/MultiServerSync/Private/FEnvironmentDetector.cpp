// FEnvironmentDetector.cpp
#include "FEnvironmentDetector.h"

FEnvironmentDetector::FEnvironmentDetector()
    : bHasGenlockHardware(false)
    , bHasNDisplay(false)
    , bIsInitialized(false)
{
}

FEnvironmentDetector::~FEnvironmentDetector()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FEnvironmentDetector::Initialize()
{
    // 기본 초기화 작업
    ScanNetworkInterfaces();
    DetectGenlockHardware();
    DetectNDisplay();

    bIsInitialized = true;
    return true;
}

void FEnvironmentDetector::Shutdown()
{
    bIsInitialized = false;
}

bool FEnvironmentDetector::IsFeatureAvailable(const FString& FeatureName)
{
    if (FeatureName == TEXT("GenlockHardware"))
    {
        return bHasGenlockHardware;
    }
    else if (FeatureName == TEXT("nDisplay"))
    {
        return bHasNDisplay;
    }
    else if (FeatureName == TEXT("NetworkInterfaces"))
    {
        return NetworkInterfaces.Num() > 0;
    }

    return false;
}

TMap<FString, FString> FEnvironmentDetector::GetFeatureInfo(const FString& FeatureName)
{
    TMap<FString, FString> Info;

    if (FeatureName == TEXT("GenlockHardware"))
    {
        Info.Add(TEXT("Available"), bHasGenlockHardware ? TEXT("Yes") : TEXT("No"));
    }
    else if (FeatureName == TEXT("nDisplay"))
    {
        Info.Add(TEXT("Available"), bHasNDisplay ? TEXT("Yes") : TEXT("No"));
    }
    else if (FeatureName == TEXT("NetworkInterfaces"))
    {
        Info.Add(TEXT("Count"), FString::FromInt(NetworkInterfaces.Num()));
        for (int32 i = 0; i < NetworkInterfaces.Num(); ++i)
        {
            Info.Add(FString::Printf(TEXT("Interface%d"), i), NetworkInterfaces[i]);
        }
    }

    return Info;
}

bool FEnvironmentDetector::DetectGenlockHardware()
{
    // 실제 구현에서는 하드웨어 젠락을 감지
    // 이 예제에서는 감지되지 않았다고 가정
    bHasGenlockHardware = false;
    return bHasGenlockHardware;
}

bool FEnvironmentDetector::DetectNDisplay()
{
    // 실제 구현에서는 nDisplay 모듈을 감지
    // 이 예제에서는 감지되지 않았다고 가정
    bHasNDisplay = false;
    return bHasNDisplay;
}

bool FEnvironmentDetector::ScanNetworkInterfaces()
{
    // 실제 구현에서는 네트워크 인터페이스를 스캔
    // 이 예제에서는 가상의 인터페이스를 추가
    NetworkInterfaces.Empty();
    NetworkInterfaces.Add(TEXT("Loopback"));
    NetworkInterfaces.Add(TEXT("Ethernet"));

    return NetworkInterfaces.Num() > 0;
}

TArray<FString> FEnvironmentDetector::GetNetworkInterfaces() const
{
    return NetworkInterfaces;
}

bool FEnvironmentDetector::HasGenlockHardware() const
{
    return bHasGenlockHardware;
}

bool FEnvironmentDetector::HasNDisplay() const
{
    return bHasNDisplay;
}