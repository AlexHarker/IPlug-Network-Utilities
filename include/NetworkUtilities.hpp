
/**
 * @file NetworkUtilities.hpp
 * @brief Contains utility classes and interfaces for managing network communication.
 *
 * This file provides the definitions for network-related structures, classes, and interfaces
 * used for managing WebSocket-based server and client connections. It includes both server and client
 * interfaces, as well as various helper classes and types for handling network operations such as
 * connection management, data transmission, and synchronization.
 *
 * The file defines several key components:
 * - `NetworkTypes`: A structure that contains shared types and utilities for network management.
 * - `VariableLock`: A helper class for managing mutex locks with both shared and exclusive modes.
 * - `NetworkServerInterface`: A templated class for managing server-side network communication.
 * - `NetworkClientInterface`: A templated class for managing client-side network communication.
 *
 * The file supports both the `nw_ws_client`/`nw_ws_server` and `cw_ws_client`/`cw_ws_server` protocols,
 * providing type aliases (`NetworkClient` and `NetworkServer`) for easier use of these protocols.
 *
 * It also includes static methods to handle various network events, such as connection establishment,
 * data transmission, and connection closure for both clients and servers.
 */

#ifndef NETWORKUTILITIES_HPP
#define NETWORKUTILITIES_HPP

#include "../dependencies/websocket-tools/websocket-tools.hpp"

#include <shared_mutex>
#include <type_traits>

#include <mutex.h>

#include "IPlugLogger.h"
#include "IPlugStructs.h"

/**
 * @struct NetworkTypes
 * @brief A structure that defines various types and utilities used for managing network connections.
 *
 * This structure contains type definitions and a helper class to manage
 * mutex locks, providing both shared and exclusive locking mechanisms.
 */

struct NetworkTypes
{
protected:
    
    /**
     * @typedef ConnectionID
     * @brief Defines a type alias for WebSocket connection identifiers.
     *
     * This alias maps the WebSocket connection identifier type to a more descriptive name `ConnectionID`.
     * It simplifies the code when referring to the connection IDs throughout the network management operations.
     */
    
    using ConnectionID = ws_connection_id;

    /**
     * @typedef SharedMutex
     * @brief Defines a type alias for a shared mutex used in thread synchronization.
     *
     * This alias represents a shared mutex type that allows multiple threads to acquire the lock simultaneously for reading,
     * while ensuring exclusive access for writing. The `SharedMutex` type simplifies the use of the shared mutex in the network
     * management context.
     */
    
    using SharedMutex = WDL_SharedMutex;
    
    /**
     * @typedef SharedLock
     * @brief Defines a type alias for a shared lock using the shared mutex.
     *
     * This alias represents a lock type that provides shared (read) access to a resource, allowing multiple threads
     * to acquire the lock concurrently for reading. It ensures that no thread can modify the resource while the shared lock is held.
     */
    
    using SharedLock = WDL_MutexLockShared;
    
    /**
     * @typedef ExclusiveLock
     * @brief Defines a type alias for an exclusive lock using the shared mutex.
     *
     * This alias represents a lock type that provides exclusive (write) access to a resource. When a thread holds
     * the exclusive lock, no other threads can access the resource, ensuring safe modification in a concurrent environment.
     */
    using ExclusiveLock = WDL_MutexLockExclusive;
    
    /**
     * @class VariableLock
     * @brief A helper class that manages mutex locks, supporting both shared and exclusive locking mechanisms.
     *
     * This class provides a flexible interface to acquire either a shared or exclusive lock on a resource based on
     * the given parameters. It simplifies the process of managing thread synchronization by automatically locking
     * and unlocking a mutex in either shared or exclusive mode.
     */
    
    class VariableLock
    {
    public:
        
        /**
         * @brief Constructs a VariableLock object and locks the mutex in either shared or exclusive mode.
         *
         * This constructor initializes the `VariableLock` with a given mutex and locks it based on the `shared` parameter.
         * If `shared` is true, the lock is acquired in shared mode, allowing multiple readers. If `shared` is false, the lock is
         * acquired in exclusive mode, ensuring only one writer.
         *
         * @param mutex A pointer to a `SharedMutex` object to be locked.
         * @param shared A boolean flag indicating whether to acquire a shared lock (true) or an exclusive lock (false). Defaults to true.
         */
        
        VariableLock(SharedMutex* mutex, bool shared = true)
        : mMutex(mutex)
        , mShared(shared)
        {
            if (mMutex)
            {
                if (shared)
                    mMutex->LockShared();
                else
                    mMutex->LockExclusive();
            }
        }
        
        /**
         * @brief Destructor for the VariableLock class, unlocking the mutex.
         *
         * The destructor automatically releases the held lock (either shared or exclusive) on the associated mutex when the
         * `VariableLock` object goes out of scope. This ensures proper cleanup and thread synchronization by unlocking the mutex.
         */
        
        ~VariableLock()
        {
            Destroy();
        }
        
        /**
         * @brief Safely destroys or resets the current lock state.
         *
         * The `Destroy` method is responsible for releasing or resetting the held lock, ensuring that the associated
         * mutex is properly unlocked. This method should be called when the lock is no longer needed, ensuring proper
         * thread synchronization and avoiding deadlocks.
         */
        
        void Destroy()
        {
            if (mMutex)
            {
                if (mShared)
                    mMutex->UnlockShared();
                else
                    mMutex->UnlockExclusive();
                    
                mMutex = nullptr;
            }
        }
        
        /**
         * @brief Promotes a shared lock to an exclusive lock.
         *
         * The `Promote` method is used to upgrade a currently held shared (read) lock to an exclusive (write) lock.
         * This allows a thread that initially acquired a shared lock to gain exclusive access to modify the resource,
         * ensuring thread-safe operations. Proper care should be taken to avoid deadlocks when promoting locks.
         */
        
        void Promote()
        {
            if (mMutex && mShared)
            {
                mMutex->SharedToExclusive();
                mShared = false;
            }
        }
        
        /**
         * @brief Demotes an exclusive lock to a shared lock.
         *
         * The `Demote` method is used to downgrade a currently held exclusive (write) lock to a shared (read) lock.
         * This allows a thread to release exclusive access and permit other threads to read the resource, while still
         * retaining shared access to it. This can help improve concurrency when write access is no longer required.
         */
        
        void Demote()
        {
            if (mMutex && !mShared)
            {
                mMutex->ExclusiveToShared();
                mShared = true;
            }
        }
        
        /**
         * @brief Deleted copy constructor to prevent copying of VariableLock instances.
         *
         * This copy constructor is explicitly deleted to prevent copying of `VariableLock` objects.
         * Mutex locks should not be copied, as doing so could lead to undefined behavior or deadlocks in
         * multithreaded environments. Each `VariableLock` instance should manage its own lock.
         *
         * @param rhs The `VariableLock` object to copy (which is disallowed).
         */
        
        VariableLock(VariableLock const& rhs) = delete;
        
        /**
         * @brief Deleted move constructor to prevent moving of VariableLock instances.
         *
         * This move constructor is explicitly deleted to prevent moving of `VariableLock` objects.
         * Moving a lock could lead to unsafe behavior in multithreaded environments, such as losing control over
         * the original lock's state. Each `VariableLock` instance is responsible for managing its own mutex lock,
         * and moving would violate that responsibility.
         *
         * @param rhs The rvalue reference to the `VariableLock` object to move (which is disallowed).
         */
        
        VariableLock(VariableLock const&& rhs) = delete;
        
        /**
         * @brief Deleted copy assignment operator to prevent assignment of VariableLock instances.
         *
         * This copy assignment operator is explicitly deleted to prevent assigning one `VariableLock` object to another.
         * Copying or assigning locks is unsafe in multithreaded environments, as it could lead to duplicate or lost ownership
         * of mutex locks, resulting in undefined behavior or deadlocks. Each `VariableLock` instance must manage its own lock.
         *
         * @param rhs The `VariableLock` object to be assigned (which is disallowed).
         */
        
        void operator = (VariableLock const& rhs) = delete;
        
        /**
         * @brief Deleted move assignment operator to prevent moving of VariableLock instances.
         *
         * This move assignment operator is explicitly deleted to prevent moving a `VariableLock` object.
         * Moving a lock could lead to losing control over the mutex lock's state, which may cause unsafe
         * behavior in multithreaded environments. Each `VariableLock` must manage its own lock, and moving
         * an instance would violate this principle.
         *
         * @param rhs The rvalue reference to the `VariableLock` object to be moved (which is disallowed).
         */
        
        void operator = (VariableLock const&& rhs) = delete;
        
    private:
        
        /**
         * @brief Pointer to the shared mutex used for locking.
         *
         * This member variable holds a pointer to the `SharedMutex` object that is being locked by the `VariableLock` instance.
         * It is used to manage thread synchronization by enabling either shared or exclusive access to the protected resource.
         */
        
        SharedMutex *mMutex;
        
        /**
         * @brief Flag indicating whether the lock is shared or exclusive.
         *
         * This boolean member variable determines the type of lock held by the `VariableLock` instance.
         * If `true`, the lock is a shared (read) lock, allowing multiple threads to access the resource simultaneously.
         * If `false`, the lock is exclusive (write), ensuring only one thread can modify the resource at a time.
         */
        
        bool mShared;
    };
};

// A generic web socket network server that requires an interface for the specifics of the networking

/**
 * @brief Template class for a network server interface, providing network management functionality.
 *
 * This class serves as an interface for network server operations, built on top of the `NetworkTypes` structure.
 * It provides various functionalities for managing network connections and handling communication.
 * The template parameter `T` allows for flexibility in the data types or protocols used by the server.
 *
 * @tparam T The data type or protocol used by the network server interface, which can vary depending on the use case.
 */

template <class T>
class NetworkServerInterface : protected NetworkTypes
{
public:
    
    /**
     * @brief Default constructor for the NetworkServerInterface class.
     *
     * This constructor initializes the `NetworkServerInterface` object, setting the internal server pointer (`mServer`)
     * to `nullptr`. It ensures that the server interface starts with no associated server, ready to be initialized later.
     */
    NetworkServerInterface() : mServer(nullptr)  {}
    
    /**
     * @brief Virtual destructor for the NetworkServerInterface class.
     *
     * This virtual destructor ensures proper cleanup of resources when a `NetworkServerInterface` object is destroyed.
     * It is defined as virtual to allow derived classes to implement their own cleanup logic, ensuring that destructors
     * in derived classes are called correctly when objects are deleted through base class pointers.
     */
    
    virtual ~NetworkServerInterface() {}
    
    /**
     * @brief Deleted copy constructor to prevent copying of NetworkServerInterface instances.
     *
     * This copy constructor is explicitly deleted to prevent copying of `NetworkServerInterface` objects.
     * Copying network server interfaces is typically unsafe or unnecessary, as it could lead to duplicate
     * or invalid server states, resource conflicts, or improper network management. Each instance should manage its own server.
     *
     * @param other The `NetworkServerInterface` object to copy (which is disallowed).
     */
    
    NetworkServerInterface(const NetworkServerInterface&) = delete;
    
    /**
     * @brief Deleted copy assignment operator to prevent assignment of NetworkServerInterface instances.
     *
     * This copy assignment operator is explicitly deleted to prevent assigning one `NetworkServerInterface` object
     * to another. Copying or assigning network server interfaces can lead to issues such as resource conflicts,
     * duplicate server states, or improper network management. Each instance should manage its own server independently.
     *
     * @param other The `NetworkServerInterface` object to assign from (which is disallowed).
     * @return This function does not return any value since assignment is disallowed.
     */
    
    NetworkServerInterface& operator=(const NetworkServerInterface&) = delete;
    
    /**
     * @brief Starts the network server on the specified port.
     *
     * This method initializes and starts the network server, listening on the given port for incoming connections.
     * If no port is provided, the server defaults to listening on port "8001".
     *
     * @param port The port number on which the server will listen for connections, provided as a string. Defaults to "8001" if not specified.
     */
    
    void StartServer(const char* port = "8001")
    {
        VariableLock lock(&mMutex);
        
        if (!mServer)
        {
            static constexpr ws_server_handlers handlers = { DoConnectServer,
                                                             DoReadyServer,
                                                             DoDataServer,
                                                             DoCloseServer
                                                            };
            
            auto server = T::create(port, "/ws", ws_server_owner<handlers>{ this });
            
            lock.Promote();
            mServer = server;
            
            DBGMSG("SERVER: Websocket server running on port %s\n", port);
        }
        else
            DBGMSG("SERVER: Websocket server already running on post %s\n", port);
    }
    
    /**
     * @brief Stops the network server and terminates all connections.
     *
     * This method gracefully shuts down the network server, closing any active connections and freeing associated resources.
     * It ensures that the server stops listening for incoming connections and that the network operations are terminated safely.
     */
    
    void StopServer()
    {
        VariableLock lock(&mMutex);

        if (mServer)
        {
            lock.Promote();
            std::unique_ptr<T> release(mServer);
            mServer = nullptr;
            lock.Destroy();
            DBGMSG("SERVER: Destroyed\n");
        }
    }
    
    /**
     * @brief Retrieves the number of currently connected clients.
     *
     * This method returns the total number of clients that are currently connected to the server.
     * It provides a quick way to check the active client count during server operations.
     *
     * @return The number of connected clients as an integer.
     */
    
    int NClients() const
    {
        SharedLock lock(&mMutex);
        
        return mServer ? static_cast<int>(mServer->size()) : 0;
    }
    
    /**
     * @brief Sends data to a specific client identified by the connection ID.
     *
     * This method transmits a data chunk to the client associated with the provided WebSocket connection ID (`id`).
     * It uses the provided `IByteChunk` object to specify the data that will be sent. The method ensures that the data
     * is delivered to the correct client over the network.
     *
     * @param id The WebSocket connection ID that uniquely identifies the client to which the data should be sent.
     * @param chunk A reference to an `iplug::IByteChunk` object containing the data to be transmitted.
     * @return Returns `true` if the data was successfully sent, `false` otherwise.
     */
    
    bool SendDataToClient(ws_connection_id id, const iplug::IByteChunk& chunk)
    {
        SharedLock lock(&mMutex);
        
        if (mServer)
        {
            mServer->send(id, chunk.GetData(), chunk.Size());
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Broadcasts data from the server to all connected clients.
     *
     * This method sends a data chunk to all currently connected clients. It uses the provided `IByteChunk` object
     * to specify the data that will be broadcast from the server. This allows the server to send the same data
     * simultaneously to multiple clients.
     *
     * @param chunk A reference to an `iplug::IByteChunk` object containing the data to be transmitted to all clients.
     * @return Returns `true` if the data was successfully sent to all clients, `false` otherwise.
     */
    
    bool SendDataFromServer(const iplug::IByteChunk& chunk)
    {
        SharedLock lock(&mMutex);
        
        if (mServer)
        {
            mServer->send(chunk.GetData(), chunk.Size());
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Checks if the server is currently connected and running.
     *
     * This method returns the connection status of the server, indicating whether the server is actively running
     * and able to accept client connections. It provides a way to verify if the server is operational.
     *
     * @return Returns `true` if the server is connected and running, `false` otherwise.
     */
    
    bool IsServerConnected() const
    {
        SharedLock lock(&mMutex);
        
        return NClients();
    }
    
    /**
     * @brief Checks if the server is currently running.
     *
     * This method returns the status of the server, indicating whether it is actively running and handling connections.
     * It helps to determine if the server is in a running state, even if it may not currently have any active connections.
     *
     * @return Returns `true` if the server is running, `false` otherwise.
     */
    
    bool IsServerRunning() const
    {
        SharedLock lock(&mMutex);
        
        return mServer;
    }
    
private:
    
    // Customisable Handlers
    
    /**
     * @brief Callback method invoked when the server is ready and a client connection is established.
     *
     * This virtual method is called when the server has successfully started and is ready to handle a connection.
     * It provides a hook for subclasses to implement custom behavior when a client is connected and the server is ready.
     * The connection ID of the client is passed as a parameter for further handling.
     *
     * @param id The `ConnectionID` of the client that has connected to the server.
     */
    
    virtual void OnServerReady(ConnectionID id) {}
    
    /**
     * @brief Pure virtual callback method invoked when data is received from a client.
     *
     * This method is called whenever the server receives data from a client. It must be implemented by any derived class
     * to handle incoming data appropriately. The method provides the connection ID of the client sending the data and
     * the actual data received as an `iplug::IByteStream` object.
     *
     * @param id The `ConnectionID` of the client sending the data.
     * @param data A reference to an `iplug::IByteStream` object containing the received data.
     */
    
    virtual void OnDataToServer(ConnectionID id, const iplug::IByteStream& data) = 0;
    
    // Handlers
    
    /**
     * @brief Handles the establishment of a socket connection for a given client.
     *
     * This method is responsible for managing the socket connection for the client identified by the provided `ConnectionID`.
     * It handles the initial setup and any necessary processing when a new client connects to the server.
     *
     * @param id The `ConnectionID` of the client whose socket connection is being handled.
     */
    
    void HandleSocketConnection(ConnectionID id)
    {
        SharedLock lock(&mMutex);
        
        DBGMSG("SERVER: Connected\n");
    }
    
    /**
     * @brief Handles the event when a client's socket connection is ready for communication.
     *
     * This method is called when the socket for a given client, identified by the `ConnectionID`, is ready for data transmission.
     * It ensures that the necessary setup is complete, allowing the server and client to begin exchanging data.
     *
     * @param id The `ConnectionID` of the client whose socket is ready for communication.
     */
    
    void HandleSocketReady(ConnectionID id)
    {
        SharedLock lock(&mMutex);
                
        DBGMSG("SERVER: New connection - num clients %i\n", NClients());
        
        OnServerReady(NClients() - 1); // should defer to main thread
    }
    
    /**
     * @brief Handles incoming data from a client's socket connection.
     *
     * This method is called when data is received from the client identified by the `ConnectionID`.
     * It processes the received data, which is passed as a pointer to the data buffer along with its size.
     * The method is responsible for handling and interpreting the incoming data.
     *
     * @param id The `ConnectionID` of the client sending the data.
     * @param pData A pointer to the data buffer containing the received data.
     * @param size The size of the data buffer, in bytes.
     */
    
    void HandleSocketData(ConnectionID id, const void *pData, size_t size)
    {
        SharedLock lock(&mMutex);
                
        if (mServer)
        {
            iplug::IByteStream stream(pData, static_cast<int>(size));
            OnDataToServer(id, stream);
        }
    }
    
    /**
     * @brief Handles the closure of a client's socket connection.
     *
     * This method is called when a client's socket connection, identified by the `ConnectionID`, is closed.
     * It is responsible for performing any necessary cleanup or resource management when the connection is terminated.
     * This may include removing the client from the server's active connection list or releasing resources tied to the connection.
     *
     * @param id The `ConnectionID` of the client whose socket connection has been closed.
     */
    
    void HandleSocketClose(ConnectionID id)
    {
        SharedLock lock(&mMutex);
                
        DBGMSG("SERVER: Closed connection - num clients %i\n", NClients());
    }
    
    // Static Handlers
    
    /**
     * @brief Static helper function to connect a client to the server.
     *
     * This static method is responsible for establishing a connection between the client, identified by the `ConnectionID`, and the server.
     * The server is passed as an untyped pointer (`void*`) to allow flexibility in how the server object is represented. The method handles
     * the necessary setup to complete the connection process.
     *
     * @param id The `ConnectionID` of the client to be connected to the server.
     * @param pUntypedServer A pointer to the server object, provided as an untyped (`void*`) pointer.
     */
    
    static void DoConnectServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        // N.B. Return values are 0 for success and 1 for close
        
        if (pServer)
            pServer->HandleSocketConnection(id);
    }
    
    /**
     * @brief Static helper function to mark a client's connection as ready for communication with the server.
     *
     * This static method is called when the client's connection, identified by the `ConnectionID`, is ready to begin data transmission.
     * The server is passed as an untyped pointer (`void*`), allowing for flexibility in representing the server object. This method handles
     * the setup required to transition the connection to a ready state.
     *
     * @param id The `ConnectionID` of the client whose connection is ready for communication.
     * @param pUntypedServer A pointer to the server object, provided as an untyped (`void*`) pointer.
     */
    
    static void DoReadyServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        if (pServer)
            pServer->HandleSocketReady(id);
    }
    
    /**
     * @brief Static helper function to handle incoming data from a client to the server.
     *
     * This static method processes data sent by the client, identified by the `ConnectionID`, to the server.
     * The data is provided as a pointer to a buffer (`pData`), along with its size in bytes. The server is passed as
     * an untyped pointer (`void*`) to allow flexibility in the server's representation. The method handles interpreting
     * and managing the received data for further processing by the server.
     *
     * @param id The `ConnectionID` of the client sending the data.
     * @param pData A pointer to the data buffer containing the data sent by the client.
     * @param size The size of the data buffer, in bytes.
     * @param pUntypedServer A pointer to the server object, provided as an untyped (`void*`) pointer.
     */
    
    static void DoDataServer(ConnectionID id, const void* pData, size_t size, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        if (pServer)
            pServer->HandleSocketData(id, pData, size);
    }
    
    /**
     * @brief Static helper function to handle the closure of a client's connection to the server.
     *
     * This static method is called when a client's connection, identified by the `ConnectionID`, is closed.
     * It is responsible for performing the necessary cleanup on the server side, such as releasing resources
     * associated with the connection or updating the server's internal state. The server is passed as an untyped
     * pointer (`void*`), allowing for flexibility in its representation.
     *
     * @param id The `ConnectionID` of the client whose connection is being closed.
     * @param pUntypedServer A pointer to the server object, provided as an untyped (`void*`) pointer.
     */
    
    static void DoCloseServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);
        
        if (pServer)
            pServer->HandleSocketClose(id);
    }
    
    /**
     * @brief Static helper function to cast an untyped pointer to a NetworkServerInterface pointer.
     *
     * This static method converts an untyped (`void*`) server pointer into a typed `NetworkServerInterface*` pointer.
     * It is useful when the server object is passed around as a `void*` pointer, allowing it to be safely cast back to
     * its proper type for further operations.
     *
     * @param pUntypedServer A pointer to the server object, provided as an untyped (`void*`) pointer.
     * @return Returns a pointer to the `NetworkServerInterface` object, cast from the untyped server pointer.
     */
    
    static NetworkServerInterface* AsServer(void *pUntypedServer)
    {
        auto pServer = reinterpret_cast<NetworkServerInterface *>(pUntypedServer);
        
        // This may occur when a request hits the server before the context is saved
        
        if (pServer->mServer == nullptr)
            return nullptr;
        
        return pServer;
    }
    
    /**
     * @brief Pointer to the server instance of type `T`.
     *
     * This member variable holds a pointer to the server object, which is templated by type `T`.
     * It represents the actual server instance that the `NetworkServerInterface` manages and interacts with during its operations.
     * The type `T` allows for flexibility in the server's implementation, depending on the specific use case.
     */
    
    T *mServer;

protected:
    
    /**
     * @brief A mutable shared mutex used for thread synchronization.
     *
     * This member variable holds a `SharedMutex` object that is used to control concurrent access to shared resources.
     * The `mutable` keyword allows this mutex to be modified even in `const` methods, enabling thread-safe operations
     * within methods that are logically constant but still need to lock or unlock the mutex.
     */
    
    mutable SharedMutex mMutex;
};

// A generic web socket network client that requires an interface for the specifics of the networking

/**
 * @brief Template class for a network client interface, providing network communication functionality.
 *
 * This class serves as an interface for network client operations, built on top of the `NetworkTypes` structure.
 * It provides functionalities for managing client-side network connections and handling communication with a server.
 * The template parameter `T` allows for flexibility in the data types or protocols used by the client.
 *
 * @tparam T The data type or protocol used by the network client interface, which can vary depending on the specific use case.
 */

template <class T>
class NetworkClientInterface : protected NetworkTypes
{
public:
    
    // Creation and Deletion
    
    /**
     * @brief Default constructor for the NetworkClientInterface class.
     *
     * This constructor initializes the `NetworkClientInterface` object, setting the internal connection pointer (`mConnection`)
     * to `nullptr`. It ensures that the client interface starts with no active connection, ready to be initialized and connected
     * to a server later.
     */
    
    NetworkClientInterface() : mConnection(nullptr) {}
    
    /**
     * @brief Virtual destructor for the NetworkClientInterface class.
     *
     * This virtual destructor ensures proper cleanup of resources when a `NetworkClientInterface` object is destroyed.
     * It is defined as virtual to allow derived classes to implement their own cleanup logic, ensuring that destructors
     * in derived classes are called correctly when objects are deleted through base class pointers.
     */
    
    virtual ~NetworkClientInterface() {}
    
    /**
     * @brief Deleted copy constructor to prevent copying of NetworkClientInterface instances.
     *
     * This copy constructor is explicitly deleted to prevent copying of `NetworkClientInterface` objects.
     * Copying network client interfaces is typically unsafe or unnecessary, as it could lead to duplicate
     * or invalid connection states, resource conflicts, or improper network management. Each instance should manage its own connection.
     *
     * @param other The `NetworkClientInterface` object to copy (which is disallowed).
     */
    
    NetworkClientInterface(const NetworkClientInterface&) = delete;
    
    /**
     * @brief Deleted copy assignment operator to prevent assignment of NetworkClientInterface instances.
     *
     * This copy assignment operator is explicitly deleted to prevent assigning one `NetworkClientInterface` object
     * to another. Copying or assigning network client interfaces can lead to issues such as resource conflicts,
     * duplicate connection states, or improper network management. Each instance should manage its own connection independently.
     *
     * @param other The `NetworkClientInterface` object to assign from (which is disallowed).
     * @return This function does not return any value since assignment is disallowed.
     */
    
    NetworkClientInterface& operator=(const NetworkClientInterface&) = delete;
    
    // Public Methods
    
    /**
     * @brief Connects the client to a server at the specified host and port.
     *
     * This method establishes a network connection to the server located at the provided `host` and `port`.
     * If no port is specified, it defaults to port 8001. It returns `true` if the connection is successfully
     * established, and `false` otherwise.
     *
     * @param host A C-string representing the hostname or IP address of the server to connect to.
     * @param port The port number to connect to on the server. Defaults to 8001 if not specified.
     * @return Returns `true` if the connection was successful, `false` otherwise.
     */
    
    bool Connect(const char* host, int port = 8001)
    {
        DBGMSG("CLIENT: Connection attempt: %s \n", host);
        
        static constexpr ws_client_handlers handlers { DoDataClient, DoCloseClient };
        auto client = T::create(host, port, "/ws", ws_client_owner<handlers>{ this });

        VariableLock lock(&mMutex, false);
        
        std::unique_ptr<T> release(mConnection);
        mConnection = client;
        
        if (client)
        {
            DBGMSG("CLIENT: Connection successful\n");
            mServer.Set(host);
        }
        else
            DBGMSG("CLIENT: Connection error\n");
        
        lock.Destroy();
        
        return IsClientConnected();
    }
    
    /**
     * @brief Sends data from the client to the connected server.
     *
     * This method transmits a data chunk from the client to the server. The data to be sent is provided as an
     * `iplug::IByteChunk` object, which contains the serialized data that needs to be transmitted over the network.
     *
     * @param chunk A reference to an `iplug::IByteChunk` object containing the data to be sent to the server.
     */
    
    void SendDataFromClient(const iplug::IByteChunk& chunk)
    {
        SharedLock lock(&mMutex);

        if (mConnection)
        {
            // FIX - errors
            
            mConnection->send(chunk.GetData(), chunk.Size());
        }
    }
    
    /**
     * @brief Checks if the client is currently connected to a server.
     *
     * This method returns the connection status of the client, indicating whether the client is actively connected to a server.
     * It provides a way to verify if the client is in an operational state and can send or receive data.
     *
     * @return Returns `true` if the client is connected to a server, `false` otherwise.
     */
    
    bool IsClientConnected() const
    {
        SharedLock lock(&mMutex);

        return mConnection;
    }
    
    /**
     * @brief Retrieves the name or address of the connected server.
     *
     * This method retrieves the name or address of the server that the client is connected to. The server name is stored in the
     * provided `WDL_String` reference. This is useful for identifying the server in client-server communications.
     *
     * @param name A reference to a `WDL_String` object where the server name or address will be stored.
     */
    
    void GetServerName(WDL_String& name)
    {
        SharedLock lock(&mMutex);
        
        name = mServer;
    }
    
private:
    
    // Customisable Methods
    
    /**
     * @brief Pure virtual callback method invoked when data is received from the server.
     *
     * This method is called whenever the client receives data from the connected server.
     * It must be implemented by any derived class to handle the incoming data appropriately.
     * The data is provided as an `iplug::IByteStream` object, which contains the serialized data sent by the server.
     *
     * @param data A reference to an `iplug::IByteStream` object containing the received data from the server.
     */
    
    virtual void OnDataToClient(const iplug::IByteStream& data) = 0;
    
    /**
     * @brief Callback method invoked when the client connection is closed.
     *
     * This virtual method is called when the client's connection to the server is closed, either by the client or the server.
     * It provides a hook for derived classes to implement custom behavior for handling the closure of the client connection,
     * such as cleanup, logging, or attempting to reconnect.
     */
    
    virtual void OnCloseClient() {}
    
    /**
     * @brief Static helper function to cast an untyped pointer to a NetworkClientInterface pointer.
     *
     * This static method converts an untyped (`void*`) pointer into a typed `NetworkClientInterface*` pointer.
     * It is useful when the client object is passed as a `void*` pointer, allowing it to be safely cast back to
     * its proper type for further operations.
     *
     * @param x A pointer to the client object, provided as an untyped (`void*`) pointer.
     * @return Returns a pointer to the `NetworkClientInterface` object, cast from the untyped pointer.
     */
    
    static NetworkClientInterface* AsClient(void *x)
    {
        return reinterpret_cast<NetworkClientInterface *>(x);
    }
    
    // Handlers
    
    /**
     * @brief Handles the closure of the client's connection to the server.
     *
     * This method is responsible for managing the shutdown process when the client connection to the server is closed.
     * It ensures that any necessary cleanup is performed, such as freeing resources or resetting the client's state,
     * and may trigger additional logic to handle the closure.
     */
    
    void HandleClose()
    {
        VariableLock lock(&mMutex);
        
        // Avoid closing twice (in case the API calls close multiple times)
        
        if (mConnection)
        {
            lock.Promote();
            std::unique_ptr<T> release(mConnection);
            mConnection = nullptr;
            mServer.Set("");
            lock.Demote();
            OnCloseClient();
            DBGMSG("CLIENT: Disconnected\n");
        }
    }
    
    /**
     * @brief Handles the incoming data from the server.
     *
     * This method processes data received from the server. The data is provided as a pointer to the buffer (`pData`)
     * and the size of the data buffer in bytes (`size`). It is responsible for interpreting the received data and
     * performing any necessary actions based on the data contents.
     *
     * @param pData A pointer to the data buffer containing the data received from the server.
     * @param size The size of the data buffer, in bytes.
     */
    
    void HandleData(const void *pData, size_t size)
    {
        SharedLock lock(&mMutex);
        
        iplug::IByteStream stream(pData, static_cast<int>(size));
        
        OnDataToClient(stream);
    }
    
    // Static Handlers
    
    /**
     * @brief Static helper function to handle data received from the server for a specific client.
     *
     * This static method processes incoming data sent by the server to the client identified by the `ConnectionID`.
     * The data is provided as a pointer to a buffer (`pData`) along with its size in bytes (`size`). The client object is
     * passed as an untyped pointer (`void*`), which is cast to the appropriate type internally. This method ensures that
     * the client processes the received data correctly.
     *
     * @param id The `ConnectionID` of the client receiving the data.
     * @param pData A pointer to the data buffer containing the data sent by the server.
     * @param size The size of the data buffer, in bytes.
     * @param x A pointer to the client object, provided as an untyped (`void*`) pointer.
     */
    
    static void DoDataClient(ConnectionID id, const void *pData, size_t size, void *x)
    {
        AsClient(x)->HandleData(pData, size);
    }
    
    /**
     * @brief Static helper function to handle the closure of a client's connection to the server.
     *
     * This static method is called when the connection for the client identified by the `ConnectionID` is closed.
     * It ensures that the necessary cleanup is performed for the client connection. The client object is passed as an
     * untyped pointer (`void*`), which is cast to the appropriate type internally, allowing for flexible client management.
     *
     * @param id The `ConnectionID` of the client whose connection is being closed.
     * @param x A pointer to the client object, provided as an untyped (`void*`) pointer.
     */
    
    static void DoCloseClient(ConnectionID id, void *x)
    {
        AsClient(x)->HandleClose();
    }
    
    /**
     * @brief Stores the name or address of the server the client is connected to.
     *
     * This member variable holds the server's name or address as a `WDL_String` object.
     * It is used to track the server information for client-server communications and may be displayed
     * or used internally to identify the server during network operations.
     */
    
    WDL_String mServer;
    
    /**
     * @brief A mutable shared mutex used for thread synchronization.
     *
     * This member variable holds a `SharedMutex` object that manages access to shared resources among multiple threads.
     * The `mutable` keyword allows this mutex to be modified even in `const` methods, enabling thread-safe operations
     * in methods that are logically constant but still require locking or unlocking of the mutex.
     */
    
    mutable SharedMutex mMutex;
    
    /**
     * @brief Pointer to the client connection of type `T`.
     *
     * This member variable holds a pointer to the client connection object, templated by type `T`.
     * It represents the actual connection instance that the `NetworkClientInterface` manages and interacts with
     * during its operations. The type `T` allows for flexibility in the connection's implementation, depending
     * on the specific use case or protocol.
     */
    
    T *mConnection;
};

// Concrete implementations based on the platform

#ifdef __APPLE__

/**
 * @typedef NetworkClient
 * @brief Type alias for a WebSocket-based network client.
 *
 * This alias defines `NetworkClient` as a specific instantiation of the `NetworkClientInterface` template class,
 * where the connection type is based on `nw_ws_client`. It simplifies the usage of the `NetworkClientInterface`
 * when working with WebSocket clients, making the code more readable and reducing redundancy.
 */

using NetworkClient = NetworkClientInterface<nw_ws_client>;

/**
 * @typedef NetworkServer
 * @brief Type alias for a WebSocket-based network server.
 *
 * This alias defines `NetworkServer` as a specific instantiation of the `NetworkServerInterface` template class,
 * where the server type is based on `nw_ws_server`. It simplifies the usage of the `NetworkServerInterface` when
 * working with WebSocket servers, making the code more concise and easier to read.
 */

using NetworkServer = NetworkServerInterface<nw_ws_server>;
#else

/**
 * @typedef NetworkClient
 * @brief Type alias for a WebSocket-based network client using the `cw_ws_client` protocol.
 *
 * This alias defines `NetworkClient` as a specific instantiation of the `NetworkClientInterface` template class,
 * where the client type is based on `cw_ws_client`. It simplifies the usage of the `NetworkClientInterface` when
 * working with WebSocket clients that use the `cw_ws_client` protocol, improving code readability and reducing redundancy.
 */

using NetworkClient = NetworkClientInterface<cw_ws_client>;

/**
 * @typedef NetworkServer
 * @brief Type alias for a WebSocket-based network server using the `cw_ws_server` protocol.
 *
 * This alias defines `NetworkServer` as a specific instantiation of the `NetworkServerInterface` template class,
 * where the server type is based on `cw_ws_server`. It simplifies the usage of the `NetworkServerInterface` when
 * working with WebSocket servers that use the `cw_ws_server` protocol, enhancing code readability and reducing redundancy.
 */

using NetworkServer = NetworkServerInterface<cw_ws_server>;
#endif

#endif /* NETWORKUTILITIES_HPP */
