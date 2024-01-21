
#ifndef NETWORKPEER_HPP
#define NETWORKPEER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <list>
#include <unordered_set>
#include <utility>

#include "DiscoverablePeer.hpp"
#include "NetworkClient.hpp"
#include "NetworkData.hpp"
#include "NetworkServer.hpp"
#include "NetworkTiming.hpp"

class NetworkPeer : public NetworkServer, NetworkClient
{
    enum class ClientState { Unconfirmed, Confirmed, Connected };
    enum class PeerSource { Unresolved, Discovered, Client, Server, Remote };

    static bool NamePrefer(const char* name1, const char* name2)
    {
        return strcmp(name1, name2) < 0;
    }
    
    // A host (a hostname and port)
    
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
        
        void UpdatePort(uint16_t port)
        {
            mPort = port;
        }
        
        bool Empty() const { return !mName.GetLength(); }
        const char *Name() const { return mName.Get(); }
        uint16_t Port() const { return mPort; }
        
    private:
        
        WDL_String mName;
        uint16_t mPort;
    };
    
    // A list of peers with timeout information
    
    class PeerList
    {
    public:

        // An individual peer with associated information
        
        class Peer
        {
        public:
                
            Peer(const char* name, uint16_t port, PeerSource source, uint32_t time = 0)
            : mHost { name, port }
            , mSource(source)
            , mTime(time)
            {}
            
            Peer(const WDL_String& name, uint16_t port, PeerSource source, uint32_t time = 0)
            : Peer(name.Get(), port, source, time)
            {}
            
            Peer() : Peer(nullptr, 0, PeerSource::Unresolved)
            {}
            
            void UpdatePort(uint16_t port)
            {
                mHost.UpdatePort(port);
            }
            
            void UpdateSource(PeerSource source)
            {
                mSource = source;
            }
            
            void UpdateTime(uint32_t time)
            {
                mTime = std::min(mTime, time);
            }
            
            void AddTime(uint32_t add)
            {
                mTime += add;
            }
            
            const char *Name() const { return mHost.Name(); }
            uint16_t Port() const { return mHost.Port(); }
            PeerSource Source() const { return mSource; }
            uint32_t Time() const { return mTime; }
            
            bool IsClient() const { return mSource == PeerSource::Client; }
            bool IsUnresolved() const { return mSource == PeerSource::Unresolved; }
            
        private:
            
            Host mHost;
            PeerSource mSource;
            uint32_t mTime;
        };
                
        using ListType = std::list<Peer>;

        void Add(const Peer& peer)
        {
            RecursiveLock lock(&mMutex);

            auto findTest = [&](const Peer& a) { return !strcmp(a.Name(), peer.Name()); };
            auto it = std::find_if(mPeers.begin(), mPeers.end(), findTest);
            
            // Add host in order or update details
            
            if (it == mPeers.end())
            {
                auto insertTest = [&](const Peer& a) { return !NamePrefer(a.Name(), peer.Name()); };
                mPeers.insert(std::find_if(mPeers.begin(), mPeers.end(), insertTest), peer);
            }
            else
            {
                it->UpdatePort(peer.Port());
                it->UpdateSource(peer.Source());
                it->UpdateTime(peer.Time());
            }
        }
        
        void Prune(uint32_t maxTime, uint32_t addTime = 0)
        {
            RecursiveLock lock(&mMutex);

            // Added time to hosts if required
            
            if (addTime)
            {
                for (auto it = mPeers.begin(); it != mPeers.end(); it++)
                    it->AddTime(addTime);
            }
              
            // Remove if max time is exceeded
            
            mPeers.remove_if([&](const Peer& a) { return a.Time() >= maxTime; });
        }
        
        void Get(ListType& list) const
        {
            RecursiveLock lock(&mMutex);

            list = mPeers;
        }
        
        int Size() const
        {
            RecursiveLock lock(&mMutex);

            return static_cast<int>(mPeers.size());
        }
        
    private:
        
        mutable RecursiveMutex mMutex;
        ListType mPeers;
    };
    
    // A list of fully confirmed clients
    
    class ClientList : private std::unordered_set<ConnectionID>
    {
    public:
        
        void Add(ConnectionID id)
        {
            RecursiveLock lock(&mMutex);
            
            insert(id);
        }
        
        void Remove(ConnectionID id)
        {
            RecursiveLock lock(&mMutex);
            
            erase(id);
        }
        
        void Clear()
        {
            RecursiveLock lock(&mMutex);
            
            clear();
        }
        
        int Size() const
        {
            RecursiveLock lock(&mMutex);
            
            return static_cast<int>(size());
        }
        
    private:
        
        mutable RecursiveMutex mMutex;
    };
    
    // A class for storing info about the next server a peer should connect to
    
    class NextServer
    {
    public:
        
        void Set(const Host& host)
        {
            RecursiveLock lock(&mMutex);

            mHost = host;
            mTimeOut.Start();
        }
        
        Host Get() const
        {
            RecursiveLock lock(&mMutex);

            if (mTimeOut.Interval() > 4)
                return Host();
            else
                return mHost;
        }
        
    private:
        
        mutable RecursiveMutex mMutex;
        Host mHost;
        CPUTimer mTimeOut;
    };
    
public:
    
    NetworkPeer(uint16_t port = 8001)
    : mClientState(ClientState::Unconfirmed)
    , mDiscoverable(GetHostName().Get(), port)
    {}
    
    ~NetworkPeer()
    {
        mDiscoverable.Stop();
        StopServer();
    }
    
    void Discover(uint32_t interval, uint32_t maxPeerTime)
    {
        if (IsClientConnected())
        {
            if (mClientState == ClientState::Confirmed)
                ClientConnectionConfirmed();

            mPeers.Add({NetworkClient::GetServerName().Get(), Port(), PeerSource::Server});
            mPeers.Prune(maxPeerTime, interval);
            return;
        }
        
        // Attempt the named next server if there is one
        
        auto nextHost = mNextServer.Get();
        
        if (!nextHost.Empty())
        {
            TryConnect(nextHost.Name(), nextHost.Port(), true);
            mPeers.Prune(maxPeerTime, interval);
            return;
        }
        
        // Check that the server is running
        
        if (!IsServerRunning())
            StartServer(mDiscoverable.Port());
        
        // Check that discoverability is on
        
        if (!mDiscoverable.IsRunning())
        {
            mDiscoverable.Start();
            mBonjourRestart.Start();
        }
        
        // Update the list of peers
        
        auto foundPeers = mDiscoverable.FindPeers();
        
        for (auto it = foundPeers.begin(); it != foundPeers.end(); it++)
        {
            // Make sure we conform the name correctly if the host is not resolved
            
            bool unresolved = it->host().empty();
            std::string host = unresolved ? it->name() : it->host();
            PeerSource source = unresolved ? PeerSource::Unresolved : PeerSource::Discovered;
            
            if (unresolved)
            {
                std::string end("-local");
                
                auto pos = host.length() - end.length();
                
                if (host.find(end, pos) != std::string::npos)
                {
                    host.resize(pos);
                    host.append(".local.");
                }
            }
            
            mPeers.Add({host.c_str(), it->port(), source});
        }
            
        // Try to connect to any available servers in order of preference
                
        PeerList::ListType peers;
        mPeers.Get(peers);
        
        for (auto it = peers.begin(); it != peers.end(); it++)
        {
            // Don't attempt to connect to clients, unresolved hosts or to self connect
            
            if (it->IsClient() || it->IsUnresolved() || IsSelf(it->Name()))
                continue;
            
                // Connect or resolve
                
            if (TryConnect(it->Name(), it->Port()))
                break;
            else
                mDiscoverable.Resolve(it->Name());
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
    
    WDL_String GetServerName() const
    {
        WDL_String str;
        
        if (IsServerConnected())
        {
            int NConfirmed = mConfirmedClients.Size();
            str = GetHostName();
            
            if (NConfirmed != NClients())
                str.AppendFormatted(256, " [%d][%d]", NConfirmed, NClients());
            else
                str.AppendFormatted(256, " [%d]", NClients());

            if (IsClientConnected())
                str.AppendFormatted(256, " [%s]", NetworkClient::GetServerName().Get());
        }
        else if (IsClientConnected())
            str = NetworkClient::GetServerName();
        else
            str.Set("Disconnected");
        
        return str;
    }
    
    static WDL_String GetHostName()
    {
        return DiscoverablePeer::GetHostName();
    }
    
    void PeerNames(WDL_String& peersNames)
    {
        PeerList::ListType peers;
        mPeers.Get(peers);
        
        for (auto it = peers.begin(); it != peers.end(); it++)
        {
            peersNames.Append(it->Name());
            
            switch (it->Source())
            {
                case PeerSource::Unresolved:    peersNames.Append(" [Unresolved]");   break;
                case PeerSource::Discovered:    peersNames.Append(" [Discovered]");   break;
                case PeerSource::Client:        peersNames.Append(" [Client]");       break;
                case PeerSource::Server:        peersNames.Append(" [Server]");       break;
                default:                        peersNames.Append(" [Remote]");       break;
            }
            
            peersNames.AppendFormatted(256, " %u\n", it->Time());
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
        mConfirmedClients.Remove(id);
    }
    
    void ClientConnectionConfirmed()
    {
        WDL_String server = NetworkClient::GetServerName();
        
        SendConnectionDataFromClient("Confirm");
        SendConnectionDataFromServer("Switch", server, Port());
        
        mClientState = ClientState::Connected;

        WaitToStop();
        mDiscoverable.Stop();
        StopServer();
        mConfirmedClients.Clear();
    }
    
    bool TryConnect(const char *host, uint16_t port, bool direct = false)
    {
        if (Connect(host, port))
        {
            if (!direct)
            {
                mClientState = ClientState::Unconfirmed;
                WDL_String host = GetHostName();
                uint16_t port = Port();
            
                SendConnectionDataFromClient("Negotiate", host, port, mConfirmedClients.Size());
            }
            else
                ClientConnectionConfirmed();
            
            return true;
        }
        
        return false;
    }
    
    void SendPeerList()
    {
        PeerList::ListType peers;
        mPeers.Get(peers);

        // Don't send unresolved peers
        
        peers.remove_if([](const PeerList::Peer& a) { return a.IsUnresolved(); });
        
        if (peers.size())
        {
            NetworkByteChunk chunk(static_cast<int>(peers.size()));
            
            for (auto it = peers.begin(); it != peers.end(); it++)
            {
                chunk.Add(it->Name());
                chunk.Add(it->Port());
                chunk.Add(it->Time());
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
            mNextServer.Set(Host(server, port));
    }
    
    void HandleConnectionDataToServer(ConnectionID id, NetworkByteStream& stream)
    {
        WDL_String clientName;
        uint16_t port = 0;
        
        if (stream.IsNextTag("Negotiate"))
        {
            WDL_String hostName = GetHostName();
            int numClients = 0;
            int numClientsLocal = mConfirmedClients.Size();
            
            stream.Get(clientName);
            stream.Get(port);
            stream.Get(numClients);

            bool prefer = numClients == numClientsLocal && NamePrefer(hostName.Get(), clientName.Get());
            int confirm = numClients < numClientsLocal || prefer;
            SendConnectionDataToClient(id, "Confirm", confirm);
            
            if (!confirm)
                SetNextServer(clientName.Get(), port);
        }
        else if (stream.IsNextTag("Ping"))
        {
            stream.Get(clientName, port);
            mPeers.Add({clientName, port, PeerSource::Client});
        }
        else if (stream.IsNextTag("Confirm"))
        {
            mConfirmedClients.Add(id);
        }
    }
    
    void HandleConnectionDataToClient(NetworkByteStream& stream)
    {
        WDL_String host;
        uint16_t port = Port();
        uint32_t time = 0;
        int size = 0;
        
        if (stream.IsNextTag("Confirm"))
        {
            int confirm = 0;
            
            stream.Get(confirm);
            
            if (confirm)
                mClientState = ClientState::Confirmed;
            else
                Disconnect();
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

                mPeers.Add({ host, port, PeerSource::Remote, time });
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

    // Tracking the client connection process
    
    std::atomic<ClientState> mClientState;
    
    // Info about other peers
    
    ClientList mConfirmedClients;
    PeerList mPeers;
    NextServer mNextServer;

    // Bonjour
    
    CPUTimer mBonjourRestart;
    DiscoverablePeer mDiscoverable;
};

#endif /* NETWORKPEER_HPP */
