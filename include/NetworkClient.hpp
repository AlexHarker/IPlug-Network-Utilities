
#ifndef NETWORKCLIENT_HPP
#define NETWORKCLIENT_HPP

#include <memory>

#include "IPlugLogger.h"
#include "IPlugStructs.h"

#include "NetworkTypes.hpp"

// A generic web socket network client that requires an interface for the specifics of the networking

template <class T>
class NetworkClientInterface : protected NetworkTypes
{
public:
    
    // Creation and Deletion
    
    NetworkClientInterface() : mConnection(nullptr), mPort(0) {}
    virtual ~NetworkClientInterface() {}
    
    NetworkClientInterface(const NetworkClientInterface&) = delete;
    NetworkClientInterface& operator=(const NetworkClientInterface&) = delete;
    
    // Public Methods
    
    bool Connect(const char* host, uint16_t port = 8001)
    {
        DBGMSG("CLIENT: Connection attempt: %s \n", host);
        
        static constexpr ws_client_handlers handlers { DoDataClient, DoCloseClient };
        auto client = T::create(host, port, "/ws", ws_client_owner<handlers>{ this });
        
        VariableLock lock(&mMutex, false);
        
        std::unique_ptr<T> release(mConnection);
        mConnection = client;
        
        if (client)
        {
            DBGMSG("CLIENT: Connection successful\n");
            mServer.Set(host);
            mPort = port;
        }
        else
        {
            DBGMSG("CLIENT: Connection error\n");
            mServer.Set("");
            mPort = 0;
        }
        
        lock.Destroy();
        
        return IsClientConnected();
    }
    
    void SendDataFromClient(const iplug::IByteChunk& chunk)
    {
        SharedLock lock(&mMutex);
        
        if (mConnection)
        {
            // FIX - errors
            
            mConnection->send(chunk.GetData(), chunk.Size());
        }
    }
    
    bool IsClientConnected() const
    {
        SharedLock lock(&mMutex);
        
        return mConnection;
    }
    
    void GetServerName(WDL_String& name) const
    {
        SharedLock lock(&mMutex);
        
        name = mServer;
    }
    
    uint16_t Port() const
    {
        SharedLock lock(&mMutex);
        
        return mPort;
    }
    
private:
    
    // Customisable Methods
    
    virtual void OnDataToClient(const iplug::IByteStream& data) = 0;
    virtual void OnCloseClient() {}
    
    static NetworkClientInterface* AsClient(void *x)
    {
        return reinterpret_cast<NetworkClientInterface *>(x);
    }
    
    // Handlers
    
    void HandleClose()
    {
        VariableLock lock(&mMutex);
        
        // Avoid closing twice (in case the API calls close multiple times)
        
        if (mConnection)
        {
            lock.Promote();
            std::unique_ptr<T> release(mConnection);
            mConnection = nullptr;
            mServer.Set("");
            mPort = 0;
            lock.Demote();
            OnCloseClient();
            DBGMSG("CLIENT: Disconnected\n");
        }
    }
    
    void HandleData(const void *pData, size_t size)
    {
        SharedLock lock(&mMutex);
        
        iplug::IByteStream stream(pData, static_cast<int>(size));
        
        OnDataToClient(stream);
    }
    
    // Static Handlers
    
    static void DoDataClient(ConnectionID id, const void *pData, size_t size, void *x)
    {
        AsClient(x)->HandleData(pData, size);
    }
    
    static void DoCloseClient(ConnectionID id, void *x)
    {
        AsClient(x)->HandleClose();
    }
    
    WDL_String mServer;
    uint16_t mPort;
    mutable SharedMutex mMutex;
    T *mConnection;
};

// Concrete implementation based on the platform

#ifdef __APPLE__
using NetworkClient = NetworkClientInterface<nw_ws_client>;
#else
using NetworkClient = NetworkClientInterface<cw_ws_client>;
#endif

#endif /* NETWORKUTILITIES_HPP */

