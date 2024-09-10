
/**
 * @file DiscoverablePeer.hpp
 * @brief Defines the `DiscoverablePeer` class for Bonjour-based peer discovery.
 *
 * This file contains the declaration of the `DiscoverablePeer` class, which facilitates
 * network peer discovery using the Bonjour (ZeroConf) protocol. It includes methods
 * to start, stop, and manage the peer service, as well as to discover other peers on
 * the network.
 *
 * The class uses the `bonjour-for-cpp` library for Bonjour integration and includes
 * thread-safe mechanisms for managing peer state. It provides functionality to
 * resolve services, manage discovered peers, and interact with them.
 *
 * Dependencies:
 * - bonjour-for-cpp.hpp: Provides Bonjour integration.
 * - IPlugLogger.h: Provides logging functionality.
 *
 * @note The class is designed to be thread-safe and uses `WDL_Mutex` for synchronization.
 */

#ifndef DISCOVERABLEPEER_HPP
#define DISCOVERABLEPEER_HPP

#include <cstring>

#include <unistd.h>

#include "IPlugLogger.h"

#include "../dependencies/bonjour-for-cpp/bonjour-for-cpp.hpp"

/**
 * @brief Retrieves and formats the host name of the machine.
 *
 * This function gets the host name of the machine using the `gethostname` function.
 * It then replaces any occurrences of '.' or '_' characters in the host name with '-'
 * and returns the formatted name as a string.
 *
 * @return A string representing the formatted host name, where '.' and '_' are replaced with '-'.
 */

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

/**
 * @class DiscoverablePeer
 * @brief A class representing a discoverable network peer using Bonjour protocol.
 *
 * The `DiscoverablePeer` class encapsulates functionality to register and manage a peer
 * that can be discovered on the network via Bonjour (ZeroConf) services. It provides
 * methods to retrieve host names, start the peer, and manage its lifecycle.
 *
 * The class internally uses the `bonjour-for-cpp` library to register the peer with a
 * specific service type and port.
 */

class DiscoverablePeer
{
public:
    
    /**
     * @brief Constructs a DiscoverablePeer object and initializes the peer service.
     *
     * The constructor initializes the `DiscoverablePeer` object by setting up the peer
     * service with a specific service type (`_elision._tcp.`), an empty domain, and a
     * port (8001). It also retrieves the host name using the `tempName()` function and
     * replaces certain characters ('.', '_') in the host name. The peer is configured
     * to operate in both multicast and unicast modes, as defined by the options.
     *
     * @note The peer is initialized but not yet started. You must explicitly call
     * the `Start()` method to begin the peer discovery process.
     */
    
    DiscoverablePeer()
    : mThisPeer(tempName().c_str(), "_elision._tcp.", "", 8001
                , { bonjour_peer_options::modes::both, true } )
    {}
    
    /**
     * @brief Retrieves the host name of the machine and stores it in the provided string.
     *
     * This static method retrieves the host name of the local machine using the `gethostname`
     * function and stores it in the provided `WDL_String` object. The host name can be used for
     * network identification and peer discovery.
     *
     * @param name A reference to a `WDL_String` object where the host name will be stored.
     *
     * @note The buffer used to retrieve the host name has a maximum length of 128 characters.
     */
    
    static void GetHostName(WDL_String& name)
    {
        constexpr int maxLength = 128;
        
        char host[maxLength];
        
        gethostname(host, maxLength);
        name.Set(host);
    }
    
    /**
     * @brief Starts the peer service, allowing it to be discovered on the network.
     *
     * This method starts the `DiscoverablePeer` service and makes the peer available for
     * discovery by other devices on the network. It logs a message indicating that the peer
     * has started and retrieves the host name to associate with the peer service.
     *
     * @note This method must be called explicitly after the `DiscoverablePeer` object
     * has been created in order to activate the peer service.
     */
    
    void Start()
    {
        DBGMSG("PEER: Started\n");
        
        WDL_String host;
        GetHostName(host);
        
        mActive = true;
        
        // Setup peer discovery
        
        mThisPeer.start();
    }
    
    /**
     * @brief Checks if the peer service is currently running.
     *
     * This method returns a boolean value indicating whether the `DiscoverablePeer` service
     * is actively running and available for discovery on the network.
     *
     * @return `true` if the peer service is running, `false` otherwise.
     *
     * @note This method is marked as `const`, meaning it does not modify the state of the object.
     */
    
    bool IsRunning() const
    {
        return mActive;
    }
    
    /**
     * @brief Discovers and retrieves a list of available peers on the network.
     *
     * This method searches for peers on the network that are advertising their services
     * using the Bonjour protocol. It returns a reference to a list of `bonjour_service`
     * objects, each representing a discovered peer.
     *
     * @return A reference to a `std::list<bonjour_service>` containing the discovered peers.
     *
     * @note The list of peers is updated each time this method is called, so it will
     * reflect the current state of the network discovery process.
     */
    
    std::list<bonjour_service>& FindPeers()
    {
        WDL_MutexLock lock(&mMutex);
        
        mThisPeer.list_peers(mPeers);
        
        return mPeers;
    }
    
    /**
     * @brief Retrieves a copy of the list of discovered peers.
     *
     * This method returns a copy of the list of peers that have been discovered on the network
     * using the Bonjour protocol. Each entry in the list is a `bonjour_service` object representing
     * a peer and its associated service details.
     *
     * @return A `std::list<bonjour_service>` containing the discovered peers.
     *
     * @note This method returns a copy of the internal peer list, so modifications to the returned
     * list will not affect the internal state of the `DiscoverablePeer` object.
     */
    
    std::list<bonjour_service> Peers()
    {
        WDL_MutexLock lock(&mMutex);
        
        return mPeers;
    }
    
    /**
     * @brief Resolves the details of a specified Bonjour service.
     *
     * This method takes a `bonjour_service` object representing a discovered peer
     * and resolves additional details about the service, such as its IP address and
     * port number. This allows for further interaction with the peer beyond just
     * discovery.
     *
     * @param service A constant reference to a `bonjour_service` object representing
     * the service to be resolved.
     *
     * @note Resolving a service may involve network communication and could take time,
     * depending on the network conditions.
     */
    
    void Resolve(const bonjour_service& service)
    {
        mThisPeer.resolve(service);
    }
    
    /**
     * @brief Stops the peer service and removes it from the network.
     *
     * This method stops the `DiscoverablePeer` service, making the peer no longer available
     * for discovery on the network. Once stopped, the peer will no longer be advertised,
     * and any ongoing peer discovery will be halted.
     *
     * @note After calling this method, the peer service can be restarted by calling the
     * `Start()` method again.
     */
    
    void Stop()
    {
        WDL_MutexLock lock(&mMutex);
        
        DBGMSG("PEER: Stopped\n");
        
        mActive = false;
        mThisPeer.stop();
        mPeers.clear();
    }
    
private:
    
    /**
     * @brief A list of discovered Bonjour services representing network peers.
     *
     * This member variable holds a list of `bonjour_service` objects, each representing
     * a peer that has been discovered on the network via the Bonjour protocol. The list
     * is updated as peers are found through the discovery process.
     *
     * @note The list contains only the peers that are currently discoverable on the network.
     * The `FindPeers()` method can be used to update this list.
     */
    
    std::list<bonjour_service> mPeers;
    
    /**
     * @brief Represents the current peer service registered on the network.
     *
     * This member variable holds the `bonjour_peer` object that represents this peer
     * on the network. It is used to register the peer's service, allowing it to be
     * discovered by other devices via the Bonjour protocol. The peer includes details
     * such as the service type, domain, port, and options for network discovery.
     *
     * @note This peer must be started with the `Start()` method to be actively discoverable.
     */
    
    bonjour_peer mThisPeer;
    
    /**
     * @brief Indicates whether the peer service is currently active.
     *
     * This boolean member variable is used to track the running state of the `DiscoverablePeer` service.
     * If `true`, the peer service is active and available for discovery on the network. If `false`,
     * the peer service is stopped and not discoverable.
     *
     * @note This variable is managed by the `Start()` and `Stop()` methods to reflect the current
     * status of the peer service.
     */
    
    bool mActive;
    
    /**
     * @brief A mutex used to synchronize access to shared resources.
     *
     * This member variable is an instance of `WDL_Mutex`, used to ensure thread-safe
     * access to shared resources within the `DiscoverablePeer` class. It helps prevent
     * race conditions when multiple threads attempt to access or modify the same data
     * concurrently.
     *
     * @note The mutex should be locked before accessing shared resources and unlocked
     * after the operation is complete to maintain thread safety.
     */
    
    WDL_Mutex mMutex;
};

#endif /* DISCOVERABLEPEER_HPP */
