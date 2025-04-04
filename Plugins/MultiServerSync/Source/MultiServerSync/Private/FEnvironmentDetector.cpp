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
    // 젠락 하드웨어 감지 구현
    MSYNC_LOG_INFO(TEXT("Detecting Genlock hardware..."));

    // NVIDIA API를 사용하여 Quadro Sync 감지
    // 이 예제에서는 Windows 플랫폼의 경우에만 구현
#if PLATFORM_WINDOWS
    bHasGenlockHardware = DetectNVIDIAQuadroSync();
#else
    bHasGenlockHardware = false;
#endif

    MSYNC_LOG_INFO(TEXT("Genlock hardware detection result: %s"),
        bHasGenlockHardware ? TEXT("Found") : TEXT("Not found"));

    return bHasGenlockHardware;
}

#if PLATFORM_WINDOWS
bool FEnvironmentDetector::DetectNVIDIAQuadroSync()
{
    // 실제 구현에서는 NVIDIA API를 사용하여 Quadro Sync 장치를 감지
    // 현재는 시스템 환경 변수를 확인하는 방식으로 간략히 구현

    // 환경 변수 확인 (테스트 목적으로 사용 가능)
    FString EnvValue;
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("QUADRO_SYNC_PRESENT")) == TEXT("1"))
    {
        MSYNC_LOG_INFO(TEXT("Quadro Sync detected via environment variable"));
        return true;
    }

    // GPU 정보 확인
    // 여기서는 문자열 검색으로 단순 확인
    // 실제 구현에서는 NVAPI 또는 WMI 등을 사용해야 함
    const FString GPUDesc = FPlatformMisc::GetPrimaryGPUBrand();
    if (GPUDesc.Contains(TEXT("Quadro")) && (GPUDesc.Contains(TEXT("Sync")) || GPUDesc.Contains(TEXT("SDI"))))
    {
        MSYNC_LOG_INFO(TEXT("Potential Quadro Sync capable device detected: %s"), *GPUDesc);
        return true;
    }

    return false;
}
#endif

bool FEnvironmentDetector::DetectNDisplay()
{
    MSYNC_LOG_INFO(TEXT("Detecting nDisplay module..."));

    // nDisplay 모듈이 로드되었는지 확인
    bHasNDisplay = FModuleManager::Get().IsModuleLoaded("DisplayCluster") ||
        FModuleManager::Get().IsModuleLoaded("nDisplay");

    // nDisplay 모듈이 로드되지 않았으면 로드를 시도
    if (!bHasNDisplay)
    {
        // 동적으로 모듈 로드 시도
        TSharedPtr<IModuleInterface> Module = FModuleManager::Get().LoadModule("DisplayCluster");
        bHasNDisplay = Module.IsValid();

        if (!bHasNDisplay)
        {
            Module = FModuleManager::Get().LoadModule("nDisplay");
            bHasNDisplay = Module.IsValid();
        }
    }

    MSYNC_LOG_INFO(TEXT("nDisplay module detection result: %s"),
        bHasNDisplay ? TEXT("Available") : TEXT("Not available"));

    return bHasNDisplay;
}

bool FEnvironmentDetector::ScanNetworkInterfaces()
{
    MSYNC_LOG_INFO(TEXT("Scanning network interfaces..."));

    // 네트워크 인터페이스 목록 초기화
    NetworkInterfaces.Empty();
    NetworkInterfaceInfo.Empty();

    // 소켓 서브시스템 가져오기
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        MSYNC_LOG_ERROR(TEXT("Failed to get socket subsystem"));
        return false;
    }

    // 로컬 호스트 이름 가져오기
    FString HostName;
    SocketSubsystem->GetHostName(HostName);
    MSYNC_LOG_INFO(TEXT("Local hostname: %s"), *HostName);

    // 모든 로컬 IP 주소 가져오기
    TSharedPtr<FInternetAddr> LocalAddr = SocketSubsystem->GetLocalHostAddr(*GLog, false);
    if (LocalAddr.IsValid())
    {
        // 기본 로컬 호스트 주소 저장
        FString LocalIP = LocalAddr->ToString(false);
        MSYNC_LOG_INFO(TEXT("Local IP address: %s"), *LocalIP);

        NetworkInterfaces.Add(TEXT("Default"));

        FNetworkInterfaceInfo Info;
        Info.Name = TEXT("Default");
        Info.IPAddress = LocalIP;
        Info.SubnetMask = TEXT("255.255.255.0"); // 기본값
        Info.IsUp = true;
        Info.SupportsMulticast = true;

        NetworkInterfaceInfo.Add(Info.Name, Info);
    }

    // 모든 네트워크 어댑터 주소 가져오기
    TArray<TSharedPtr<FInternetAddr>> AddressList;
    if (SocketSubsystem->GetLocalAdapterAddresses(AddressList))
    {
        for (int32 i = 0; i < AddressList.Num(); ++i)
        {
            if (AddressList[i].IsValid())
            {
                FString AdapterIP = AddressList[i]->ToString(false);

                // 로컬 루프백 주소 필터링 (127.0.0.1)
                if (AdapterIP.StartsWith(TEXT("127.")))
                {
                    MSYNC_LOG_INFO(TEXT("Detected loopback interface: %s"), *AdapterIP);

                    NetworkInterfaces.Add(TEXT("Loopback"));

                    FNetworkInterfaceInfo Info;
                    Info.Name = TEXT("Loopback");
                    Info.IPAddress = AdapterIP;
                    Info.SubnetMask = TEXT("255.0.0.0");
                    Info.IsUp = true;
                    Info.SupportsMulticast = false;

                    NetworkInterfaceInfo.Add(Info.Name, Info);
                }
                // 실제 네트워크 인터페이스
                else
                {
                    FString InterfaceName = FString::Printf(TEXT("Adapter%d"), i);
                    MSYNC_LOG_INFO(TEXT("Detected network interface %s: %s"), *InterfaceName, *AdapterIP);

                    NetworkInterfaces.Add(InterfaceName);

                    FNetworkInterfaceInfo Info;
                    Info.Name = InterfaceName;
                    Info.IPAddress = AdapterIP;
                    Info.SubnetMask = TEXT("255.255.255.0"); // 기본값
                    Info.IsUp = true;
                    Info.SupportsMulticast = true;

                    NetworkInterfaceInfo.Add(Info.Name, Info);
                }
            }
        }
    }

    MSYNC_LOG_INFO(TEXT("Detected %d network interfaces"), NetworkInterfaces.Num());

    return NetworkInterfaces.Num() > 0;
}

bool FEnvironmentDetector::GetNetworkInterfaceInfo(const FString& InterfaceName, FNetworkInterfaceInfo& OutInfo) const
{
    const FNetworkInterfaceInfo* Info = NetworkInterfaceInfo.Find(InterfaceName);
    if (Info)
    {
        OutInfo = *Info;
        return true;
    }
    return false;
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