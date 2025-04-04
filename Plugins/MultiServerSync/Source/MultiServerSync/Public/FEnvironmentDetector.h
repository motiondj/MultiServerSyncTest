// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"
#include "Interfaces/IPv4/IPv4Address.h"

/**
 * 네트워크 인터페이스 정보 구조체
 * 시스템에서 감지된 각 네트워크 인터페이스의 상세 정보를 저장
 */
struct FNetworkInterfaceInfo
{
    /** 인터페이스 이름 */
    FString Name;

    /** IP 주소 (문자열 형식) */
    FString IPAddress;

    /** 서브넷 마스크 (문자열 형식) */
    FString SubnetMask;

    /** 인터페이스가 활성화되어 있는지 여부 */
    bool IsUp;

    /** 멀티캐스트를 지원하는지 여부 */
    bool SupportsMulticast;

    /** IPv4 주소 객체로 변환 */
    FIPv4Address GetIPv4Address() const
    {
        FIPv4Address Result;
        FIPv4Address::Parse(IPAddress, Result);
        return Result;
    }

    /** 기본 생성자 */
    FNetworkInterfaceInfo()
        : Name(TEXT("Unknown"))
        , IPAddress(TEXT("0.0.0.0"))
        , SubnetMask(TEXT("0.0.0.0"))
        , IsUp(false)
        , SupportsMulticast(false)
    {
    }
};

/**
 * Environment detector class that implements the IEnvironmentDetector interface
 * Responsible for discovering hardware and software capabilities in the environment
 */
class FEnvironmentDetector : public IEnvironmentDetector
{
public:
    /** Constructor */
    FEnvironmentDetector();

    /** Destructor */
    virtual ~FEnvironmentDetector();

    // Begin IEnvironmentDetector interface
    virtual bool Initialize() override;
    virtual void Shutdown() override;
    virtual bool IsFeatureAvailable(const FString& FeatureName) override;
    virtual TMap<FString, FString> GetFeatureInfo(const FString& FeatureName) override;
    // End IEnvironmentDetector interface

    /** Detect genlock hardware (Quadro Sync) */
    bool DetectGenlockHardware();

    /** Detect nDisplay module */
    bool DetectNDisplay();

    /** Scan network interfaces */
    bool ScanNetworkInterfaces();

    /** Get the list of available network interfaces */
    TArray<FString> GetNetworkInterfaces() const;

    /** Check if genlock hardware is available */
    bool HasGenlockHardware() const;

    /** Check if nDisplay module is available */
    bool HasNDisplay() const;

    /**
     * 특정 네트워크 인터페이스의 상세 정보를 가져옴
     * @param InterfaceName - 정보를 요청할 인터페이스 이름
     * @param OutInfo - 결과가 저장될 구조체 참조
     * @return 인터페이스 정보를 찾았으면 true, 아니면 false
     */
    bool GetNetworkInterfaceInfo(const FString& InterfaceName, FNetworkInterfaceInfo& OutInfo) const;

    /**
     * 기본 네트워크 인터페이스의 정보를 가져옴
     * @param OutInfo - 결과가 저장될 구조체 참조
     * @return 인터페이스 정보를 찾았으면 true, 아니면 false
     */
    bool GetDefaultNetworkInterface(FNetworkInterfaceInfo& OutInfo) const;

    /**
     * 멀티캐스트를 지원하는 첫 번째 인터페이스의 정보를 가져옴
     * @param OutInfo - 결과가 저장될 구조체 참조
     * @return 인터페이스 정보를 찾았으면 true, 아니면 false
     */
    bool GetFirstMulticastInterface(FNetworkInterfaceInfo& OutInfo) const;

    /**
     * 젠락 하드웨어(Quadro Sync)의 상세 정보를 가져옴
     * @return 젠락 하드웨어 정보를 담은 맵
     */
    TMap<FString, FString> GetGenlockHardwareDetails() const;

    /**
     * nDisplay 모듈의 상세 정보를 가져옴
     * @return nDisplay 모듈 정보를 담은 맵
     */
    TMap<FString, FString> GetNDisplayDetails() const;

private:
    /** Available network interfaces */
    TArray<FString> NetworkInterfaces;

    /** 네트워크 인터페이스 정보 맵 */
    TMap<FString, FNetworkInterfaceInfo> NetworkInterfaceInfo;

    /** Genlock hardware detected flag */
    bool bHasGenlockHardware;

    /** nDisplay module detected flag */
    bool bHasNDisplay;

    /** Is the detector initialized */
    bool bIsInitialized;

#if PLATFORM_WINDOWS
    /** NVIDIA Quadro Sync 감지 함수 */
    bool DetectNVIDIAQuadroSync();
#endif

    /**
     * 하드웨어 정보를 로깅
     * 디버깅 및 문제 해결을 위한 정보 수집
     */
    void LogSystemInfo();
};