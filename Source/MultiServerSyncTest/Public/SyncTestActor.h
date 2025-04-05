// MultiServerSyncTest/Source/MultiServerSyncTest/Public/SyncTestActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SyncTestActor.generated.h"

UCLASS(Blueprintable)
class MULTISERVERSYNCTEST_API ASyncTestActor : public AActor
{
    GENERATED_BODY()

public:
    ASyncTestActor();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    // 시간 동기화 정보를 블루프린트에서 접근 가능하도록 노출
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    float GetTimeOffset() const;

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    float GetPathDelay() const;

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    bool IsMasterNode() const;

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    int32 GetCurrentFrameNumber() const;

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    float GetFrameTimeDelta() const;

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    bool IsTimeInSync() const;

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    FString GetSyncStatusText() const;

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    void StartLoggingToFile(const FString& FileName);

    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Test")
    void StopLoggingToFile();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synchronization")
    bool bForceMaster;

private:
    bool bIsLogging;
    FString LogFileName;
    float LogTimer;
};