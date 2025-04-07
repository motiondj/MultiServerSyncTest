#pragma once

#include "CoreMinimal.h"
#include "ISyncFrameworkManager.h"

/**
 * FMultiServerSyncModule에서 ISyncFrameworkManager를 가져오기 위한 유틸리티 클래스
 * 에디터 모듈에서 런타임 모듈의 프레임워크 매니저에 접근할 수 있게 합니다.
 */
class MULTISERVERSYNC_API FSyncFrameworkManagerUtil
{
public:
    /**
     * 현재 활성화된 프레임워크 매니저 인스턴스를 가져옵니다.
     * FMultiServerSyncModule::GetFrameworkManager()를 내부적으로 호출합니다.
     */
    static ISyncFrameworkManager* Get();

    /**
     * 프레임워크 매니저가 초기화되었는지 확인합니다.
     */
    static bool IsInitialized();
};