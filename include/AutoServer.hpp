
#ifndef AUTOSERVER_HPP
#define AUTOSERVER_HPP

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>
#include <vector>

#include "DiscoverablePeer.hpp"
#include "NetworkData.hpp"
#include "NetworkTiming.hpp"
#include "NetworkUtilities.hpp"

class AutoServer : public NetworkServer, NetworkClient
{
    class PeerList
    {
    public:

        struct HostLinger
        {
            HostLinger(const char* name, uint32_t time = 0)
            : mHost(name)
            , mTime(time)
            {}
            
            HostLinger(const WDL_String& name, uint32_t time = 0)
            : mHost(name)
            , mTime(time)
            {}
            
            HostLinger()
            : HostLinger(nullptr)
            {}
            
            WDL_String mHost;
            uint32_t mTime;
        };
                
        void Add(const HostLinger& host)
        {
            auto findTest = [&](const HostLinger& a) { return !strcmp(a.mHost.Get(), host.mHost.Get()); };
            auto it = std::find_if(mPeers.begin(), mPeers.end(), findTest);
            
            // Update time or add host
            
            if (it != mPeers.end())
                it->mTime = std::min(it->mTime, host.mTime);
            else
                mPeers.push_back(host);
        }
        
        void Prune(uint32_t maxTime, uint32_t addTime = 0)
        {
            // Added time to hosts if required
            
            if (addTime)
            {
                for (auto it = mPeers.begin(); it != mPeers.end(); it++)
                    it->mTime += addTime;
            }
              
            // Remove if max time is exceeded
            
            auto removeTest = [&](const HostLinger& a) { return a.mTime >= maxTime; };
            mPeers.erase(std::remove_if(mPeers.begin(), mPeers.end(), removeTest), mPeers.end());
        }
        
        const HostLinger& operator [](int N) const
        {
            return mPeers[N];
        }
        
        int Size() const
        {
            return static_cast<int>(mPeers.size());
        }
        
    private:
        
        std::vector<HostLinger> mPeers;
    };
    
    class NextServer
    {
    public:
        
        void Set(const char *host, uint32_t port)
        {
            mHost.Set(host);
            mPort = port;
            mTimeOut.Start();
        }
        
        std::pair<WDL_String, uint32_t> Get()
        {
            if (mTimeOut.Interval() > 4)
                return { WDL_String(), 0 };
            else
                return { mHost, mPort };
        }
        
    private:
        
        WDL_String mHost;
        uint32_t mPort;
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
            mPeers.Prune(maxPeerTime, interval);
            return;
        }
        
        // Attempt the named next server is there is one
        
        auto nextServer = mNextServer.Get();
        
        if (nextServer.first.GetLength())
        {
            TryConnect(nextServer.first.Get(), nextServer.second);
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
            return;
        }
        
        // Try to connect to any available servers in order of preference
        
        auto peers = mDiscoverable.FindPeers();
        
        WDL_String host;
        mDiscoverable.GetHostName(host);
        host.Append(".");
        
        auto raw_cmp = [](const char *a, const char *b)
        {
            return strcmp(a, b) < 0;
        };
        
        auto cmp = [&](const bonjour_service& a, const bonjour_service& b)
        {
            return raw_cmp(a.host().c_str(), b.host().c_str());
        };
        
        auto rmv = [&](const bonjour_service& a)
        {
            return a.host().empty() || raw_cmp(host.Get(), a.host().c_str()) || !strcmp(host.Get(), a.host().c_str());
        };
        
        peers.remove_if(rmv);
        peers.sort(cmp);
        
        // Attempt to connect in order
        
        for (auto it = peers.begin(); it != peers.end(); it++)
        {
            if (TryConnect(it->host().c_str(), it->port()))
                break;
            else
                mDiscoverable.Resolve(*it);
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
    
    void GetServerName(WDL_String& str)
    {
        if (IsServerConnected())
        {
            mDiscoverable.GetHostName(str);
            str.AppendFormatted(256, " [%d]", NClients());
        }
        else if (IsClientConnected())
            NetworkClient::GetServerName(str);
        else
            str.Set("Disconnected");
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
    void SendConnectionDataFromServer(ws_connection_id id, const Args& ...args)
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
    
    bool TryConnect(const char *host, uint32_t port)
    {
        if (Connect(host, port))
        {
            SendConnectionDataFromServer("SwitchServer", host, port);
            
            WaitToStop();
            mDiscoverable.Stop();
            StopServer();
            
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
                chunk.Add(mPeers[i].mHost);
                chunk.Add(mPeers[i].mTime);
            }
            
            SendConnectionDataFromServer("Peers", chunk);
        }
    }
    
    void PingClients()
    {
        SendConnectionDataFromServer("Ping");
    }
    
    void HandleConnectionDataToServer(ConnectionID id, NetworkByteStream& stream)
    {
        if (stream.IsNextTag("Ping"))
        {
            WDL_String host;
            
            stream.Get(host);
            mPeers.Add(host);
        }
    }
    
    void HandleConnectionDataToClient(NetworkByteStream& stream)
    {
        if (stream.IsNextTag("SwitchServer"))
        {
            WDL_String host;
            uint32_t port;
            
            stream.Get(host, port);
            mNextServer.Set(host.Get(), port);
        }
        else if (stream.IsNextTag("Ping"))
        {
            WDL_String host;
            mDiscoverable.GetHostName(host);
            
            SendConnectionDataFromClient("Ping", host);
        }
        else if (stream.IsNextTag("Peers"))
        {
            int size;
            
            stream.Get(size);
            
            for (int i = 0; i < size; i++)
            {
                WDL_String host;
                uint32_t time;
                
                stream.Get(host);
                stream.Get(time);

                mPeers.Add({ host, time });
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

    PeerList mPeers;
    NextServer mNextServer;
    CPUTimer mBonjourRestart;
    DiscoverablePeer mDiscoverable;
};

#endif /* AUTOSERVER_HPP */
