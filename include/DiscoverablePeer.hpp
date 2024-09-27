
/**
 * @file DiscoverablePeer.hpp
 * @brief Defines the DiscoverablePeer class and its associated methods for network peer discovery using Bonjour.
 *
 * This file contains the definition of the DiscoverablePeer class, which allows a peer to be discoverable
 * on a local network using Bonjour or similar service discovery protocols. The class includes methods
 * for starting and stopping the discovery process, resolving peer details, and managing a list of
 * discovered peers.
 *
 * The class inherits privately from the bonjour_peer class and uses several helper methods and members
 * to handle network registration, peer discovery, and thread synchronization.
 */

#ifndef DISCOVERABLEPEER_HPP
#define DISCOVERABLEPEER_HPP

#include <cstring>
#include <string>

#include <unistd.h>

#include "IPlugLogger.h"

#include "../dependencies/bonjour-for-cpp/include/bonjour-for-cpp.hpp"

/**
 * @brief Represents a peer that can be discovered over a network using Bonjour.
 *
 * The DiscoverablePeer class is responsible for managing a network peer that can be discovered via
 * Bonjour. It inherits privately from the bonjour_peer class, meaning that the functionality
 * from bonjour_peer is used internally by DiscoverablePeer but is not exposed to external users
 * of this class.
 *
 * The class provides mechanisms for managing the peer's name, registration, and networking details.
 */

class DiscoverablePeer : private bonjour_peer
{
public:
    
    /**
     * @brief Constructs a DiscoverablePeer object with the specified parameters.
     *
     * This constructor initializes a DiscoverablePeer using a provided name,
     * registration name, and port. The name is conformed, and the registration
     * name is concatenated before being passed to the base class constructor.
     *
     * @param name A constant character pointer representing the peer's name. The name is processed by ConformName.
     * @param regname A constant character pointer representing the registration name, processed by RegNameConcat.
     * @param port A 16-bit unsigned integer representing the port number.
     */
    
    DiscoverablePeer(const char* name, const char* regname, uint16_t port)
    : bonjour_peer(ConformName(name).c_str(), RegNameConcat(regname).c_str(), "", port)
    {}
    
    /**
     * @brief Retrieves the static host name of the peer.
     *
     * This method returns the static host name as a `WDL_String`.
     * It is a static function, meaning it can be called without creating an instance of the class.
     *
     * @return A `WDL_String` containing the static host name.
     */
    
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
    
    /**
     * @brief Retrieves the host name of this DiscoverablePeer.
     *
     * This method returns the host name associated with the peer as a `WDL_String`.
     * The method does not modify the object, as indicated by the `const` qualifier.
     *
     * @return A `WDL_String` containing the host name of the peer.
     */
    
    WDL_String GetHostName() const
    {
        WDL_String name(resolved_host().c_str());
        
        return name;
    }
    
    /**
     * @brief Retrieves the registration type of this DiscoverablePeer.
     *
     * This method returns the registration type as a C-style string (`const char*`).
     * The registration type is used in service discovery protocols to identify the type of service
     * provided by the peer. The method does not modify the object, as indicated by the `const` qualifier.
     *
     * @return A C-style string (`const char*`) representing the registration type of the peer.
     */
    
    const char *RegType() const
    {
        return bonjour_peer::regtype();
    }
    
    /**
     * @brief Retrieves the domain of this DiscoverablePeer.
     *
     * This method returns the domain associated with the peer as a C-style string (`const char*`).
     * The domain is typically used in service discovery protocols to define the network scope in which
     * the peer can be discovered. The method does not modify the object, as indicated by the `const` qualifier.
     *
     * @return A C-style string (`const char*`) representing the domain of the peer.
     */
    
    const char *Domain() const
    {
        return bonjour_peer::domain();
    }
    
    /**
     * @brief Retrieves the port number associated with this DiscoverablePeer.
     *
     * This method returns the port number as a 16-bit unsigned integer (`uint16_t`).
     * The port number is used for network communication and defines the specific port
     * on which the peer is accessible. The method does not modify the object, as indicated
     * by the `const` qualifier.
     *
     * @return A 16-bit unsigned integer (`uint16_t`) representing the port number of the peer.
     */
    
    uint16_t Port() const
    {
        return bonjour_peer::port();
    }
    
    /**
     * @brief Starts the DiscoverablePeer service.
     *
     * This method initiates the discovery process, making the peer available on the network.
     * Once called, the peer will be discoverable by other devices or services that use
     * Bonjour or similar network discovery protocols.
     */
    
    void Start()
    {
        DBGMSG("PEER: Started\n");
        
        mActive = true;
        
        // Setup peer discovery
        
        bonjour_peer::start();
    }
    
    /**
     * @brief Stops the DiscoverablePeer service.
     *
     * This method halts the discovery process, making the peer no longer discoverable on the network.
     * It should be called when the peer no longer needs to be available for discovery.
     */
    
    void Stop()
    {
        WDL_MutexLock lock(&mMutex);
        
        DBGMSG("PEER: Stopped\n");
        
        mActive = false;
        bonjour_peer::stop();
        mPeers.clear();
    }
    
    /**
     * @brief Checks if the DiscoverablePeer service is currently running.
     *
     * This method returns a boolean value indicating whether the peer discovery process is active.
     * It allows checking if the peer is currently discoverable on the network. The method does not
     * modify the object, as indicated by the `const` qualifier.
     *
     * @return `true` if the peer discovery process is running, `false` otherwise.
     */
    
    bool IsRunning() const
    {
        return mActive;
    }
    
    /**
     * @brief Finds and returns a list of discovered peers on the network.
     *
     * This method searches for peers that are currently discoverable using the Bonjour service
     * or similar protocols. It returns a reference to a list of `bonjour_service` objects,
     * representing the discovered peers. The list contains details about the services
     * provided by the peers.
     *
     * @return A reference to a `std::list` of `bonjour_service` objects representing the discovered peers.
     */
    
    std::list<bonjour_service>& FindPeers()
    {
        WDL_MutexLock lock(&mMutex);
        
        bonjour_peer::list_peers(mPeers);
        
        return mPeers;
    }
    
    /**
     * @brief Retrieves a list of peers that have been discovered.
     *
     * This method returns a copy of the list of `bonjour_service` objects, each representing a
     * discovered peer on the network. It does not modify the object, as indicated by the `const` qualifier.
     *
     * @return A `std::list` of `bonjour_service` objects representing the discovered peers.
     */
    
    std::list<bonjour_service> Peers() const
    {
        WDL_MutexLock lock(&mMutex);
        
        return mPeers;
    }
    
    /**
     * @brief Resolves the specified host name to obtain more details about the peer.
     *
     * This method takes a host name as input and attempts to resolve it, retrieving
     * additional information about the peer associated with the given host name.
     * This could involve querying the network to gather details such as IP addresses
     * or available services.
     *
     * @param host A constant character pointer representing the host name to resolve.
     */
    
    void Resolve(const char* host)
    {
        bonjour_peer::resolve(bonjour_named(host, RegType(), Domain()));
    }
    
private:
    
    /**
     * @brief Concatenates and processes the given registration name for use in service discovery.
     *
     * This static method takes a registration name as input and processes it, returning a
     * concatenated string that can be used in network registration or service discovery protocols.
     * This method is static, meaning it can be called without an instance of the class.
     *
     * @param regname A constant character pointer representing the registration name to be processed.
     * @return A `std::string` containing the concatenated and processed registration name.
     */
    
    static std::string RegNameConcat(const char* regname)
    {
        return std::string("_") + regname + std::string("._tcp.");
    }
    
    /**
     * @brief Conforms and processes the provided peer name to ensure it follows the required format.
     *
     * This static method takes a peer name as input and processes it to conform to a specified format
     * suitable for use in network discovery or registration. This may include trimming, normalizing,
     * or modifying the name to meet certain criteria. It returns the conformed name as a `std::string`.
     * This method is static, meaning it can be called without an instance of the class.
     *
     * @param name A constant character pointer representing the peer name to be processed.
     * @return A `std::string` containing the conformed and processed name.
     */
    
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
    
    /**
     * @brief A mutable mutex used for thread synchronization.
     *
     * This `WDL_Mutex` object is used to ensure thread-safe access to shared resources within
     * the `DiscoverablePeer` class. The `mutable` keyword allows the mutex to be modified even
     * in `const` methods, ensuring that thread synchronization can occur in methods that would
     * otherwise not allow modification of member variables.
     */
    
    mutable WDL_Mutex mMutex;
    
    /**
     * @brief Indicates whether the DiscoverablePeer service is currently active.
     *
     * This boolean member variable stores the state of the peer service. It is `true`
     * when the peer discovery service is running and the peer is discoverable, and `false`
     * when the service is stopped or inactive.
     */
    
    bool mActive;
    
    /**
     * @brief A list of discovered peers on the network.
     *
     * This member variable holds a list of `bonjour_service` objects, each representing a
     * peer discovered through the Bonjour service or similar protocols. It keeps track of
     * the peers that have been found and provides details about the services they offer.
     */
    
    std::list<bonjour_service> mPeers;
};

#endif /* DISCOVERABLEPEER_HPP */
