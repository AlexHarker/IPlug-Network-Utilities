
#ifndef NETWORKSERVER_HPP
#define NETWORKSERVER_HPP

#include <memory>

#include "IPlugLogger.h"
#include "IPlugStructs.h"

#include "NetworkTypes.hpp"

// A generic web socket network server that requires an interface for the specifics of the networking

template <class T>
class NetworkServerInterface : protected NetworkTypes
{
public:
    
    NetworkServerInterface() : mServer(nullptr)  {}
    virtual ~NetworkServerInterface() {}
    
    NetworkServerInterface(const NetworkServerInterface&) = delete;
    NetworkServerInterface& operator=(const NetworkServerInterface&) = delete;
    
    void StartServer(const char* port = "8001")
    {
        VariableLock lock(&mMutex);
        
        if (!mServer)
        {
            static constexpr ws_server_handlers handlers = { DoConnectServer,
                                                             DoReadyServer,
                                                             DoDataServer,
                                                             DoCloseServer
                                                            };
            
            auto server = T::create(port, "/ws", ws_server_owner<handlers>{ this });
            
            lock.Promote();
            mServer = server;
            
            DBGMSG("SERVER: Websocket server running on port %s\n", port);
        }
        else
            DBGMSG("SERVER: Websocket server already running on post %s\n", port);
    }
    
    void StopServer()
    {
        VariableLock lock(&mMutex);

        if (mServer)
        {
            lock.Promote();
            std::unique_ptr<T> release(mServer);
            mServer = nullptr;
            lock.Destroy();
            DBGMSG("SERVER: Destroyed\n");
        }
    }
    
    int NClients() const
    {
        SharedLock lock(&mMutex);
        
        return mServer ? static_cast<int>(mServer->size()) : 0;
    }
    
    bool SendDataToClient(ws_connection_id id, const iplug::IByteChunk& chunk)
    {
        SharedLock lock(&mMutex);
        
        if (mServer)
        {
            mServer->send(id, chunk.GetData(), chunk.Size());
            return true;
        }
        
        return false;
    }
    
    bool SendDataFromServer(const iplug::IByteChunk& chunk)
    {
        SharedLock lock(&mMutex);
        
        if (mServer)
        {
            mServer->send(chunk.GetData(), chunk.Size());
            return true;
        }
        
        return false;
    }
    
    bool IsServerConnected() const
    {
        SharedLock lock(&mMutex);
        
        return NClients();
    }
    
    bool IsServerRunning() const
    {
        SharedLock lock(&mMutex);
        
        return mServer;
    }
    
private:
    
    // Customisable Handlers
    
    virtual void OnServerReady(ConnectionID id) {}
    virtual void OnServerDisconnect(ConnectionID id) {}
    virtual void OnDataToServer(ConnectionID id, const iplug::IByteStream& data) = 0;
    
    // Handlers
    
    void HandleSocketConnection(ConnectionID id)
    {
        SharedLock lock(&mMutex);
        
        DBGMSG("SERVER: Connected\n");
    }
    
    void HandleSocketReady(ConnectionID id)
    {
        SharedLock lock(&mMutex);
                
        DBGMSG("SERVER: New connection - num clients %i\n", NClients());
        
        OnServerReady(id);
    }
    
    void HandleSocketData(ConnectionID id, const void *pData, size_t size)
    {
        SharedLock lock(&mMutex);
                
        if (mServer)
        {
            iplug::IByteStream stream(pData, static_cast<int>(size));
            OnDataToServer(id, stream);
        }
    }
    
    void HandleSocketClose(ConnectionID id)
    {
        SharedLock lock(&mMutex);
                
        DBGMSG("SERVER: Closed connection - num clients %i\n", NClients());
        
        OnServerDisconnect(id);
    }
    
    // Static Handlers
    
    static void DoConnectServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        // N.B. Return values are 0 for success and 1 for close
        
        if (pServer)
            pServer->HandleSocketConnection(id);
    }
    
    static void DoReadyServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        if (pServer)
            pServer->HandleSocketReady(id);
    }
    
    static void DoDataServer(ConnectionID id, const void* pData, size_t size, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        if (pServer)
            pServer->HandleSocketData(id, pData, size);
    }
    
    static void DoCloseServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);
        
        if (pServer)
            pServer->HandleSocketClose(id);
    }
    
    static NetworkServerInterface* AsServer(void *pUntypedServer)
    {
        auto pServer = reinterpret_cast<NetworkServerInterface *>(pUntypedServer);
        
        // This may occur when a request hits the server before the context is saved
        
        if (pServer->mServer == nullptr)
            return nullptr;
        
        return pServer;
    }
    
    T *mServer;

protected:
    
    mutable SharedMutex mMutex;
};

// Concrete implementations based on the platform

#ifdef __APPLE__
using NetworkServer = NetworkServerInterface<nw_ws_server>;
#else
using NetworkServer = NetworkServerInterface<cw_ws_server>;
#endif

#endif /* NETWORKSERVER_HPP */
