
#ifndef DISCOVERABLEPEER_HPP
#define DISCOVERABLEPEER_HPP

#include <cstring>
#include <string>

#include <unistd.h>

#include "IPlugLogger.h"

#include "../dependencies/bonjour-for-cpp/bonjour-for-cpp.hpp"

class DiscoverablePeer : private bonjour_peer
{
public:
    
    DiscoverablePeer(const char* name, const char* regname, uint16_t port)
    : bonjour_peer(ConformName(name).c_str(), RegNameConcat(regname).c_str(), "", port)
    {}
    
    static WDL_String GetStaticHostName()
    {
        constexpr int maxLength = 128;
        
        char host[maxLength];
        
        gethostname(host, maxLength);
        
        WDL_String name(host);
        
        if (!strstr(name.Get(), ".local"))
            name.Append(".local");
        
        name.Append(".");
         
        return name;
    }
    
    WDL_String GetHostName() const
    {
        WDL_String name(resolved_host().c_str());
        
        return name;
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
    
    static std::string RegNameConcat(const char* regname)
    {
        return std::string("_") + regname + std::string("._tcp.");
    }
    
    static std::string ConformName(const char* name)
    {
        std::string conformedName(name);
        size_t pos = -1;
        
        while ((pos = conformedName.find_first_of("._")) != std::string::npos)
            conformedName[pos] = '-';
        
        if (!conformedName.empty() && conformedName.back() == '-')
            conformedName.resize(conformedName.length() - 1);
        
        return conformedName;
    }
    
    mutable WDL_Mutex mMutex;
    bool mActive;
    std::list<bonjour_service> mPeers;
};

#endif /* DISCOVERABLEPEER_HPP */
