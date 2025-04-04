// FEnvironmentDetector.cpp
#include "FEnvironmentDetector.h"
#include "FSyncLog.h" // 로깅 시스템 헤더 추가
#include "SocketSubsystem.h" // 소켓 서브시스템 헤더 추가
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformMisc.h"

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

    // 시스템 정보 로깅
    LogSystemInfo();

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

        // 젠락 하드웨어에 대한 추가 정보가 있다면 추가
        if (bHasGenlockHardware)
        {
            TMap<FString, FString> Details = GetGenlockHardwareDetails();
            Info.Append(Details);
        }
    }
    else if (FeatureName == TEXT("nDisplay"))
    {
        Info.Add(TEXT("Available"), bHasNDisplay ? TEXT("Yes") : TEXT("No"));

        // nDisplay에 대한 추가 정보가 있다면 추가
        if (bHasNDisplay)
        {
            TMap<FString, FString> Details = GetNDisplayDetails();
            Info.Append(Details);
        }
    }
    else if (FeatureName == TEXT("NetworkInterfaces"))
    {
        Info.Add(TEXT("Count"), FString::FromInt(NetworkInterfaces.Num()));
        for (int32 i = 0; i < NetworkInterfaces.Num(); ++i)
        {
            FNetworkInterfaceInfo InterfaceInfo;
            if (GetNetworkInterfaceInfo(NetworkInterfaces[i], InterfaceInfo))
            {
                Info.Add(FString::Printf(TEXT("Interface%d"), i), NetworkInterfaces[i]);
                Info.Add(FString::Printf(TEXT("Interface%d_IP"), i), InterfaceInfo.IPAddress);
                Info.Add(FString::Printf(TEXT("Interface%d_Multicast"), i), InterfaceInfo.SupportsMulticast ? TEXT("Yes") : TEXT("No"));
            }
        }
    }

    return Info;
}

bool FEnvironmentDetector::DetectGenlockHardware()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Detecting Genlock hardware..."));

    // NVIDIA API를 사용하여 Quadro Sync 감지
    // 이 예제에서는 Windows 플랫폼의 경우에만 구현
#if PLATFORM_WINDOWS
    bHasGenlockHardware = DetectNVIDIAQuadroSync();
#else
    bHasGenlockHardware = false;
#endif

    UE_LOG(LogMultiServerSync, Display, TEXT("Genlock hardware detection result: %s"),
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
        UE_LOG(LogMultiServerSync, Display, TEXT("Quadro Sync detected via environment variable"));
        return true;
    }

    // GPU 정보 확인
    // 여기서는 문자열 검색으로 단순 확인
    // 실제 구현에서는 NVAPI 또는 WMI 등을 사용해야 함
    const FString GPUDesc = FPlatformMisc::GetPrimaryGPUBrand();
    if (GPUDesc.Contains(TEXT("Quadro")) && (GPUDesc.Contains(TEXT("Sync")) || GPUDesc.Contains(TEXT("SDI"))))
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Potential Quadro Sync capable device detected: %s"), *GPUDesc);
        return true;
    }

    return false;
}
#endif

bool FEnvironmentDetector::DetectNDisplay()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Detecting nDisplay module..."));

    // nDisplay 모듈이 로드되었는지 확인
    bHasNDisplay = FModuleManager::Get().IsModuleLoaded("DisplayCluster") ||
        FModuleManager::Get().IsModuleLoaded("nDisplay");

    // nDisplay 모듈이 로드되지 않았으면 로드를 시도
    if (!bHasNDisplay)
    {
        // 동적으로 모듈 로드 시도
        // MakeShareable로 포인터를 감싸서 TSharedPtr로 변환
        TSharedPtr<IModuleInterface> Module = MakeShareable(FModuleManager::Get().LoadModule("DisplayCluster"));
        bHasNDisplay = Module.IsValid();

        if (!bHasNDisplay)
        {
            Module = MakeShareable(FModuleManager::Get().LoadModule("nDisplay"));
            bHasNDisplay = Module.IsValid();
        }
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("nDisplay module detection result: %s"),
        bHasNDisplay ? TEXT("Available") : TEXT("Not available"));

    return bHasNDisplay;
}

bool FEnvironmentDetector::ScanNetworkInterfaces()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Scanning network interfaces..."));

    // 네트워크 인터페이스 목록 초기화
    NetworkInterfaces.Empty();
    NetworkInterfaceInfo.Empty();

    // 소켓 서브시스템 가져오기
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to get socket subsystem"));
        return false;
    }

    // 로컬 호스트 이름 가져오기
    FString HostName;
    SocketSubsystem->GetHostName(HostName);
    UE_LOG(LogMultiServerSync, Display, TEXT("Local hostname: %s"), *HostName);

    // 모든 로컬 IP 주소 가져오기 부분 수정
    bool bCanBindAll = false; // 참조형으로 전달할 변수 선언
    TSharedPtr<FInternetAddr> LocalAddr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
    if (LocalAddr.IsValid())
    {
        // 기본 로컬 호스트 주소 저장
        FString LocalIP = LocalAddr->ToString(false);
        UE_LOG(LogMultiServerSync, Display, TEXT("Local IP address: %s"), *LocalIP);

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
                    UE_LOG(LogMultiServerSync, Display, TEXT("Detected loopback interface: %s"), *AdapterIP);

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
                    UE_LOG(LogMultiServerSync, Display, TEXT("Detected network interface %s: %s"), *InterfaceName, *AdapterIP);

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

    UE_LOG(LogMultiServerSync, Display, TEXT("Detected %d network interfaces"), NetworkInterfaces.Num());

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

bool FEnvironmentDetector::GetDefaultNetworkInterface(FNetworkInterfaceInfo& OutInfo) const
{
    return GetNetworkInterfaceInfo(TEXT("Default"), OutInfo);
}

bool FEnvironmentDetector::GetFirstMulticastInterface(FNetworkInterfaceInfo& OutInfo) const
{
    for (const auto& Pair : NetworkInterfaceInfo)
    {
        if (Pair.Value.SupportsMulticast && Pair.Value.IsUp)
        {
            OutInfo = Pair.Value;
            return true;
        }
    }
    return false;
}

TMap<FString, FString> FEnvironmentDetector::GetGenlockHardwareDetails() const
{
    TMap<FString, FString> Details;

    if (bHasGenlockHardware)
    {
        Details.Add(TEXT("Type"), TEXT("NVIDIA Quadro Sync"));
        // 추가 정보는 실제 구현에서 추가
    }

    return Details;
}

TMap<FString, FString> FEnvironmentDetector::GetNDisplayDetails() const
{
    TMap<FString, FString> Details;

    if (bHasNDisplay)
    {
        // nDisplay에 대한 추가 정보를 가져올 수 있으면 추가
        Details.Add(TEXT("ModuleName"), FModuleManager::Get().IsModuleLoaded("DisplayCluster") ? TEXT("DisplayCluster") : TEXT("nDisplay"));
    }

    return Details;
}

void FEnvironmentDetector::LogSystemInfo()
{
    // 시스템 정보 로깅
    UE_LOG(LogMultiServerSync, Display, TEXT("System Info:"));
    UE_LOG(LogMultiServerSync, Display, TEXT("  OS: %s"), *FPlatformMisc::GetOSVersion());
    UE_LOG(LogMultiServerSync, Display, TEXT("  CPU: %s"), *FPlatformMisc::GetCPUBrand());
    UE_LOG(LogMultiServerSync, Display, TEXT("  GPU: %s"), *FPlatformMisc::GetPrimaryGPUBrand());

    // float 타입으로 변환하여 %f 형식 지정자 사용
    float PhysicalMemoryGB = static_cast<float>(FPlatformMemory::GetPhysicalGBRam());
    UE_LOG(LogMultiServerSync, Display, TEXT("  Physical Memory: %.2f GB"), PhysicalMemoryGB);
}