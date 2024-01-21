
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

class DiscoverablePeer
{
public:
    
    DiscoverablePeer()
    : mThisPeer(tempName().c_str(), "_elision._tcp.", "", 8001, { bonjour_peer_options::modes::both, true } )
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
    
    void Start()
    {
        DBGMSG("PEER: Started\n");
        
        mActive = true;
        
        // Setup peer discovery
        
        mThisPeer.start();
    }
    
    bool IsRunning() const
    {
        return mActive;
    }
    
    uint16_t Port() const
    {
        return mThisPeer.port();
    }
    
    const char *RegType() const
    {
        return mThisPeer.regtype();
    }
    
    const char *Domain() const
    {
        return mThisPeer.domain();
    }
    
    std::list<bonjour_service>& FindPeers()
    {
        WDL_MutexLock lock(&mMutex);
        
        mThisPeer.list_peers(mPeers);
        
        return mPeers;
    }
    
    std::list<bonjour_service> Peers()
    {
        WDL_MutexLock lock(&mMutex);
        
        return mPeers;
    }
    
    void Resolve(const char* host)
    {
        mThisPeer.resolve(bonjour_named(host, RegType(), Domain()));
    }
    
    void Stop()
    {
        WDL_MutexLock lock(&mMutex);
        
        DBGMSG("PEER: Stopped\n");
        
        mActive = false;
        mThisPeer.stop();
        mPeers.clear();
    }
    
private:
    
    std::list<bonjour_service> mPeers;
    bonjour_peer mThisPeer;
    bool mActive;
    WDL_Mutex mMutex;
};

#endif /* DISCOVERABLEPEER_HPP */
