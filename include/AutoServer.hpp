
#ifndef AUTOSERVER_HPP
#define AUTOSERVER_HPP

#include <algorithm>
#include <chrono>
#include <cstring>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DiscoverablePeer.hpp"
#include "NetworkData.hpp"
#include "NetworkTiming.hpp"
#include "NetworkUtilities.hpp"

class AutoServer : public NetworkServer, NetworkClient
{
    enum ClientState { Unconfirmed, Confirmed, Connected };
    
    static bool NamePrefer(const char* name1, const char* name2)
    {
        return strcmp(name1, name2) < 0;
    }
    
    class Host
    {
    public:
        
        Host(const char* name, uint16_t port)
        : mName(name)
        , mPort(port)
        {}
        
        Host(const WDL_String& name, uint16_t port)
        : mName(name)
        , mPort(port)
        {}
        
        Host() : Host("", 0)
        {}
        
        bool Empty() const { return !mName.GetLength(); }
        const char *Name() const { return mName.Get(); }
        uint16_t Port() const { return mPort; }
        
    private:
        
        WDL_String mName;
        uint16_t mPort;
    };
    
    class PeerList
    {
    public:

        class Peer
        {
        public:
            
            Peer(const char* name, uint16_t port, bool client, uint32_t time = 0)
            : mHost { name, port }
            , mClient(client)
            , mTime(time)
            {}
            
            Peer(const WDL_String& name, uint16_t port, bool client, uint32_t time = 0)
            : Peer(name.Get(), port, client, time)
            {}
            
            Peer() : Peer(nullptr, 0, true)
            {}
            
            void UpdateTime(uint32_t time)
            {
                mTime = std::min(mTime, time);
            }
            
            void AddTime(uint32_t add)
            {
                mTime += mTime;
            }
            
            const char *Name() const { return mHost.Name(); }
            uint16_t Port() const { return mHost.Port(); }
            bool IsClient() const { return mClient; }
            uint32_t Time() const { return mTime; }
            
        private:
            
            Host mHost;
            bool mClient;
            uint32_t mTime;
        };
                
        void Add(const Peer& peer)
        {
            auto findTest = [&](const Peer& a) { return !strcmp(a.Name(), peer.Name()); };
            auto it = std::find_if(mPeers.begin(), mPeers.end(), findTest);
            
            // Add host in order or update time
            
            if (it == mPeers.end())
            {
                auto insertTest = [&](const Peer& a) { return NamePrefer(a.Name(), peer.Name()); };
                auto jt = std::find_if(mPeers.begin(), mPeers.end(), insertTest);
                
                mPeers.insert(jt, peer);
            }
            else
                it->UpdateTime(peer.Time());
        }
        
        void Prune(uint32_t maxTime, uint32_t addTime = 0)
        {
            // Added time to hosts if required
            
            if (addTime)
            {
                for (auto it = mPeers.begin(); it != mPeers.end(); it++)
                    it->AddTime(addTime);
            }
              
            // Remove if max time is exceeded
            
            auto removeTest = [&](const Peer& a) { return a.Time() >= maxTime; };
            mPeers.erase(std::remove_if(mPeers.begin(), mPeers.end(), removeTest), mPeers.end());
        }
        
        const Peer& operator [](int N) const
        {
            return mPeers[N];
        }
        
        int Size() const
        {
            return static_cast<int>(mPeers.size());
        }
        
    private:
        
        std::vector<Peer> mPeers;
    };
    
    class NextServer
    {
    public:
        
        void Set(const char *host, uint16_t port)
        {
            mHost = Host { host, port };
            mTimeOut.Start();
        }
        
        Host Get()
        {
            if (mTimeOut.Interval() > 4)
                return { WDL_String(), 0 };
            else
                return mHost;
        }
        
    private:
        
        Host mHost;
        CPUTimer mTimeOut;
    };
    
public:
    
    ~AutoServer()
    {
        mDiscoverable.Stop();
        StopServer();
    }
    
    void Discover(uint32_t interval)
    {
        static constexpr uint32_t maxPeerTime = 8000;
        
        if (IsClientConnected())
        {
            if (mClientState == ClientState::Confirmed)
                ClientConnectionConfirmed();

            mPeers.Prune(maxPeerTime, interval);
            return;
        }
        
        // Attempt the named next server is there is one
        
        auto nextHost = mNextServer.Get();
        
        if (!nextHost.Empty())
        {
            TryConnect(nextHost.Name(), nextHost.Port());
            mPeers.Prune(maxPeerTime, interval);
            return;
        }
        
        // Check that the server is running
        
        if (!IsServerRunning())
            StartServer();
        
        // Check that discoverability is on
        
        if (!mDiscoverable.IsRunning())
        {
            mDiscoverable.Start();
            mBonjourRestart.Start();
            mPeers.Prune(maxPeerTime, interval);
            return;
        }
        
        // Update the list of peers
        
        auto peers = mDiscoverable.FindPeers();
        
        for (auto it = peers.begin(); it != peers.end(); it++)
        {
            if (!it->host().empty())
                mPeers.Add({it->host().c_str(), it->port(), false});
        }
            
        // Try to connect to any available servers in order of preference
                
        for (int i = 0; i < mPeers.Size(); i++)
        {
            auto& peer = mPeers[i];
            
            // Don't attempt to connect to clients or to self connect
            
            if (peer.IsClient() || IsSelf(peer.Name()))
                continue;
            
                // Connect or resolve
                
            if (TryConnect(peer.Name(), peer.Port()))
                break;
            //else
            //    mDiscoverable.Resolve(bonjour_named(peer.Get(), peer.mPort));
        }
        
        if (mBonjourRestart.Interval() > 15)
            mDiscoverable.Stop();
                
        if (IsServerConnected())
        {
            SendPeerList();
            PingClients();
        }
        
        mPeers.Prune(maxPeerTime, interval);
    }
    
    void GetServerName(WDL_String& str) const
    {
        if (IsServerConnected())
        {
            str = GetHostName();
            str.AppendFormatted(256, " [%lu][%d]", mConfirmedClients.size(), NClients());
        }
        else if (IsClientConnected())
            NetworkClient::GetServerName(str);
        else
            str.Set("Disconnected");
    }
    
    WDL_String GetHostName() const
    {
        WDL_String str;
        mDiscoverable.GetHostName(str);
        
        return str;
    }
    
    void PeerNames(WDL_String& peersNames)
    {
        auto peers = mDiscoverable.Peers();
        
        for (auto it = peers.begin(); it != peers.end(); it++)
        {
            peersNames.Append(it->name());
            if (it->host().empty())
                peersNames.Append(" [Unresolved]");
            peersNames.Append("\n");
        }
    }
    
    template <class ...Args>
    void SendToClient(ws_connection_id id, const Args& ...args)
    {
        SendTaggedToClient(GetDataTag(), id, std::forward<const Args>(args)...);
    }
    
    template <class ...Args>
    void SendFromServer(const Args& ...args)
    {
        SendTaggedFromServer(GetDataTag(), std::forward<const Args>(args)...);
    }
    
    template <class ...Args>
    void SendFromClient(const Args& ...args)
    {
        SendTaggedFromClient(GetDataTag(), std::forward<const Args>(args)...);
    }

private:
    
    bool IsSelf(const char* peerName) const
    {
        WDL_String host = GetHostName();
        
        return !strcmp(host.Get(), peerName);
    }
        
    void WaitToStop()
    {
        std::chrono::duration<double, std::milli> ms(500);
        std::this_thread::sleep_for(ms);
    }
    
    constexpr static const char *GetConnectionTag()
    {
        return "~";
    }
    
    constexpr static const char *GetDataTag()
    {
        return "-";
    }
    
    template <class ...Args>
    void SendConnectionDataToClient(ws_connection_id id, const Args& ...args)
    {
        SendTaggedToClient(GetConnectionTag(), id, std::forward<const Args>(args)...);
    }
    
    template <class ...Args>
    void SendConnectionDataFromServer(const Args& ...args)
    {
        SendTaggedFromServer(GetConnectionTag(), std::forward<const Args>(args)...);
    }
    
    template <class ...Args>
    void SendConnectionDataFromClient(const Args& ...args)
    {
        SendTaggedFromClient(GetConnectionTag(), std::forward<const Args>(args)...);
    }

    template <class ...Args>
    void SendTaggedToClient(const char *tag, ws_connection_id id, const Args& ...args)
    {
        SendDataToClient(id, NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    template <class ...Args>
    void SendTaggedFromServer(const char *tag, const Args& ...args)
    {
        SendDataFromServer(NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    template <class ...Args>
    void SendTaggedFromClient(const char *tag, const Args& ...args)
    {
        SendDataFromClient(NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    void OnServerDisconnect(ConnectionID id) override
    {
        mConfirmedClients.erase(id);
    }
    
    void ClientConnectionConfirmed()
    {
        WDL_String hostName;
        NetworkClient::GetServerName(hostName);
        
        SendConnectionDataFromClient("Confirm");
        SendConnectionDataFromServer("Switch", hostName, Port());
        
        mClientState = ClientState::Connected;

        WaitToStop();
        mDiscoverable.Stop();
        StopServer();
        mConfirmedClients.clear();
    }
    
    bool TryConnect(const char *host, uint16_t port)
    {
        if (Connect(host, port))
        {
            mClientState = ClientState::Unconfirmed;
            WDL_String host = GetHostName();
            uint16_t port = Port();
            
            SendConnectionDataFromClient("Negotiate", host, port, mConfirmedClients.size());
            
            return true;
        }
        
        return false;
    }
    
    void SendPeerList()
    {
        if (mPeers.Size())
        {
            NetworkByteChunk chunk(mPeers.Size());
            
            for (int i = 0; i < mPeers.Size(); i++)
            {
                chunk.Add(mPeers[i].Name());
                chunk.Add(mPeers[i].Port());
                chunk.Add(mPeers[i].Time());
            }
            
            SendConnectionDataFromServer("Peers", chunk);
        }
    }
    
    void PingClients()
    {
        SendConnectionDataFromServer("Ping");
    }
    
    void SetNextServer(const char* server, uint16_t port)
    {
        // Prevent self connection
        
        if (!IsSelf(server))
            mNextServer.Set(server, port);
    }
    
    void HandleConnectionDataToServer(ConnectionID id, NetworkByteStream& stream)
    {
        WDL_String clientName;
        uint16_t port = 0;
        
        if (stream.IsNextTag("Negotiate"))
        {
            WDL_String hostName = GetHostName();
            int numClients = 0;
            
            stream.Get(clientName);
            stream.Get(port);
            stream.Get(numClients);

            bool lessClients = numClients < mConfirmedClients.size();
            bool prefer = numClients == mConfirmedClients.size() && NamePrefer(hostName.Get(), clientName.Get());
            int confirm = lessClients || prefer;
            SendConnectionDataToClient(id, "Confirm", confirm);
            
            if (!confirm)
                SetNextServer(clientName.Get(), port);
        }
        else if (stream.IsNextTag("Ping"))
        {
            stream.Get(clientName, port);
            mPeers.Add({clientName, port, true});
        }
        else if (stream.IsNextTag("Confirm"))
        {
            mConfirmedClients.insert(id);
        }
    }
    
    void HandleConnectionDataToClient(NetworkByteStream& stream)
    {
        WDL_String host;
        uint16_t port = mDiscoverable.Port();
        uint32_t time = 0;
        int size = 0;
        
        if (stream.IsNextTag("Confirm"))
        {
            int confirm = 0;
            
            stream.Get(confirm);
            
            if (confirm)
                mClientState = ClientState::Confirmed;
        }
        else if (stream.IsNextTag("Switch"))
        {
            stream.Get(host, port);
            SetNextServer(host.Get(), port);
        }
        else if (stream.IsNextTag("Ping"))
        {
            host = GetHostName();
            SendConnectionDataFromClient("Ping", host, port);
        }
        else if (stream.IsNextTag("Peers"))
        {
            stream.Get(size);
            
            for (int i = 0; i < size; i++)
            {
                stream.Get(host);
                stream.Get(port);
                stream.Get(time);

                mPeers.Add({ host, port, false, time });
            }
        }
    }
    
    void OnDataToServer(ConnectionID id, const iplug::IByteStream& data) final
    {
        NetworkByteStream stream(data);
                
        if (stream.IsNextTag(GetConnectionTag()))
        {
            HandleConnectionDataToServer(id, stream);
        }
        else if (stream.IsNextTag(GetDataTag()))
        {
            ReceiveAsServer(id, stream);
        }
        else
        {
            DBGMSG("Unknown network message to client");
        }
    }
    
    void OnDataToClient(const iplug::IByteStream& data) final
    {
        NetworkByteStream stream(data);

        if (stream.IsNextTag(GetConnectionTag()))
        {
            HandleConnectionDataToClient(stream);
        }
        else if (stream.IsNextTag(GetDataTag()))
        {
            ReceiveAsClient(stream);
        }
        else
        {
            DBGMSG("Unknown network message to client");
        }
    }
    
    virtual void ReceiveAsServer(ConnectionID id, NetworkByteStream& data) {}
    virtual void ReceiveAsClient(NetworkByteStream& data) {}

    ClientState mClientState;
    std::unordered_set<ConnectionID> mConfirmedClients;
    PeerList mPeers;
    NextServer mNextServer;
    CPUTimer mBonjourRestart;
    DiscoverablePeer mDiscoverable;
};

#endif /* AUTOSERVER_HPP */
