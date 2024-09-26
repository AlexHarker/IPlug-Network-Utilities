
/**
 * @file NetworkPeer.hpp
 * @brief Defines the NetworkPeer class and related functionality for managing peer-to-peer network communication.
 *
 * This file contains the declarations for the `NetworkPeer` class, along with supporting classes and methods
 * used to manage peer discovery, client-server communication, and connection handling in a peer-to-peer
 * network environment. It includes functionality for handling peers, clients, and servers, as well as sending
 * and receiving data across the network.
 *
 * Classes included:
 * - NetworkPeer
 * - Peer
 * - PeerList
 * - ClientList
 * - NextServer
 *
 * Key features:
 * - Peer discovery and management
 * - Handling client and server connections
 * - Sending and receiving data between peers
 * - Managing connection states and peer timeouts
 */

#ifndef NETWORKPEER_HPP
#define NETWORKPEER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <list>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DiscoverablePeer.hpp"
#include "NetworkClient.hpp"
#include "NetworkData.hpp"
#include "NetworkServer.hpp"
#include "NetworkTiming.hpp"

/**
 * @brief Represents a network peer that can function as both a server and a client.
 *
 * This class is responsible for managing network communication by acting as a peer. It inherits functionality
 * from both NetworkServer and NetworkClient. The inheritance from NetworkServer is private, meaning the peer
 * does not expose server functionalities directly but may use them internally. Public functionalities of the
 * NetworkClient are available.
 */

class NetworkPeer : private NetworkServer, NetworkClient
{
public:
    
    /**
     * @brief Defines the source type of a network peer.
     *
     * This enumeration represents the various types of sources a network peer can originate from,
     * providing a way to classify and differentiate between peer types in the network.
     */
    
    enum class PeerSource {
        Unresolved, /**< The peer's source is not yet determined or unresolved. */
        Discovered, /**< The peer has been discovered through network scanning or discovery mechanisms. */
        Client,     /**< The peer is identified as a client in the network. */
        Server,     /**< The peer is identified as a server in the network. */
        Remote      /**< The peer is a remote entity, typically part of an external network. */
    };
    
    /**
     * @brief Defines an alias for the ConnectionID type.
     *
     * This type alias simplifies access to the ConnectionID type defined in NetworkTypes.
     * It is used to uniquely identify a connection within the network.
     */
    
    using ConnectionID = NetworkTypes::ConnectionID;

private:
    
    /**
     * @brief Represents the various states of a network client during its lifecycle.
     *
     * This enumeration defines the possible states a network client can transition through
     * as it attempts to connect and interact with the network.
     */
    
    enum class ClientState {
        Unconfirmed,    /**< The client has initiated contact but has not yet been confirmed by the server. */
        Confirmed,      /**< The client has been confirmed by the server but is not fully connected. */
        Failed,         /**< The client has failed to establish or maintain a connection. */
        Connected       /**< The client has successfully connected to the server. */
    };
    
    // A host (a hostname and port)
    
    /**
     * @brief Represents a host in the network.
     *
     * The Host class is responsible for managing network-related operations and behavior
     * associated with a network host. This may include tasks such as hosting connections,
     * managing peers, and facilitating communication between clients and servers.
     */
    
    class Host
    {
    public:
        
        /**
         * @brief Constructs a Host object with a specified name and port.
         *
         * This constructor initializes a Host object using the provided name and port number.
         *
         * @param name The name of the host, typically a string representing the host's identifier.
         * @param port The network port number associated with the host, represented as a 16-bit unsigned integer.
         */
        
        Host(const char* name, uint16_t port)
        : mName(name)
        , mPort(port)
        {}
        
        /**
         * @brief Constructs a Host object with a specified WDL_String name and port.
         *
         * This constructor initializes a Host object using the provided name, represented as a WDL_String,
         * and the specified network port number.
         *
         * @param name The name of the host, represented as a WDL_String, typically used as an identifier for the host.
         * @param port The network port number associated with the host, represented as a 16-bit unsigned integer.
         */
        
        Host(const WDL_String& name, uint16_t port)
        : mName(name)
        , mPort(port)
        {}
        
        /**
         * @brief Default constructor for the Host class.
         *
         * Initializes a Host object with an empty name and a port number of 0. This constructor
         * delegates to the parameterized constructor, effectively creating a Host with default values.
         */
        
        Host() : Host("", 0)
        {}
        
        /**
         * @brief Updates the network port for the peer.
         *
         * This function changes the port number used for network communication by the peer.
         *
         * @param port The new port number to be set. Must be a valid 16-bit unsigned integer.
         */
        
        void UpdatePort(uint16_t port)
        {
            mPort = port;
        }
        
        /**
         * @brief Checks if the host name is empty.
         *
         * This method returns true if the host's name has no characters, indicating that the host
         * has not been assigned a name. It checks the length of the mName string.
         *
         * @return true if the host name is empty, false otherwise.
         */
        
        bool Empty() const { return !mName.GetLength(); }
        
        /**
         * @brief Retrieves the host's name as a C-style string.
         *
         * This method returns the name of the host, stored in the mName member, as a C-style
         * null-terminated string.
         *
         * @return A pointer to the host's name as a const char*.
         */
        
        const char *Name() const { return mName.Get(); }
        
        /**
         * @brief Retrieves the network port number associated with the host.
         *
         * This method returns the port number that the host is using for network communication.
         *
         * @return The host's port number as a 16-bit unsigned integer.
         */
        
        uint16_t Port() const { return mPort; }
        
    private:
        
        /**
         * @brief The name of the host.
         *
         * This member variable holds the name of the host as a WDL_String, typically used
         * as an identifier for the host in the network.
         */
        
        WDL_String mName;
        
        /**
         * @brief The network port number associated with the host.
         *
         * This member variable stores the port number, represented as a 16-bit unsigned integer,
         * which is used for network communication with the host.
         */
        
        uint16_t mPort;
    };
    
    // A list of peers with timeout information
    
    /**
     * @brief Manages a list of network peers.
     *
     * The PeerList class is responsible for storing, organizing, and managing the collection
     * of peers in a network. It provides functionalities to add, remove, and query peers within the list.
     */
    
    class PeerList
    {
    public:

        // An individual peer with associated information
        
        /**
         * @brief Represents an individual peer in the network.
         *
         * The Peer class encapsulates the data and functionality related to a single network peer.
         * This may include details like the peer's identity, connection state, and communication methods.
         */
        
        class Peer
        {
        public:
             
            /**
             * @brief Constructs a Peer object with the specified name, port, source, and optional time.
             *
             * This constructor initializes a Peer object by assigning a host name and port, as well as a peer source and an optional time value.
             *
             * @param name The name of the peer's host, typically used to identify the peer.
             * @param port The network port number the peer is using for communication, represented as a 16-bit unsigned integer.
             * @param source The source type of the peer, represented by the PeerSource enum, indicating how the peer was discovered or its role.
             * @param time (optional) The time associated with the peer, represented as a 32-bit unsigned integer. Defaults to 0 if not provided.
             */
            
            Peer(const char* name, uint16_t port, PeerSource source, uint32_t time = 0)
            : mHost { name, port }
            , mSource(source)
            , mTime(time)
            {}
            
            /**
             * @brief Constructs a Peer object with the specified WDL_String name, port, source, and optional time.
             *
             * This constructor initializes a Peer object by converting the WDL_String name to a C-style string
             * and delegating to the other Peer constructor. It also assigns a network port, peer source, and optional time value.
             *
             * @param name The name of the peer's host, represented as a WDL_String.
             * @param port The network port number the peer is using for communication, represented as a 16-bit unsigned integer.
             * @param source The source type of the peer, represented by the PeerSource enum, indicating how the peer was discovered or its role.
             * @param time (optional) The time associated with the peer, represented as a 32-bit unsigned integer. Defaults to 0 if not provided.
             */
            
            Peer(const WDL_String& name, uint16_t port, PeerSource source, uint32_t time = 0)
            : Peer(name.Get(), port, source, time)
            {}
            
            /**
             * @brief Default constructor for the Peer class.
             *
             * Initializes a Peer object with default values. The name is set to nullptr, the port is set to 0,
             * and the source is set to PeerSource::Unresolved. This constructor delegates to the parameterized constructor.
             */
            
            Peer() : Peer(nullptr, 0, PeerSource::Unresolved)
            {}
            
            /**
             * @brief Updates the port number for the peer.
             *
             * This method allows changing the network port number that the peer uses for communication.
             *
             * @param port The new port number to be set for the peer, represented as a 16-bit unsigned integer.
             */
            
            void UpdatePort(uint16_t port)
            {
                mHost.UpdatePort(port);
            }
            
            /**
             * @brief Updates the source type of the peer.
             *
             * This method changes the source of the peer, which indicates how the peer was discovered or its role in the network.
             *
             * @param source The new source type for the peer, represented by the PeerSource enum.
             */
            
            void UpdateSource(PeerSource source)
            {
                mSource = source;
            }
            
            /**
             * @brief Updates the time associated with the peer.
             *
             * This method sets or updates the time value related to the peer, which can represent
             * the time of connection, last communication, or any other time-related data.
             *
             * @param time The new time value to be set for the peer, represented as a 32-bit unsigned integer.
             */
            
            void UpdateTime(uint32_t time)
            {
                mTime = std::min(mTime, time);
            }
            
            /**
             * @brief Increments the time associated with the peer by a specified amount.
             *
             * This method adds a specified value to the current time associated with the peer.
             * It can be used to extend or adjust the time value, such as for tracking time elapsed.
             *
             * @param add The amount of time to be added, represented as a 32-bit unsigned integer.
             */
            
            void AddTime(uint32_t add)
            {
                mTime += add;
            }
            
            /**
             * @brief Retrieves the name of the peer's host as a C-style string.
             *
             * This method returns the name of the peer's host by calling the `Name()` method of the `mHost` object.
             * The name is returned as a null-terminated C-style string.
             *
             * @return A pointer to the peer's host name as a const char*.
             */
            
            const char *Name() const { return mHost.Name(); }
            
            /**
             * @brief Retrieves the network port number of the peer's host.
             *
             * This method returns the port number used by the peer's host by calling the `Port()` method of the `mHost` object.
             *
             * @return The port number of the peer's host as a 16-bit unsigned integer.
             */
            
            uint16_t Port() const { return mHost.Port(); }
            
            /**
             * @brief Retrieves the source type of the peer.
             *
             * This method returns the source of the peer, indicating how the peer was discovered
             * or its role in the network. The source is represented by the `PeerSource` enum.
             *
             * @return The source type of the peer as a `PeerSource` enum value.
             */
            
            PeerSource Source() const { return mSource; }
            
            /**
             * @brief Retrieves the time associated with the peer.
             *
             * This method returns the time value associated with the peer. The time could represent
             * the time of connection, the last communication, or any other time-related information.
             *
             * @return The time associated with the peer as a 32-bit unsigned integer.
             */
            
            uint32_t Time() const { return mTime; }
            
            /**
             * @brief Checks if the peer is a client.
             *
             * This method determines whether the peer's source type is set to `PeerSource::Client`.
             * It returns true if the peer is identified as a client, otherwise false.
             *
             * @return true if the peer is a client, false otherwise.
             */
            
            bool IsClient() const { return mSource == PeerSource::Client; }
            
            /**
             * @brief Checks if the peer's source is unresolved.
             *
             * This method determines whether the peer's source type is set to `PeerSource::Unresolved`.
             * It returns true if the peer's source is unresolved, indicating that the peer's origin or role
             * in the network has not yet been determined.
             *
             * @return true if the peer's source is unresolved, false otherwise.
             */
            
            bool IsUnresolved() const { return mSource == PeerSource::Unresolved; }
            
        private:
            
            /**
             * @brief Represents the host information associated with the peer.
             *
             * This member variable stores the `Host` object, which contains details about the peer's host,
             * such as the host's name and network port.
             */
            
            Host mHost;
            
            /**
             * @brief Represents the source type of the peer.
             *
             * This member variable stores the source of the peer, which is used to determine
             * how the peer was discovered or its role in the network. It is of type `PeerSource`.
             */
            
            PeerSource mSource;
            
            /**
             * @brief Represents the time associated with the peer.
             *
             * This member variable stores a 32-bit unsigned integer that represents the time value
             * related to the peer. This could be the time of connection, last communication, or any
             * other relevant time information.
             */
            
            uint32_t mTime;
        };
          
        /**
         * @brief Defines a type alias for a list of Peer objects.
         *
         * This type alias represents a list container (`std::list`) that holds `Peer` objects.
         * It simplifies the declaration and use of a list of peers in the code.
         */
        
        using ListType = std::list<Peer>;

        /**
         * @brief Adds a peer to the list of peers.
         *
         * This method takes a reference to a `Peer` object and adds it to the internal list of peers.
         *
         * @param peer A constant reference to the `Peer` object to be added to the list.
         */
        
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
        
        /**
         * @brief Removes peers that have exceeded the specified time limit.
         *
         * This method iterates through the list of peers and removes any peer whose time value exceeds the specified `maxTime`.
         * An optional `addTime` can be used to adjust the time threshold before pruning.
         *
         * @param maxTime The maximum allowed time value for a peer. Peers with a time value greater than this will be removed.
         * @param addTime (optional) An additional time value to be added to the peer's time before comparing it to `maxTime`. Defaults to 0.
         */
        
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
        
        /**
         * @brief Retrieves the current list of peers.
         *
         * This method copies the current list of peers into the provided `list` reference.
         * The list contains all the peers managed by the object.
         *
         * @param list A reference to a `ListType` object where the current list of peers will be copied.
         */
        
        void Get(ListType& list) const
        {
            RecursiveLock lock(&mMutex);

            list = mPeers;
        }
        
        /**
         * @brief Returns the number of peers in the list.
         *
         * This method provides the current size of the peer list, indicating how many peers are being managed.
         *
         * @return The number of peers in the list as an integer.
         */
        
        int Size() const
        {
            RecursiveLock lock(&mMutex);

            return static_cast<int>(mPeers.size());
        }
        
    private:
        
        /**
         * @brief A mutable recursive mutex used for thread-safe access to the peer list.
         *
         * This `RecursiveMutex` allows for locking and unlocking in a recursive manner, ensuring thread-safe
         * operations on the peer list or other data. The `mutable` keyword allows the mutex to be modified
         * even in const-qualified methods.
         */
        
        mutable RecursiveMutex mMutex;
        
        /**
         * @brief A list that holds the collection of peers.
         *
         * This member variable stores a list of `Peer` objects, representing all the peers managed by the class.
         * The list is of type `ListType`, which is a type alias for `std::list<Peer>`.
         */
        
        ListType mPeers;
    };
    
    // A list of fully confirmed clients
    
    /**
     * @brief Manages a list of client connection IDs.
     *
     * The `ClientList` class is responsible for managing a set of client connections, using `ConnectionID` as the identifier.
     * It privately inherits from `std::unordered_set<ConnectionID>`, meaning that it provides its own interface
     * for managing connection IDs while internally using an unordered set for storage.
     */
    
    class ClientList : private std::unordered_set<ConnectionID>
    {
    public:
        
        /**
         * @brief Adds a client connection ID to the list.
         *
         * This method inserts a new `ConnectionID` into the client list, ensuring that the client connection
         * is tracked and managed within the internal unordered set.
         *
         * @param id The `ConnectionID` of the client to be added to the list.
         */
        
        void Add(ConnectionID id)
        {
            RecursiveLock lock(&mMutex);
            
            insert(id);
        }
        
        /**
         * @brief Removes a client connection ID from the list.
         *
         * This method removes the specified `ConnectionID` from the client list,
         * ensuring that the client connection is no longer tracked.
         *
         * @param id The `ConnectionID` of the client to be removed from the list.
         */
        
        void Remove(ConnectionID id)
        {
            RecursiveLock lock(&mMutex);
            
            erase(id);
        }
        
        /**
         * @brief Removes all client connection IDs from the list.
         *
         * This method clears the entire client list, removing all `ConnectionID` entries and effectively resetting the list.
         */
        
        void Clear()
        {
            RecursiveLock lock(&mMutex);
            
            clear();
        }
        
        /**
         * @brief Returns the number of client connection IDs in the list.
         *
         * This method provides the current size of the client list, indicating how many `ConnectionID` entries
         * are being tracked.
         *
         * @return The number of client connection IDs in the list as an integer.
         */
        
        int Size() const
        {
            RecursiveLock lock(&mMutex);
            
            return static_cast<int>(size());
        }
        
    private:
        
        /**
         * @brief A mutable recursive mutex for ensuring thread-safe access to the client list.
         *
         * This `RecursiveMutex` is used to provide thread safety when accessing or modifying the client list.
         * The `mutable` keyword allows the mutex to be locked or unlocked even in const-qualified methods,
         * enabling safe concurrent access to the client list.
         */
        
        mutable RecursiveMutex mMutex;
    };
    
    // A class for storing info about the next server a peer should connect to
    
    /**
     * @brief Represents the next server in a network communication sequence.
     *
     * The `NextServer` class is responsible for managing and handling the functionality
     * related to the next server in a communication process, possibly involving tasks
     * like server discovery, connection handling, and data transfer.
     */
    
    class NextServer
    {
    public:
        
        /**
         * @brief Sets the server information using the specified host.
         *
         * This method updates the `NextServer` object with the provided `Host` object,
         * which contains information such as the server's name and port.
         *
         * @param host A reference to a `Host` object containing the new server information.
         */
        
        void Set(const Host& host)
        {
            RecursiveLock lock(&mMutex);

            mHost = host;
            mTimeOut.Start();
        }
        
        /**
         * @brief Retrieves the current host information of the server.
         *
         * This method returns the `Host` object that contains the current server's details,
         * such as its name and port number.
         *
         * @return A `Host` object representing the server's current host information.
         */
        
        Host Get() const
        {
            RecursiveLock lock(&mMutex);

            if (mTimeOut.Interval() > 4)
                return Host();
            else
                return mHost;
        }
        
    private:
        
        /**
         * @brief A mutable recursive mutex used for thread-safe access to the server information.
         *
         * This `RecursiveMutex` ensures that access to the server's host information is thread-safe.
         * The `mutable` keyword allows the mutex to be locked and unlocked even in const-qualified methods,
         * facilitating concurrent access without compromising safety.
         */
        
        mutable RecursiveMutex mMutex;
        
        /**
         * @brief Stores the host information for the server.
         *
         * This member variable holds a `Host` object that contains the server's name and network port,
         * representing the current server details in the network.
         */
        
        Host mHost;
        
        /**
         * @brief A CPU timer used to track timeouts for server operations.
         *
         * This member variable holds a `CPUTimer` object, which is used to manage and monitor
         * timeout events related to the server. It helps in controlling operations that need
         * to complete within a certain time frame.
         */
        
        CPUTimer mTimeOut;
    };
    
public:
            
    // Peer information structure
    
    /**
     * @brief Holds information about a network peer.
     *
     * The `PeerInfo` struct is used to store and organize data related to a network peer,
     * such as its identity, connection details, or other relevant information. This structure
     * provides a convenient way to bundle peer-related data together.
     */
    
    struct PeerInfo
    {
        
        /**
         * @brief Constructs a PeerInfo object with the specified name, port, source, and time.
         *
         * This constructor initializes a PeerInfo object by setting the peer's name, port number, source type,
         * and associated time.
         *
         * @param name The name of the peer, typically used as an identifier.
         * @param port The network port number the peer is using, represented as a 16-bit unsigned integer.
         * @param source The source type of the peer, represented by the `PeerSource` enum, indicating how the peer was discovered or its role.
         * @param time The time associated with the peer, represented as a 32-bit unsigned integer. This could indicate the time of connection or other relevant timing information.
         */
        
        PeerInfo(const char* name, uint16_t port, PeerSource source, uint32_t time)
        : mName(name)
        , mPort(port)
        , mSource(source)
        , mTime(time)
        {}
        
        /**
         * @brief Stores the name of the peer.
         *
         * This member variable holds the name of the peer as a `WDL_String`, typically used to identify the peer
         * within the network.
         */
        
        WDL_String mName;
        
        /**
         * @brief Stores the network port number used by the peer.
         *
         * This member variable holds the port number associated with the peer's network communication,
         * represented as a 16-bit unsigned integer.
         */
        
        uint16_t mPort;
        
        /**
         * @brief Stores the source type of the peer.
         *
         * This member variable holds the source type of the peer, which is represented by the `PeerSource` enum.
         * It indicates how the peer was discovered or its role within the network (e.g., client, server, etc.).
         */
        
        PeerSource mSource;
        
        /**
         * @brief Stores the time associated with the peer.
         *
         * This member variable holds a 32-bit unsigned integer representing the time associated with the peer.
         * This value could indicate the time of connection, last communication, or any other relevant timing information.
         */
        
        uint32_t mTime;
    };
    
    /**
     * @brief Constructs a `NetworkPeer` object with a specified registration name and optional port.
     *
     * This constructor initializes a `NetworkPeer` object by setting the client state to `Unconfirmed`
     * and creating a `DiscoverablePeer` object with the host's static name, the given registration name, and port.
     *
     * @param regname The registration name of the peer, typically used as an identifier in the network.
     * @param port (optional) The network port number used by the peer, represented as a 16-bit unsigned integer.
     *        Defaults to 8001 if not provided.
     */
    
    NetworkPeer(const char *regname, uint16_t port = 8001)
    : mClientState(ClientState::Unconfirmed)
    , mDiscoverable(DiscoverablePeer::GetStaticHostName().Get(), regname, port)
    {}
    
    /**
     * @brief Destructor for the `NetworkPeer` class.
     *
     * Cleans up resources and performs any necessary shutdown tasks when the `NetworkPeer` object is destroyed.
     * This ensures proper resource management and network disconnection (if applicable) upon the object's destruction.
     */
    
    ~NetworkPeer()
    {
        mDiscoverable.Stop();
        StopServer();
    }
    
    /**
     * @brief Retrieves the host name of the peer.
     *
     * This method returns the host name associated with the `NetworkPeer` object as a `WDL_String`.
     *
     * @return The host name of the peer as a `WDL_String`.
     */
    
    WDL_String GetHostName() const
    {
        return mDiscoverable.GetHostName();
    }
    
    // Peer status (these do not correspond directly to the state of NstworkServer and NetworkClient
    
    /**
     * @brief Checks if the peer is currently connected as a server.
     *
     * This method returns true if the peer has confirmed clients, indicating that it is connected as a server.
     * The size of `mConfirmedClients` is used to determine if there are any confirmed clients.
     *
     * @return true if the peer is connected as a server (i.e., has confirmed clients), false otherwise.
     */
    
    bool IsConnectedAsServer() const { return mConfirmedClients.Size(); }
    
    /**
     * @brief Checks if the peer is currently connected as a client.
     *
     * This method returns true if the peer is connected as a client, which is determined by two conditions:
     * the client connection must be established (`IsClientConnected()` returns true), and the client's state
     * must be `ClientState::Connected`.
     *
     * @return true if the peer is connected as a client, false otherwise.
     */
    
    bool IsConnectedAsClient() const  { return IsClientConnected() && mClientState == ClientState::Connected; }
    
    /**
     * @brief Checks if the peer is disconnected from the network.
     *
     * This method returns true if the peer is neither connected as a server nor as a client.
     * It checks both `IsConnectedAsServer()` and `IsConnectedAsClient()` methods, and if both return false,
     * the peer is considered disconnected.
     *
     * @return true if the peer is disconnected, false otherwise.
     */
    
    bool IsDisconnected() const { return !IsConnectedAsServer() && !IsConnectedAsClient(); }
    
    /**
     * @brief Initiates the peer discovery process at regular intervals.
     *
     * This method starts the discovery process to find new peers in the network. The discovery process
     * runs at the specified `interval` and peers are kept in the list for a maximum duration defined by `maxPeerTime`.
     *
     * @param interval The time interval (in milliseconds) between discovery attempts.
     * @param maxPeerTime The maximum amount of time (in milliseconds) that a peer remains in the list before being considered inactive or removed.
     */
    
    void Discover(uint32_t interval, uint32_t maxPeerTime)
    {
        if (IsClientConnected())
        {
            if (mClientState != ClientState::Failed)
            {
                if (mClientState == ClientState::Confirmed)
                    ClientConnectionConfirmed();
                
                mPeers.Add({NetworkClient::GetServerName().Get(), Port(), PeerSource::Server});
                mPeers.Prune(maxPeerTime, interval);
                return;
            }
            else
                Disconnect();
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
    
    /**
     * @brief Retrieves the name of the server the peer is connected to or hosting.
     *
     * This method returns the server's name as a `WDL_String`. It can represent the name of the server
     * the peer is connected to (as a client) or the name of the server being hosted (as a server).
     *
     * @return The server's name as a `WDL_String`.
     */
    
    WDL_String GetServerName() const
    {
        WDL_String str;
        
        if (IsServerConnected())
        {
            const int confirmed = mConfirmedClients.Size();
            const int clients = NClients();
            str = GetHostName();
            
            if (confirmed != clients)
                str.AppendFormatted(256, " [%d/%d]", confirmed, clients);
            else
                str.AppendFormatted(256, " [%d]", clients);

            if (IsClientConnected())
                str.AppendFormatted(256, " [%s]", NetworkClient::GetServerName().Get());
        }
        else if (IsClientConnected())
            str = NetworkClient::GetServerName();
        else
            str.Set("Disconnected");
        
        return str;
    }
    
    /**
     * @brief Retrieves a list of information about the connected peers.
     *
     * This method returns a vector of `PeerInfo` objects, each containing details about
     * the connected peers, such as their names, ports, sources, and associated time information.
     *
     * @return A `std::vector` of `PeerInfo` objects representing the connected peers.
     */
    
    std::vector<PeerInfo> GetPeerInfo() const
    {
        std::vector<PeerInfo> info;
        
        PeerList::ListType peers;
        mPeers.Get(peers);
        
        for (auto it = peers.begin(); it != peers.end(); it++)
            info.emplace_back(it->Name(), it->Port(), it->Source(), it->Time());
        
        return info;
    }
    
    /**
     * @brief Sends data to a specified client.
     *
     * This templated method allows sending a message or data to a specific client identified by `id`.
     * It accepts a variable number of arguments of any type, which are forwarded to the client.
     *
     * @tparam Args The types of the arguments to be sent to the client.
     * @param id The connection ID of the client to send data to.
     * @param args The data or message to be sent to the client, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendToClient(ws_connection_id id, const Args& ...args)
    {
        SendTaggedToClient(GetDataTag(), id, std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends data from the server to all connected clients.
     *
     * This templated method allows sending a message or data from the server to all connected clients.
     * It accepts a variable number of arguments of any type, which are broadcast to all clients.
     *
     * @tparam Args The types of the arguments to be sent to the clients.
     * @param args The data or message to be sent to all clients, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendFromServer(const Args& ...args)
    {
        SendTaggedFromServer(GetDataTag(), std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends data from the client to the connected server.
     *
     * This templated method allows sending a message or data from the client to the server.
     * It accepts a variable number of arguments of any type, which are forwarded to the server.
     *
     * @tparam Args The types of the arguments to be sent to the server.
     * @param args The data or message to be sent to the server, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendFromClient(const Args& ...args)
    {
        SendTaggedFromClient(GetDataTag(), std::forward<const Args>(args)...);
    }

private:
    
    /**
     * @brief Compares two peer names and determines the preferred one.
     *
     * This static method compares two peer names and returns true if `name1` is preferred over `name2`.
     * The comparison logic could be based on factors like alphabetical order, length, or other criteria.
     *
     * @param name1 The first peer name to compare, represented as a C-style string.
     * @param name2 The second peer name to compare, represented as a C-style string.
     * @return true if `name1` is preferred over `name2`, false otherwise.
     */
    
    static bool NamePrefer(const char* name1, const char* name2)
    {
        return strcmp(name1, name2) < 0;
    }
    
    /**
     * @brief Checks if the given peer name matches the current peer's name.
     *
     * This method compares the provided `peerName` with the current peer's name to determine
     * if they refer to the same peer. It returns true if the names match.
     *
     * @param peerName The name of the peer to compare, represented as a C-style string.
     * @return true if the provided peer name matches the current peer's name, false otherwise.
     */
    
    bool IsSelf(const char* peerName) const
    {
        WDL_String host = GetHostName();
        
        return !strcmp(host.Get(), peerName);
    }
    
    /**
     * @brief Waits for the network peer to stop its operations.
     *
     * This method blocks execution until the peer has finished all its network operations
     * and is ready to stop. It ensures that any ongoing tasks or communications are completed
     * before the peer stops.
     */
    
    void WaitToStop()
    {
        std::chrono::duration<double, std::milli> ms(500);
        std::this_thread::sleep_for(ms);
    }
    
    /**
     * @brief Retrieves a constant connection tag used to identify connections.
     *
     * This constexpr static method returns a constant C-style string that serves as a tag
     * for identifying connections. The connection tag can be used for logging, debugging,
     * or other identification purposes in the network.
     *
     * @return A constant pointer to a C-style string representing the connection tag.
     */
    
    constexpr static const char *GetConnectionTag()
    {
        return "~";
    }
    
    /**
     * @brief Retrieves a constant data tag used to identify data transmissions.
     *
     * This constexpr static method returns a constant C-style string that serves as a tag
     * for identifying data in transmissions. The data tag can be used for tracking, logging,
     * or categorizing transmitted data in the network.
     *
     * @return A constant pointer to a C-style string representing the data tag.
     */
    
    constexpr static const char *GetDataTag()
    {
        return "-";
    }
    
    /**
     * @brief Sends connection-related data to a specific client.
     *
     * This templated method allows sending connection-specific data to a client identified by `id`.
     * It accepts a variable number of arguments of any type, which are forwarded as the connection data.
     *
     * @tparam Args The types of the arguments representing the connection data to be sent to the client.
     * @param id The connection ID of the client to which the data will be sent.
     * @param args The connection data or message to be sent, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendConnectionDataToClient(ws_connection_id id, const Args& ...args)
    {
        SendTaggedToClient(GetConnectionTag(), id, std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends connection-related data from the server to all connected clients.
     *
     * This templated method allows the server to send connection-specific data to all connected clients.
     * It accepts a variable number of arguments of any type, which are broadcast as the connection data.
     *
     * @tparam Args The types of the arguments representing the connection data to be sent to the clients.
     * @param args The connection data or message to be sent, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendConnectionDataFromServer(const Args& ...args)
    {
        SendTaggedFromServer(GetConnectionTag(), std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends connection-related data from the client to the server.
     *
     * This templated method allows the client to send connection-specific data to the server.
     * It accepts a variable number of arguments of any type, which are forwarded as the connection data.
     *
     * @tparam Args The types of the arguments representing the connection data to be sent to the server.
     * @param args The connection data or message to be sent, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendConnectionDataFromClient(const Args& ...args)
    {
        SendTaggedFromClient(GetConnectionTag(), std::forward<const Args>(args)...);
    }

    /**
     * @brief Sends tagged data to a specific client.
     *
     * This templated method sends data to a client identified by `id`, with an associated tag for identification.
     * The tag is a C-style string that can be used to categorize or label the data being sent.
     * It accepts a variable number of arguments of any type, which are forwarded as the tagged data.
     *
     * @tparam Args The types of the arguments representing the tagged data to be sent to the client.
     * @param tag A constant C-style string used to tag or label the data being sent.
     * @param id The connection ID of the client to which the tagged data will be sent.
     * @param args The data or message to be sent, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendTaggedToClient(const char *tag, ws_connection_id id, const Args& ...args)
    {
        SendDataToClient(id, NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    /**
     * @brief Sends tagged data from the server to all connected clients.
     *
     * This templated method sends data from the server to all connected clients, with an associated tag for identification.
     * The tag is a C-style string that categorizes or labels the data being sent.
     * It accepts a variable number of arguments of any type, which are forwarded as the tagged data.
     *
     * @tparam Args The types of the arguments representing the tagged data to be sent to the clients.
     * @param tag A constant C-style string used to tag or label the data being sent.
     * @param args The data or message to be sent to all connected clients, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendTaggedFromServer(const char *tag, const Args& ...args)
    {
        SendDataFromServer(NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    /**
     * @brief Sends tagged data from the client to the server.
     *
     * This templated method allows the client to send data to the server with an associated tag for identification.
     * The tag is a C-style string that categorizes or labels the data being sent.
     * It accepts a variable number of arguments of any type, which are forwarded as the tagged data.
     *
     * @tparam Args The types of the arguments representing the tagged data to be sent to the server.
     * @param tag A constant C-style string used to tag or label the data being sent.
     * @param args The data or message to be sent to the server, provided as a variadic argument pack.
     */
    
    template <class ...Args>
    void SendTaggedFromClient(const char *tag, const Args& ...args)
    {
        SendDataFromClient(NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    /**
     * @brief Handles the event when a client disconnects from the server.
     *
     * This method is called when a client identified by the given `ConnectionID` disconnects from the server.
     * It overrides a base class method to provide custom behavior for handling disconnections on the server side.
     *
     * @param id The `ConnectionID` of the client that disconnected from the server.
     */
    
    void OnServerDisconnect(ConnectionID id) override
    {
        mConfirmedClients.Remove(id);
    }
    
    /**
     * @brief Handles the event when the client's connection to the server is confirmed.
     *
     * This method is called when the client's connection to the server is successfully established and confirmed.
     * It can be used to trigger any necessary actions or state changes following the confirmation of the connection.
     */
    
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
    
    /**
     * @brief Attempts to establish a connection to a specified host and port.
     *
     * This method tries to connect to a server at the given host address and port. An optional parameter
     * allows the connection to be made directly if set to true.
     *
     * @param host The address of the host to connect to, represented as a C-style string.
     * @param port The network port to connect to, represented as a 16-bit unsigned integer.
     * @param direct (optional) A boolean flag indicating whether to establish a direct connection.
     *        Defaults to false.
     * @return true if the connection attempt is successful, false otherwise.
     */
    
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
    
    /**
     * @brief Sends the list of connected peers to the relevant clients or server.
     *
     * This method transmits the current list of peers in the network, allowing other clients or the server
     * to have up-to-date information about the connected peers.
     */
    
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
    
    /**
     * @brief Sends a ping message to all connected clients.
     *
     * This method is used to send a ping signal to all clients connected to the server.
     * Pinging can be used to check the availability or responsiveness of clients and maintain an active connection.
     */
    
    void PingClients()
    {
        SendConnectionDataFromServer("Ping");
    }
    
    /**
     * @brief Sets the next server to connect to, along with its port.
     *
     * This method specifies the address and port of the next server that the peer should connect to.
     * It updates the server information, preparing the peer for the next connection attempt.
     *
     * @param server The address of the next server, represented as a C-style string.
     * @param port The network port of the next server, represented as a 16-bit unsigned integer.
     */
    
    void SetNextServer(const char* server, uint16_t port)
    {
        // Prevent self connection
        
        if (!IsSelf(server))
            mNextServer.Set(Host(server, port));
    }
    
    /**
     * @brief Handles incoming connection data sent to the server from a client.
     *
     * This method processes connection-related data received from a client identified by `ConnectionID`.
     * The data is provided via a `NetworkByteStream`, which contains the information sent by the client.
     *
     * @param id The `ConnectionID` of the client that sent the data.
     * @param stream A reference to the `NetworkByteStream` that contains the data to be processed.
     */
    
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
    
    /**
     * @brief Handles incoming connection data sent to the client from the server.
     *
     * This method processes connection-related data received from the server.
     * The data is provided via a `NetworkByteStream`, which contains the information sent by the server.
     *
     * @param stream A reference to the `NetworkByteStream` that contains the data to be processed.
     */
    
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
            
            mClientState = confirm ? ClientState::Confirmed : ClientState::Failed;
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
    
    /**
     * @brief Handles data sent from a client to the server.
     *
     * This method is triggered when data is received from a client, identified by the `ConnectionID`.
     * The data is provided as an `iplug::IByteStream`, which contains the information sent by the client.
     * This method is marked as `final`, meaning it cannot be overridden in derived classes.
     *
     * @param id The `ConnectionID` of the client that sent the data.
     * @param data A reference to the `iplug::IByteStream` containing the data from the client.
     */
    
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
            DBGMSG("Unknown network message to client\n");
        }
    }
    
    /**
     * @brief Handles data sent from the server to the client.
     *
     * This method is triggered when data is received by the client from the server.
     * The data is provided as an `iplug::IByteStream`, which contains the information sent by the server.
     * This method is marked as `final`, meaning it cannot be overridden in derived classes.
     *
     * @param data A reference to the `iplug::IByteStream` containing the data from the server.
     */
    
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
            DBGMSG("Unknown network message to client\n");
        }
    }
    
    /**
     * @brief Receives and processes data sent from a client to the server.
     *
     * This virtual method is intended to be overridden by derived classes to handle data received from a client.
     * It is called when a client sends data to the server, with the client's `ConnectionID` and the data provided
     * in a `NetworkByteStream`.
     *
     * @param id The `ConnectionID` of the client that sent the data.
     * @param data A reference to the `NetworkByteStream` containing the data sent by the client.
     */
    
    virtual void ReceiveAsServer(ConnectionID id, NetworkByteStream& data) {}
    
    /**
     * @brief Receives and processes data sent from the server to the client.
     *
     * This virtual method is intended to be overridden by derived classes to handle data received from the server.
     * It is called when the server sends data to the client, with the data provided in a `NetworkByteStream`.
     *
     * @param data A reference to the `NetworkByteStream` containing the data sent by the server.
     */
    
    virtual void ReceiveAsClient(NetworkByteStream& data) {}

    // Tracking the client connection process
    
    /**
     * @brief Holds the current state of the client in a thread-safe manner.
     *
     * This member variable stores the client's state using an `std::atomic<ClientState>`, ensuring thread-safe
     * access and modification of the client's state. The `ClientState` enum represents various states, such as
     * unconfirmed, confirmed, or connected.
     */
    
    std::atomic<ClientState> mClientState;
    
    // Info about other peers
    
    /**
     * @brief Manages the list of clients that have been confirmed by the server.
     *
     * This member variable stores a `ClientList` of clients whose connections have been successfully confirmed.
     * The list tracks all clients that are actively connected to the server and have completed the confirmation process.
     */
    
    ClientList mConfirmedClients;
    
    /**
     * @brief Manages the list of known network peers.
     *
     * This member variable stores a `PeerList` containing information about all known peers in the network.
     * It is used to track and manage peers, whether they are clients or other servers, and maintain their connection details.
     */
    
    PeerList mPeers;
    
    /**
     * @brief Stores information about the next server to connect to.
     *
     * This member variable holds a `NextServer` object, which contains the details of the next server
     * that the client or peer is expected to connect to. It manages the server's address, port, and other
     * relevant connection details.
     */
    
    NextServer mNextServer;

    // Bonjour
    
    /**
     * @brief A CPU timer used to manage the restart interval for the Bonjour service.
     *
     * This member variable holds a `CPUTimer` object, which is responsible for timing the restart
     * of the Bonjour service. It ensures that the service is restarted at the appropriate intervals
     * to maintain proper network discovery functionality.
     */
    
    CPUTimer mBonjourRestart;
    
    /**
     * @brief Manages the peer's discoverability in the network.
     *
     * This member variable holds a `DiscoverablePeer` object, which handles the functionality that allows
     * the peer to be discovered by other devices in the network. It manages broadcasting and responding to
     * discovery requests, making the peer visible to others.
     */
    
    DiscoverablePeer mDiscoverable;
};

#endif /* NETWORKPEER_HPP */
