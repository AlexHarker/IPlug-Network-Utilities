
#ifndef AUTOSERVER_HPP
#define AUTOSERVER_HPP

#include <wdlstring.h>

#include <utility>

#include "NetworkUtilities.hpp"

class CPUTimer
{
public:
    
    CPUTimer()
    {
        Start();
    }
    
    void Start()
    {
        mStart = std::chrono::steady_clock::now();
    }
    
    double Interval()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - mStart).count();
    }
    
private:
    
    std::chrono::steady_clock::time_point mStart;
};

class IntervalPoll
{
public:
    
    IntervalPoll(double intervalMS)
    : mInterval(intervalMS / 1000.0)
    , mLastTime(mTimer.Interval() - mInterval)
    {}
    
    bool operator()()
    {
        double time = mTimer.Interval();
        
        if (time >= mLastTime + mInterval)
        {
            mLastTime = time;
            return true;
        }
        
        return false;
    }
    
    double Until()
    {
        double time = mTimer.Interval();
        
        return std::max(0.0, ((mLastTime + mInterval) - time) * 1000.0);
    }
    
    void Reset()
    {
        mTimer.Start();
        mLastTime = mTimer.Interval() - mInterval;
    }
    
private:
    
    CPUTimer mTimer;
    double mInterval;
    double mLastTime;
};

class NetworkByteChunk : public iplug::IByteChunk
{
public:

    template <typename ...Args>
    NetworkByteChunk(Args&& ...args)
    {
        Add(std::forward<Args>(args)...);
    }
    
private:
    
    template <typename First, typename ...Args>
    void Add(const First& value, const Args& ...args)
    {
        Add(value);
        Add(std::forward<const Args>(args)...);
    }
    
    void Add(const char* str) { PutStr(str); }
    
    void Add(const iplug::IByteChunk& chunk) { PutChunk(&chunk); }
    
    template <typename T>
    void Add(const T& value) { Put(&value); }
};

class NetworkByteStream
{
public:
    
    NetworkByteStream(const iplug::IByteStream& stream, int startPos = 0)
    : mStream(stream)
    , mPos(startPos)
    {}
    
    inline int Tell() const
    {
        return mPos;
    }
    
    inline void Seek(int pos)
    {
        mPos = pos;
    }
    
    template <class T>
    inline int Get(T& value)
    {
        mPos = mStream.Get(&value, mPos);
        return mPos;
    }
    
    template <class First, class ...Args>
    inline int Get(First& value, Args& ...args)
    {
        mPos = mStream.Get(&value, mPos);
        return Get(args...);
    }
    
    inline int Get(WDL_String& str)
    {
        mPos = mStream.GetStr(str, mPos);
        return mPos;
    }
    
    bool IsNextTag(const char* tag)
    {
        WDL_String nextTag;
        
        int pos = mStream.GetStr(nextTag, mPos);
                
        if (strcmp(nextTag.Get(), tag) == 0)
        {
            mPos = pos;
            return true;
        }
        
        return false;
    }
    
private:
    
    const iplug::IByteStream& mStream;
    int mPos;
};

class AutoServer : public NetworkServer, NetworkClient
{
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
    
    void Discover()
    {
        if (IsClientConnected())
            return;
        
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
    
    void SendFromServer(ws_connection_id id, const iplug::IByteChunk& chunk)
    {
        SendTaggedFromServer(GetDataTag(), id, chunk);
    }
    
    void SendFromServer(const iplug::IByteChunk& chunk)
    {
        SendTaggedFromServer(GetDataTag(), chunk);
    }
    
    void SendFromClient(const iplug::IByteChunk& chunk)
    {
        SendTaggedFromClient(GetDataTag(), chunk);
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
    
    void SendConnectionDataFromServer(ws_connection_id id, const iplug::IByteChunk& chunk)
    {
        SendTaggedFromServer(GetConnectionTag(), id, chunk);
    }
    
    void SendConnectionDataFromServer(const iplug::IByteChunk& chunk)
    {
        SendTaggedFromServer(GetConnectionTag(), chunk);
    }
    
    void SendConnectionDataFromClient(const iplug::IByteChunk& chunk)
    {
        SendTaggedFromClient(GetConnectionTag(), chunk);
    }

    void SendTaggedFromServer(const char *tag, ws_connection_id id, const iplug::IByteChunk& chunk)
    {
        SendDataFromServer(id, NetworkByteChunk(tag, chunk));
    }
    
    void SendTaggedFromServer(const char *tag, const iplug::IByteChunk& chunk)
    {
        SendDataFromServer(NetworkByteChunk(tag, chunk));
    }
    
    void SendTaggedFromClient(const char *tag, const iplug::IByteChunk& chunk)
    {
        SendDataFromClient(NetworkByteChunk(tag, chunk));
    }
    
    bool TryConnect(const char *host, uint32_t port)
    {
        if (Connect(host, port))
        {
            SendConnectionDataFromServer(NetworkByteChunk("SwitchServer", host, port));
            
            WaitToStop();
            mDiscoverable.Stop();
            StopServer();
            
            return true;
        }
        
        return false;
    }
    
    void HandleConnectionDataToServer(NetworkByteStream& stream) {}
    
    void HandleConnectionDataToClient(NetworkByteStream& stream)
    {
        if (stream.IsNextTag("SwitchServer"))
        {
            WDL_String host;
            uint32_t port;
            
            stream.Get(host, port);
            mNextServer.Set(host.Get(), port);
        }
    }
    
    void OnDataToServer(ConnectionID id, const iplug::IByteStream& data) final
    {
        NetworkByteStream stream(data);
                
        if (stream.IsNextTag(GetConnectionTag()))
        {
            HandleConnectionDataToServer(stream);
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

    NextServer mNextServer;
    CPUTimer mBonjourRestart;
    DiscoverablePeer mDiscoverable;
};

#endif /* AUTOSERVER_HPP */
