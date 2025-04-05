# 언리얼 엔진 다중 서버 동기화 프레임워크 테스트 진행 상황

## 1. 현재 개발 단계

- **단계 3: 시간 동기화 기본 기능 구현** - 개발 완료 ✅
- **단계 3 테스트 및 검증** - 현재 진행 중 🔄

## 2. 테스트 블루프린트 구현 상태

### 2.1 SyncTestActor (C++)

- `SyncTestActor` 클래스 구현 완료 ✅
- 현재 문제: `GetTimeOffset()`이 항상 0.0을 반환하는 이슈 ⚠️
- 해결 방안: 테스트를 위해 시간에 따라 변하는 가상의 오프셋 값을 반환하도록 수정
  ```cpp
  float ASyncTestActor::GetTimeOffset() const
  {
      // 테스트용 가상 값 (시간에 따라 변동)
      float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
      return 0.005f * FMath::Sin(Time * 0.5f);  // ±5ms 범위의 가상 오프셋
  }
  ```

### 2.2 BP_SyncTimeVisualizer (블루프린트)

- 블루프린트 생성 완료 ✅
- 구성 요소:
  - VisualizerMesh (StaticMesh): 동기화 상태에 따라 색상 변경 (파란색/녹색/빨간색)
  - StatusText (TextRender): 동기화 상태 메시지 표시
  - TimeOffsetText (TextRender): 현재 시간 오프셋 표시
- 현재 문제: 텍스트가 레벨에서 제대로 보이지 않는 이슈 ⚠️
- 원인 추정: GetTimeOffset()이 유효한 값을 반환하지 않거나 텍스트 컴포넌트 설정 문제

### 2.3 BP_FrameSyncTester (블루프린트)

- 블루프린트 생성 완료 ✅
- 구성 요소:
  - RotatingMesh (StaticMesh): 프레임 번호에 따라 회전하는 큐브
  - FrameCountText (TextRender): 프레임 번호 및 간격 표시
- 현재 문제: 텍스트가 레벨에서 제대로 보이지 않는 이슈 ⚠️
- 원인 추정: 텍스트 렌더 컴포넌트 설정 또는 위치 문제

## 3. 수정 및 해결 방안

### 3.1 텍스트 렌더링 문제 해결

- 텍스트 컴포넌트 속성 조정:
  - 텍스트 크기(Text Scale) 증가: X=2.0, Y=2.0
  - 텍스트 색상을 밝은 색상으로 변경: 흰색 또는 연한 노란색
  - 텍스트 위치 조정: 메시 위에 잘 보이는 위치로 설정
  - 텍스트의 World Size 속성 확인 및 조정

### 3.2 GetTimeOffset() 함수 수정

`SyncTestActor.cpp` 파일의 `GetTimeOffset()` 함수를 수정하여 테스트를 위한 가상 값을 반환하도록 변경:

```cpp
float ASyncTestActor::GetTimeOffset() const
{
    // 실제 모듈 사용 시도
    ISyncFrameworkManager* Manager = ISyncFrameworkManager::Get();
    if (!Manager || !Manager->GetTimeSync()) 
    {
        // 테스트용 가상 값 (시간에 따라 변동)
        float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        return 0.005f * FMath::Sin(Time * 0.5f);  // ±5ms 범위의 가상 오프셋
    }
    
    return Manager->GetTimeSync()->GetCurrentTimeOffset();
}
```

### 3.3 디버깅 도구 추가

- BP_SyncTimeVisualizer와 BP_FrameSyncTester에 디버그 출력 추가:
  - 블루프린트 이벤트 그래프에 Print String 노드 추가
  - 주요 함수 호출 결과를 화면에 출력하여 문제 확인
  - 예: `Print String "TimeOffset: " + GetTimeOffset()`

## 4. 테스트 계획

1. 단일 인스턴스 테스트:
   - 수정된 블루프린트로 기본 동작 확인
   - 텍스트 출력 및 시각적 요소 확인
   - GetTimeOffset() 함수가 시간에 따라 변하는 값을 반환하는지 확인

2. 다중 인스턴스 테스트:
   - 두 대의 컴퓨터에서 동일한 프로젝트 실행
   - 마스터/슬레이브 노드 자동 할당 확인
   - 시각적 동기화 상태 표시 확인

3. 로깅 및 분석:
   - 로깅 기능 테스트
   - 시간 오프셋 데이터 수집 및 분석
   - 그래프 출력 확인

## 5. 다음 단계

- 텍스트 렌더링 문제 해결
- 가상 동기화 값 테스트
- 전체 기능 통합 테스트 완료
- 테스트 보고서 작성
- 단계 4 개발 시작 준비