
/**
 * @file NetworkClient.hpp
 * @brief Defines the interface and implementation details for network client communication.
 *
 * This file contains the `NetworkClientInterface` template class, which provides an interface
 * for implementing client-side network communication with various server types. It handles
 * connection management, data transmission, and connection termination. The interface uses
 * template specialization to allow flexibility in connection implementations for different platforms.
 *
 * Key components:
 * - The `NetworkClientInterface` class, which provides methods for connecting, sending data,
 *   receiving data, and handling connection closure.
 * - Platform-specific typedefs for `NetworkClient`, allowing different client implementations
 *   based on the operating system.
 * - Utilities for casting and handling connections using generic pointers.
 *
 * The file supports both Apple (`nw_ws_client`) and non-Apple (`cw_ws_client`) platforms through
 * conditional compilation.
 */

#ifndef NETWORKCLIENT_HPP
#define NETWORKCLIENT_HPP

#include <memory>

#include "IPlugLogger.h"
#include "IPlugStructs.h"

#include "NetworkTypes.hpp"

// A generic web socket network client that requires an interface for the specifics of the networking

/**
 * @brief Interface class for a network client, providing base functionality for handling network communication.
 *
 * This template class defines an interface for network clients. It is intended to be used as a base class
 * for various network client implementations. The class is parameterized with a template type `T` and
 * inherits from `NetworkTypes`, which provides common network type definitions and utilities.
 *
 * @tparam T The type used for customization or specialization within the network client interface.
 */

template <class T>
class NetworkClientInterface : protected NetworkTypes
{
public:
    
    // Creation and Deletion
    
    /**
     * @brief Default constructor for the NetworkClientInterface class.
     *
     * Initializes a new instance of the NetworkClientInterface class with default values.
     * The connection is set to nullptr, and the port is initialized to 0.
     */
    
    NetworkClientInterface() : mConnection(nullptr), mPort(0) {}
    
    /**
     * @brief Virtual destructor for the NetworkClientInterface class.
     *
     * Ensures proper cleanup of derived classes when deleted through a pointer to NetworkClientInterface.
     */
    
    virtual ~NetworkClientInterface() {}
    
    /**
     * @brief Deleted copy constructor for the NetworkClientInterface class.
     *
     * Copying instances of NetworkClientInterface is not allowed to prevent multiple instances
     * from sharing the same connection or other resources.
     *
     * @param other The NetworkClientInterface object to copy from (deleted).
     */
    
    NetworkClientInterface(const NetworkClientInterface&) = delete;
    
    /**
     * @brief Deleted copy assignment operator for the NetworkClientInterface class.
     *
     * Copy assignment is disabled to prevent assigning one NetworkClientInterface instance to another,
     * which could lead to issues with resource management, such as sharing the same connection.
     *
     * @param other The NetworkClientInterface object to assign from (deleted).
     * @return This function does not return anything since it is deleted.
     */
    
    NetworkClientInterface& operator=(const NetworkClientInterface&) = delete;
    
    // Public Methods
    
    /**
     * @brief Establishes a connection to a specified host and port.
     *
     * This method attempts to connect to a remote server at the given host and port.
     *
     * @param host The hostname or IP address of the server to connect to.
     * @param port The port number to connect to on the remote server.
     * @return Returns true if the connection is successfully established, false otherwise.
     */
    
    bool Connect(const char* host, uint16_t port)
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
            mPort = port;
        }
        else
        {
            DBGMSG("CLIENT: Connection error\n");
            mServer.Set("");
            mPort = 0;
        }
        
        lock.Destroy();
        
        return IsClientConnected();
    }
    
    /**
     * @brief Disconnects the client from the current network connection.
     *
     * This method closes the existing connection to the server and cleans up any associated
     * resources. It should be called when the client no longer needs the connection or when
     * the connection needs to be reset.
     */
    
    void Disconnect()
    {
        HandleClose();
    }
    
    /**
     * @brief Sends data from the client to the connected server.
     *
     * This method transmits the provided data chunk to the server. The data is passed as an
     * `iplug::IByteChunk` object, which encapsulates a block of binary data.
     *
     * @param chunk A reference to an `iplug::IByteChunk` object containing the data to be sent
     *              from the client to the server.
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
     * @brief Checks whether the client is currently connected to a server.
     *
     * This method returns the connection status of the client. It can be used to verify
     * if the client is successfully connected to a remote server.
     *
     * @return True if the client is connected, false otherwise.
     */
    
    bool IsClientConnected() const
    {
        SharedLock lock(&mMutex);
        
        return mConnection;
    }
    
    /**
     * @brief Retrieves the name or address of the connected server.
     *
     * This method returns a `WDL_String` containing the name or IP address of the server
     * to which the client is currently connected. If the client is not connected, the
     * returned string may be empty or contain a default value.
     *
     * @return A `WDL_String` representing the server name or address.
     */
    
    WDL_String GetServerName() const
    {
        SharedLock lock(&mMutex);
                
        return mServer;
    }
    
    /**
     * @brief Retrieves the port number used for the current connection.
     *
     * This method returns the port number that the client is using to communicate
     * with the server. If the client is not connected, the returned value may be 0 or
     * an uninitialized state.
     *
     * @return The port number of the current connection as a 16-bit unsigned integer.
     */
    
    uint16_t Port() const
    {
        SharedLock lock(&mMutex);
        
        return mPort;
    }
    
private:
    
    // Customisable Methods
    
    /**
     * @brief Pure virtual method to handle incoming data from the server.
     *
     * This method is called whenever data is received from the server.
     * Derived classes must implement this function to define how incoming data is processed.
     *
     * @param data A reference to an `iplug::IByteStream` object containing the data received from the server.
     */
    
    virtual void OnDataToClient(const iplug::IByteStream& data) = 0;
    
    /**
     * @brief Callback method invoked when the client connection is closed.
     *
     * This method is called when the client's connection to the server is closed, either
     * intentionally or due to an error. It can be overridden by derived classes to define
     * custom behavior when the connection is terminated.
     *
     * The default implementation does nothing.
     */
    
    virtual void OnCloseClient() {}
    
    /**
     * @brief Casts a generic pointer to a `NetworkClientInterface` pointer.
     *
     * This static method takes a generic void pointer and attempts to cast it
     * to a `NetworkClientInterface` pointer. It is useful for converting raw
     * pointers to the appropriate network client type within the system.
     *
     * @param x A generic pointer (`void*`) that is expected to point to a `NetworkClientInterface` object.
     * @return A pointer to `NetworkClientInterface`, or nullptr if the cast is invalid.
     */
    
    static NetworkClientInterface* AsClient(void *x)
    {
        return reinterpret_cast<NetworkClientInterface *>(x);
    }
    
    // Handlers
    
    /**
     * @brief Handles the closing of the client's connection.
     *
     * This method is responsible for managing the actions that need to be taken
     * when the client's connection to the server is closed. It may involve
     * cleaning up resources, notifying other components, or triggering
     * callbacks related to the disconnection.
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
            mPort = 0;
            lock.Demote();
            OnCloseClient();
            DBGMSG("CLIENT: Disconnected\n");
        }
    }
    
    /**
     * @brief Handles incoming data from the server.
     *
     * This method processes the raw data received from the server. The data is passed as
     * a pointer to a memory buffer (`pData`) and the size of the data in bytes (`size`).
     * It is responsible for interpreting and managing the incoming data appropriately.
     *
     * @param pData A pointer to the raw data buffer received from the server.
     * @param size The size of the data in bytes.
     */
    
    void HandleData(const void *pData, size_t size)
    {
        SharedLock lock(&mMutex);
        
        iplug::IByteStream stream(pData, static_cast<int>(size));
        
        OnDataToClient(stream);
    }
    
    // Static Handlers
    
    /**
     * @brief Static callback function for handling data received from the server.
     *
     * This static method is invoked when data is received on the connection identified
     * by `id`. It processes the raw data passed in `pData` with the given size and allows
     * a user-defined context `x` to be passed along for further handling, typically cast
     * to a relevant object type.
     *
     * @param id The connection identifier (`ConnectionID`) representing the client-server connection.
     * @param pData A pointer to the raw data buffer received from the server.
     * @param size The size of the data in bytes.
     * @param x A generic pointer (`void*`) to user-defined data or context, often used to point to a relevant object.
     */
    
    static void DoDataClient(ConnectionID id, const void *pData, size_t size, void *x)
    {
        AsClient(x)->HandleData(pData, size);
    }
    
    /**
     * @brief Static callback function for handling the closure of a client connection.
     *
     * This static method is called when the connection identified by `id` is closed. It handles
     * the necessary cleanup or further processing after the connection is terminated. The user-defined
     * context `x` can be passed to this function for customized behavior or further object interaction.
     *
     * @param id The connection identifier (`ConnectionID`) representing the client-server connection being closed.
     * @param x A generic pointer (`void*`) to user-defined data or context, often used to point to a relevant object for handling the connection closure.
     */
    
    static void DoCloseClient(ConnectionID id, void *x)
    {
        AsClient(x)->HandleClose();
    }
    
    /**
     * @brief Stores the name or address of the server the client is connected to.
     *
     * This member variable holds the server's name or IP address as a `WDL_String`.
     * It is used to track the server information for the current connection.
     */
    
    WDL_String mServer;
    
    /**
     * @brief Stores the port number used for the current connection.
     *
     * This member variable holds the port number as a 16-bit unsigned integer.
     * It represents the port on the server to which the client is connected.
     */
    
    uint16_t mPort;
    
    /**
     * @brief A mutable mutex used to synchronize access to shared resources.
     *
     * This member variable is a `SharedMutex`, which is used to control concurrent access
     * to shared data in a thread-safe manner. The `mutable` keyword allows the mutex to be
     * modified even in const member functions, ensuring safe access to resources that may
     * need to be locked in read-only operations.
     */
    
    mutable SharedMutex mMutex;
    
    /**
     * @brief A pointer to the client's connection object.
     *
     * This member variable holds a pointer to the connection object of type `T`.
     * It represents the active connection between the client and the server.
     * The type `T` is a template parameter, allowing flexibility in the connection's implementation.
     */
    
    T *mConnection;
};

// Concrete implementation based on the platform

/**
 * @brief Defines a platform-specific alias for the `NetworkClient` type.
 *
 * This block sets up a type alias for `NetworkClient` depending on the operating system:
 * - On Apple platforms (`__APPLE__`), `NetworkClient` is an alias for `NetworkClientInterface<nw_ws_client>`.
 * - On non-Apple platforms, `NetworkClient` is an alias for `NetworkClientInterface<cw_ws_client>`.
 *
 * This allows the code to use different implementations of the network client depending on the platform.
 */

#ifdef __APPLE__
using NetworkClient = NetworkClientInterface<nw_ws_client>;
#else
using NetworkClient = NetworkClientInterface<cw_ws_client>;
#endif

#endif /* NETWORKUTILITIES_HPP */

