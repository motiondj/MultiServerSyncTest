# 언리얼 엔진 플러그인 모듈 간 링크 문제 해결 가이드

## 문제 개요

언리얼 엔진에서 플러그인을 개발할 때, 여러 모듈로 나누어 구성하는 것이 일반적입니다. 주로 런타임 모듈과 에디터 모듈로 분리합니다. 이 과정에서 에디터 모듈이 런타임 모듈의 클래스나 함수를 사용하려고 할 때 링커 오류가 발생할 수 있습니다.

### 발생한 링커 오류 예시

```
error LNK2019: "public: class TSharedPtr<class IEnvironmentDetector,1> __cdecl FSyncFrameworkManager::GetEnvironmentDetector(void)const" (?GetEnvironmentDetector@FSyncFrameworkManager@@QEBA?AV?$TSharedPtr@VIEnvironmentDetector@@$00@@XZ) 함수에서 참조되는 확인할 수 없는 외부 기호
```

이 오류는 `MultiServerSyncEditor` 모듈이 `MultiServerSync` 모듈의 `FSyncFrameworkManager` 클래스 메서드를 사용하려고 할 때 발생했습니다.

## 문제의 원인

1. **모듈 간 의존성 설정 미흡**: 비록 Build.cs 파일에서 모듈 의존성을 선언했지만, 모듈 간 링크가 올바르게 설정되지 않았습니다.

2. **클래스 가시성 제한**: 일부 클래스가 모듈 내부에서만 접근 가능하도록 설정되어 있을 수 있습니다.

3. **바이너리 불일치**: 모듈을 컴파일할 때 서로 다른 바이너리 형식이나 링크 설정을 사용할 수 있습니다.

4. **DLL 경계 문제**: 다른 DLL에서 정의된 템플릿 클래스나 컨테이너를 사용할 때 문제가 발생할 수 있습니다.

## 해결 방법

### 1. 모듈 의존성 설정 강화

`MultiServerSyncEditor.Build.cs` 파일에서 다음과 같이 설정을 추가하세요:

```csharp
using UnrealBuildTool;
using System.IO;

public class MultiServerSyncEditor : ModuleRules
{
    public MultiServerSyncEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // 공용 헤더 경로
        PublicIncludePaths.AddRange(
            new string[] {
                // 공용 헤더 디렉토리 추가
            }
        );
        
        // 비공개 헤더 경로 (런타임 모듈의 Private 경로에 접근하도록 설정)
        PrivateIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "../MultiServerSync/Private"),
            }
        );
        
        // 런타임 모듈 종속성 추가 (중요)
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "MultiServerSync" // 런타임 모듈에 대한 의존성
            }
        );
        
        // 모듈 바인딩 설정
        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "MultiServerSync",
            }
        );
        
        // 정적 링크 설정 (모놀리식 빌드에서만)
        if (Target.LinkType == TargetLinkType.Monolithic)
        {
            PublicAdditionalLibraries.Add("MultiServerSync.lib");
        }
        
        // 기타 필요한 모듈
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "InputCore",
                "UnrealEd",
                "LevelEditor",
                "Slate",
                "SlateCore",
                "EditorStyle",
            }
        );
    }
}
```

### 2. 인터페이스 기반 설계 강화

모듈 간 경계를 넘을 때는 인터페이스를 통해 접근하는 것이 좋습니다. 구체적인 클래스 구현 대신 인터페이스에 의존하세요.

```cpp
// 직접 클래스 인스턴스 생성 대신
// FEnvironmentDetector EnvironmentDetector; // 이렇게 하지 말고

// 프레임워크 매니저를 통해 인터페이스 타입으로 접근
FSyncFrameworkManager& Manager = FSyncFrameworkManager::Get();
TSharedPtr<IEnvironmentDetector> EnvironmentDetector = Manager.GetEnvironmentDetector();
```

### 3. 클래스 선언과 정의 분리

클래스 선언과 정의를 적절히 분리하여 모듈 간 의존성을 줄이세요.

```cpp
// 헤더 파일 (.h)
class MULTISERVERSYNC_API FSyncFrameworkManager
{
public:
    static FSyncFrameworkManager& Get();
    TSharedPtr<IEnvironmentDetector> GetEnvironmentDetector() const;
    // ...
};

// 구현 파일 (.cpp)
FSyncFrameworkManager& FSyncFrameworkManager::Get()
{
    // 구현...
}

TSharedPtr<IEnvironmentDetector> FSyncFrameworkManager::GetEnvironmentDetector() const
{
    // 구현...
}
```

### 4. 모듈 API 매크로 사용

모듈 경계를 넘어 사용할 클래스나 함수에는 API 매크로를 사용하세요:

```cpp
// MyModule.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMyModule : public IModuleInterface
{
public:
    // 인터페이스 구현
};

// 중요: 모듈 API 매크로 활성화
#define MULTISERVERSYNC_API DLLEXPORT
```

### 5. 간소화된 테스트 사용

복잡한 테스트 대신 더미 테스트를 사용하여 기본적인 기능 검증을 먼저 수행하세요:

```cpp
// 더미 테스트 예시
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentDetectorDummyTest, "MultiServerSync.EnvironmentDetector.Dummy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnvironmentDetectorDummyTest::RunTest(const FString& Parameters)
{
    // 기본 기능만 테스트
    return true;
}
```

## 추가 링크 방법 (고급)

### 1. 정적 라이브러리 사용

런타임 모듈을 동적 라이브러리(DLL) 대신 정적 라이브러리로 빌드하면 링크 문제를 회피할 수 있습니다:

```csharp
// 정적 라이브러리로 빌드 설정
PrecompileForTargets = PrecompileTargetsType.Any;
bPrecompile = true;
```

### 2. 런타임 함수 로드

동적으로 함수를 로드하는 방법도 있습니다:

```cpp
// 동적 함수 로드 예시
typedef TSharedPtr<IEnvironmentDetector> (*GetEnvironmentDetectorFunc)();
void* ModuleHandle = FPlatformProcess::GetDllHandle(TEXT("MultiServerSync"));
if (ModuleHandle)
{
    GetEnvironmentDetectorFunc GetDetectorFunc = (GetEnvironmentDetectorFunc)FPlatformProcess::GetDllExport(ModuleHandle, TEXT("GetEnvironmentDetector"));
    if (GetDetectorFunc)
    {
        TSharedPtr<IEnvironmentDetector> Detector = GetDetectorFunc();
    }
}
```

## 모듈 구조 재설계 고려사항

링크 문제가 계속되면 모듈 구조를 재설계하는 것도 고려해볼 수 있습니다:

1. **공통 모듈 추가**: 두 모듈이 공유하는 코드를 별도의 공통 모듈로 분리

2. **단일 모듈로 통합**: 규모가 작은 프로젝트라면 모듈을 하나로 통합하는 것도 방법

3. **인터페이스 모듈 도입**: 순수 인터페이스만 포함하는 별도의 모듈을 만들어 다른 모듈들이 이에 의존하도록 설계

## 언리얼 엔진 빌드 시스템 고려사항

1. **Hot Reload 비활성화**: 개발 중 링크 문제가 발생하면 Hot Reload를 비활성화하고 전체 재빌드 시도

2. **빌드 제품군 정리**: 주기적으로 Binaries 및 Intermediate 폴더를 정리하여 캐시된 빌드 파일로 인한 문제 방지

3. **디버그 빌드 사용**: 개발 중에는 디버그 빌드를 사용하여 더 자세한 오류 정보 확인

4. **UnrealBuildTool 로그 확인**: `-verbose` 플래그를 사용하여 UnrealBuildTool의 상세 로그 확인

## 언리얼 엔진 버전별 문제 해결

언리얼 엔진 버전에 따라 모듈 링크 방식이 다를 수 있으니 해당 버전의 공식 문서를 참조하세요. 특히 UE5에서는 모듈 시스템이 UE4와 비교하여 일부 변경되었습니다.

## 결론

모듈 간 링크 문제는 복잡할 수 있지만, 위의 방법들을 통해 대부분 해결할 수 있습니다. 중요한 것은 깔끔한 인터페이스 설계와 적절한 모듈 의존성 설정입니다. 문제가 지속되면 모듈 구조를 단순화하거나 인터페이스 중심으로 재설계하는 것이 좋습니다.