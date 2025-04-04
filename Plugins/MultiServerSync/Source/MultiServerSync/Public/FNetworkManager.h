// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"
#include "Common/UdpSocketBuilder.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

/**
 * Network manager class that implements the INetworkManager interface
 * Handles all network communication between servers
 */
class FNetworkManager : public INetworkManager
{
public:
    /** Constructor */
    FNetworkManager();

    /** Destructor */
    virtual ~FNetworkManager();

    // Begin INetworkManager interface
    virtual bool Initialize() override;
    virtual void Shutdown() override;
    virtual bool SendMessage(const FString& EndpointId, const TArray<uint8>& Message) override;
    virtual bool BroadcastMessage(const TArray<uint8>& Message) override;
    virtual void RegisterMessageHandler(TFunction<void(const FString&, const TArray<uint8>&)> Handler) override;
    // End INetworkManager interface

    /** Discover other servers on the network */
    bool DiscoverServers();

    /** Get the list of discovered servers */
    TArray<FString> GetDiscoveredServers() const;

    /** Generate unique project identifier */
    FGuid GenerateProjectId() const;

    /** Set the project identifier */
    void SetProjectId(const FGuid& ProjectId);

    /** Get the project identifier */
    FGuid GetProjectId() const;

private:
    /** Broadcast socket for server discovery */
    FSocket* BroadcastSocket;

    /** Receiving socket for messages */
    FSocket* ReceiveSocket;

    /** Thread for receiving messages */
    FRunnableThread* ReceiverThread;

    /** Message handler callback */
    TFunction<void(const FString&, const TArray<uint8>&)> MessageHandler;

    /** List of discovered server endpoints */
    TArray<FString> DiscoveredServers;

    /** Project unique identifier */
    FGuid ProjectId;

    /** Is the network manager initialized */
    bool bIsInitialized;

    /** Receive thread worker class declaration */
    class FReceiverWorker;

    /** Instance of the receiver worker */
    FReceiverWorker* ReceiverWorker;
};