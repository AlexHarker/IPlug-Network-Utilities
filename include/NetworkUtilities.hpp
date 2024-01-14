
#ifndef NETWORKUTILITIES_HPP
#define NETWORKUTILITIES_HPP

#include "../dependencies/websocket-tools/websocket-tools.hpp"

#include <shared_mutex>
#include <type_traits>

#include <mutex.h>

#include "IPlugLogger.h"
#include "IPlugStructs.h"

struct NetworkTypes
{
protected:
    
    using ConnectionID = ws_connection_id;

    using SharedMutex = WDL_SharedMutex;
    using SharedLock = WDL_MutexLockShared;
    using ExclusiveLock = WDL_MutexLockExclusive;
    
    class VariableLock
    {
    public:
        
        VariableLock(SharedMutex* mutex, bool shared = true)
        : mMutex(mutex)
        , mShared(shared)
        {
            if (mMutex)
            {
                if (shared)
                    mMutex->LockShared();
                else
                    mMutex->LockExclusive();
            }
        }
        
        ~VariableLock()
        {
            Destroy();
        }
        
        void Destroy()
        {
            if (mMutex)
            {
                if (mShared)
                    mMutex->UnlockShared();
                else
                    mMutex->UnlockExclusive();
                    
                mMutex = nullptr;
            }
        }
        
        void Promote()
        {
            if (mMutex && mShared)
            {
                mMutex->SharedToExclusive();
                mShared = false;
            }
        }
        
        void Demote()
        {
            if (mMutex && !mShared)
            {
                mMutex->ExclusiveToShared();
                mShared = true;
            }
        }
        
        VariableLock(VariableLock const& rhs) = delete;
        VariableLock(VariableLock const&& rhs) = delete;
        void operator = (VariableLock const& rhs) = delete;
        void operator = (VariableLock const&& rhs) = delete;
        
    private:
        SharedMutex *mMutex;
        bool mShared;
    };
};

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
        
        OnServerReady(NClients() - 1); // should defer to main thread
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

// A generic web socket network client that requires an interface for the specifics of the networking

template <class T>
class NetworkClientInterface : protected NetworkTypes
{
public:
    
    // Creation and Deletion
    
    NetworkClientInterface() : mConnection(nullptr) {}
    virtual ~NetworkClientInterface() {}
    
    NetworkClientInterface(const NetworkClientInterface&) = delete;
    NetworkClientInterface& operator=(const NetworkClientInterface&) = delete;
    
    // Public Methods
    
    bool Connect(const char* host, int port = 8001)
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
        }
        else
            DBGMSG("CLIENT: Connection error\n");
        
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
    
    void GetServerName(WDL_String& name)
    {
        SharedLock lock(&mMutex);
        
        name = mServer;
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
    mutable SharedMutex mMutex;
    T *mConnection;
};

// Concrete implementations based on the platform

#ifdef __APPLE__
using NetworkClient = NetworkClientInterface<nw_ws_client>;
using NetworkServer = NetworkServerInterface<nw_ws_server>;
#else
using NetworkClient = NetworkClientInterface<cw_ws_client>;
using NetworkServer = NetworkServerInterface<cw_ws_server>;
#endif

#endif /* NETWORKUTILITIES_HPP */
