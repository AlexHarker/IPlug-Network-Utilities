
#ifndef DISCOVERABLEPEER_HPP
#define DISCOVERABLEPEER_HPP

#include <cstring>

#include <unistd.h>

#include "IPlugLogger.h"

#include "../dependencies/bonjour-for-cpp/bonjour-for-cpp.hpp"

std::string tempName()
{
    constexpr int maxLength = 128;
    char host[maxLength];
    
    gethostname(host, maxLength);
    
    std::string name(host);
    size_t pos = -1;
    
    while ((pos = name.find_first_of("._")) != std::string::npos)
        name[pos]= '-';
    
    return name;
}

class DiscoverablePeer : private bonjour_peer
{
public:
    
    DiscoverablePeer()
    : bonjour_peer(tempName().c_str(), "_elision._tcp.", "", 8001, { bonjour_peer_options::modes::both, true } )
    {}
    
    static void GetHostName(WDL_String& name)
    {
        constexpr int maxLength = 128;
        
        char host[maxLength];
        
        gethostname(host, maxLength);
        name.Set(host);
        
        if (!strstr(name.Get(), ".local"))
            name.Append(".local");
        
        name.Append(".");
    }
    
    const char *RegType() const
    {
        return bonjour_peer::regtype();
    }
    
    const char *Domain() const
    {
        return bonjour_peer::domain();
    }
    
    uint16_t Port() const
    {
        return bonjour_peer::port();
    }
    
    void Start()
    {
        DBGMSG("PEER: Started\n");
        
        mActive = true;
        
        // Setup peer discovery
        
        bonjour_peer::start();
    }
    
    void Stop()
    {
        WDL_MutexLock lock(&mMutex);
        
        DBGMSG("PEER: Stopped\n");
        
        mActive = false;
        bonjour_peer::stop();
        mPeers.clear();
    }
    
    bool IsRunning() const
    {
        return mActive;
    }
    
    std::list<bonjour_service>& FindPeers()
    {
        WDL_MutexLock lock(&mMutex);
        
        bonjour_peer::list_peers(mPeers);
        
        return mPeers;
    }
    
    std::list<bonjour_service> Peers() const
    {
        WDL_MutexLock lock(&mMutex);
        
        return mPeers;
    }
    
    void Resolve(const char* host)
    {
        bonjour_peer::resolve(bonjour_named(host, RegType(), Domain()));
    }
    
private:
    
    mutable WDL_Mutex mMutex;
    bool mActive;
    std::list<bonjour_service> mPeers;
};

#endif /* DISCOVERABLEPEER_HPP */
