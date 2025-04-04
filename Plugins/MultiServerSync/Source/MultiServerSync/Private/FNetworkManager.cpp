#include "FNetworkManager.h"

FNetworkManager::FNetworkManager()
    : BroadcastSocket(nullptr)
    , ReceiveSocket(nullptr)
    , ReceiverThread(nullptr)
    , MessageHandler(nullptr)
    , ReceiverWorker(nullptr)  // 순서 변경
    , bIsInitialized(false)    // 순서 변경
{
}

FNetworkManager::~FNetworkManager()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FNetworkManager::Initialize()
{
    bIsInitialized = true;
    return true;
}

void FNetworkManager::Shutdown()
{
    bIsInitialized = false;
}

bool FNetworkManager::SendMessage(const FString& EndpointId, const TArray<uint8>& Message)
{
    return false;
}

bool FNetworkManager::BroadcastMessage(const TArray<uint8>& Message)
{
    return false;
}

void FNetworkManager::RegisterMessageHandler(TFunction<void(const FString&, const TArray<uint8>&)> Handler)
{
    MessageHandler = Handler;
}

bool FNetworkManager::DiscoverServers()
{
    return false;
}

TArray<FString> FNetworkManager::GetDiscoveredServers() const
{
    return DiscoveredServers;
}

FGuid FNetworkManager::GenerateProjectId() const
{
    return FGuid::NewGuid();
}

void FNetworkManager::SetProjectId(const FGuid& InProjectId)
{
    ProjectId = InProjectId;
}

FGuid FNetworkManager::GetProjectId() const
{
    return ProjectId;
}