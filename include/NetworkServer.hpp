
/**
 * @file NetworkServer.hpp
 * @brief Defines the `NetworkServerInterface` template class and related server functionality.
 *
 * This file contains the declaration and implementation of the `NetworkServerInterface` class template,
 * which provides a generic WebSocket server interface for handling client connections, data transfer,
 * and server lifecycle management. The server interface can be specialized with different server types
 * depending on the platform. It also includes platform-specific type definitions for `NetworkServer`
 * to ensure compatibility with both Apple and non-Apple systems.
 *
 * Key features include:
 * - Server start and stop functionality.
 * - Handling client connections, data, and disconnections.
 * - Platform-specific typedefs for the `NetworkServer`.
 *
 * @note This file relies on platform-specific WebSocket server implementations.
 *
 * @see NetworkTypes.hpp
 * @see IPlugLogger.h
 * @see IPlugStructs.h
 */

#ifndef NETWORKSERVER_HPP
#define NETWORKSERVER_HPP

#include <memory>
#include <string>

#include "IPlugLogger.h"
#include "IPlugStructs.h"

#include "NetworkTypes.hpp"

// A generic web socket network server that requires an interface for the specifics of the networking

/**
 * @brief A generic WebSocket network server interface.
 *
 * This template class provides an interface for a WebSocket server that utilizes
 * a custom type for server creation and handling. It extends the functionality of
 * the `NetworkTypes` class and manages the lifecycle and operations of the server,
 * including starting, stopping, and handling clients.
 *
 * @tparam T The type that defines the specific implementation details for the server.
 */

template <class T>
class NetworkServerInterface : protected NetworkTypes
{
public:
    
    /**
     * @brief Constructs a new NetworkServerInterface object.
     *
     * This constructor initializes a new instance of the `NetworkServerInterface` class
     * and sets the server pointer to `nullptr`, indicating that the server is not yet started.
     */
    
    NetworkServerInterface() : mServer(nullptr)  {}
    
    /**
     * @brief Destroys the NetworkServerInterface object.
     *
     * This virtual destructor ensures proper cleanup of the `NetworkServerInterface` object
     * and its resources when the object is destroyed. It allows for derived classes to
     * handle their own specific cleanup procedures if necessary.
     */
    
    virtual ~NetworkServerInterface() {}
    
    /**
     * @brief Deleted copy constructor.
     *
     * This prevents copying of `NetworkServerInterface` instances.
     * Copying is disabled to avoid unintended behavior or resource duplication,
     * ensuring that each instance manages its own server resources independently.
     *
     * @param other The other `NetworkServerInterface` instance (unused).
     */
    
    NetworkServerInterface(const NetworkServerInterface&) = delete;
    
    /**
     * @brief Deleted copy assignment operator.
     *
     * This prevents assignment of one `NetworkServerInterface` instance to another.
     * Assignment is disabled to avoid duplication of server resources and to ensure
     * that each instance manages its own resources independently.
     *
     * @param other The other `NetworkServerInterface` instance (unused).
     * @return A reference to this instance (unused).
     */
    
    NetworkServerInterface& operator=(const NetworkServerInterface&) = delete;
    
    /**
     * @brief Starts the server on the specified port.
     *
     * This method initializes the server and begins listening for incoming connections
     * on the provided port number. It wraps the port number into a string and
     * calls the overloaded `StartServer(const char* port)` method.
     *
     * @param port The port number on which the server will listen for incoming connections.
     */
    
    void StartServer(uint16_t port)
    {
        StartServer(std::to_string(port).c_str());
    }
    
    /**
     * @brief Starts the server on the specified port (as a string).
     *
     * This method initializes the WebSocket server and begins listening for incoming
     * connections on the provided port, specified as a string. It sets up the server
     * with handlers for connection, readiness, data reception, and closure events.
     * If the server is already running, a message will be logged instead.
     *
     * @param port The port number as a string on which the server will listen for connections.
     */
    
    void StartServer(const char* port)
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
     * @brief Stops the currently running server.
     *
     * This method shuts down the WebSocket server if it is currently running.
     * It releases the server's resources, destroys the server instance, and logs a
     * message indicating that the server has been stopped. If no server is running,
     * no action is taken.
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
     * @brief Retrieves the number of connected clients.
     *
     * This method returns the current number of clients connected to the server.
     * It provides a thread-safe way to access the client count by utilizing a shared lock.
     *
     * @return The number of connected clients as an integer.
     */
    
    int NClients() const
    {
        SharedLock lock(&mMutex);
        
        return mServer ? static_cast<int>(mServer->size()) : 0;
    }
    
    /**
     * @brief Sends data to a specific client.
     *
     * This method transmits the given byte chunk of data to the client identified by the
     * provided WebSocket connection ID. It ensures that the data is sent to the correct
     * client based on the connection ID.
     *
     * @param id The WebSocket connection ID that uniquely identifies the target client.
     * @param chunk The data to be sent, encapsulated in an `iplug::IByteChunk` object.
     *
     * @return `true` if the data was successfully sent, `false` otherwise.
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
     * @brief Sends data from the server to all connected clients.
     *
     * This method broadcasts the given byte chunk of data to all clients currently connected
     * to the server. It ensures that the data is sent to every active WebSocket connection.
     *
     * @param chunk The data to be sent, encapsulated in an `iplug::IByteChunk` object.
     *
     * @return `true` if the data was successfully sent to all clients, `false` otherwise.
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
     * @brief Checks if the server is currently connected.
     *
     * This method returns whether the server is currently running and connected,
     * indicating that it is capable of handling client connections.
     *
     * @return `true` if the server is connected and running, `false` otherwise.
     */
    
    bool IsServerConnected() const
    {
        SharedLock lock(&mMutex);
        
        return NClients();
    }
    
    /**
     * @brief Checks if the server is currently running.
     *
     * This method returns whether the server is actively running. It can be used
     * to determine if the server has been started and is operational, regardless
     * of whether it has active client connections.
     *
     * @return `true` if the server is running, `false` otherwise.
     */
    
    bool IsServerRunning() const
    {
        SharedLock lock(&mMutex);
        
        return mServer;
    }
    
private:
    
    // Customisable Handlers
    
    /**
     * @brief Callback invoked when the server is ready.
     *
     * This virtual method is called when the server is ready to handle client connections.
     * It can be overridden in derived classes to implement specific behavior when the server
     * reaches a ready state.
     *
     * @param id The connection ID associated with the server readiness event.
     */
    
    virtual void OnServerReady(ConnectionID id) {}
    
    /**
     * @brief Callback invoked when the server disconnects.
     *
     * This virtual method is called when the server disconnects from a client or shuts down.
     * It can be overridden in derived classes to implement custom behavior when a disconnection
     * event occurs.
     *
     * @param id The connection ID associated with the disconnection event.
     */
    
    virtual void OnServerDisconnect(ConnectionID id) {}
    
    /**
     * @brief Pure virtual callback invoked when data is received by the server from a client.
     *
     * This method must be implemented by derived classes to handle data sent by a client
     * to the server. It is called whenever the server receives a data packet from a client.
     *
     * @param id The connection ID of the client that sent the data.
     * @param data The data received from the client, encapsulated in an `iplug::IByteStream` object.
     */
    
    virtual void OnDataToServer(ConnectionID id, const iplug::IByteStream& data) = 0;
    
    // Handlers
    
    /**
     * @brief Handles a new socket connection.
     *
     * This method is called when a new socket connection is established with the server.
     * It manages the connection identified by the provided `ConnectionID`. Specific handling
     * behavior for the new connection can be implemented within this method.
     *
     * @param id The connection ID representing the newly established socket connection.
     */
    
    void HandleSocketConnection(ConnectionID id)
    {
        SharedLock lock(&mMutex);
        
        DBGMSG("SERVER: Connected\n");
    }
    
    /**
     * @brief Handles a socket that is ready for communication.
     *
     * This method is called when a socket connection becomes ready to send and receive data.
     * It manages the connection identified by the provided `ConnectionID` and can be used
     * to perform any setup required when the socket is ready for data transmission.
     *
     * @param id The connection ID of the socket that is ready for communication.
     */
    
    void HandleSocketReady(ConnectionID id)
    {
        SharedLock lock(&mMutex);
                
        DBGMSG("SERVER: New connection - num clients %i\n", NClients());
        
        OnServerReady(id);
    }
    
    /**
     * @brief Handles incoming data from a socket connection.
     *
     * This method is called when data is received from a client socket. It processes the
     * data received from the connection identified by the provided `ConnectionID`. The
     * data is passed as a pointer along with its size for further handling or processing.
     *
     * @param id The connection ID of the client that sent the data.
     * @param pData A pointer to the data received from the client.
     * @param size The size of the data in bytes.
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
     * @brief Handles the closing of a socket connection.
     *
     * This method is called when a client socket connection is closed. It manages the
     * disconnection event for the client identified by the provided `ConnectionID`.
     * Any cleanup or resource release related to the closed connection can be performed here.
     *
     * @param id The connection ID of the client whose socket has been closed.
     */
    
    void HandleSocketClose(ConnectionID id)
    {
        SharedLock lock(&mMutex);
                
        DBGMSG("SERVER: Closed connection - num clients %i\n", NClients());
        
        OnServerDisconnect(id);
    }
    
    // Static Handlers
    
    /**
     * @brief Static callback for handling a new server connection.
     *
     * This static method is called when a new client connection is made to the server.
     * It manages the connection using the provided `ConnectionID` and the untyped server
     * pointer, which allows the method to interact with the server instance.
     *
     * @param id The connection ID of the newly connected client.
     * @param pUntypedServer A void pointer to the server instance, allowing the callback to interact with it.
     */
    
    static void DoConnectServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        // N.B. Return values are 0 for success and 1 for close
        
        if (pServer)
            pServer->HandleSocketConnection(id);
    }
    
    /**
     * @brief Static callback for handling when a server connection is ready.
     *
     * This static method is called when a client connection to the server becomes ready
     * for communication. It manages the ready state using the provided `ConnectionID`
     * and the untyped server pointer, which allows interaction with the server instance.
     *
     * @param id The connection ID of the client that is ready for communication.
     * @param pUntypedServer A void pointer to the server instance, allowing the callback to interact with it.
     */
    
    static void DoReadyServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        if (pServer)
            pServer->HandleSocketReady(id);
    }
    
    /**
     * @brief Static callback for handling incoming data from a client.
     *
     * This static method is called when the server receives data from a connected client.
     * It processes the data using the provided `ConnectionID` to identify the client,
     * and the data is passed as a pointer along with its size. The method also takes
     * an untyped server pointer to interact with the server instance.
     *
     * @param id The connection ID of the client that sent the data.
     * @param pData A pointer to the data received from the client.
     * @param size The size of the data in bytes.
     * @param pUntypedServer A void pointer to the server instance, allowing the callback to interact with it.
     */
    
    static void DoDataServer(ConnectionID id, const void* pData, size_t size, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);

        if (pServer)
            pServer->HandleSocketData(id, pData, size);
    }
    
    /**
     * @brief Static callback for handling the closure of a client connection.
     *
     * This static method is called when a client disconnects from the server.
     * It handles the disconnection using the provided `ConnectionID` to identify
     * the client, and it takes an untyped server pointer to perform any necessary
     * cleanup or resource management related to the server instance.
     *
     * @param id The connection ID of the client that has disconnected.
     * @param pUntypedServer A void pointer to the server instance, allowing the callback to interact with it.
     */
    
    static void DoCloseServer(ConnectionID id, void *pUntypedServer)
    {
        auto pServer = AsServer(pUntypedServer);
        
        if (pServer)
            pServer->HandleSocketClose(id);
    }
    
    /**
     * @brief Casts an untyped server pointer to a `NetworkServerInterface` pointer.
     *
     * This static method converts an untyped (void) server pointer into a typed
     * `NetworkServerInterface` pointer. It is used to safely retrieve the actual
     * server instance from an untyped context.
     *
     * @param pUntypedServer A void pointer representing the untyped server instance.
     * @return A pointer to the `NetworkServerInterface` instance.
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
     * @brief Pointer to the server instance.
     *
     * This member variable holds a pointer to the server instance of type `T`.
     * It manages the server's lifecycle and operations, including starting, stopping,
     * and handling client connections.
     */
    
    T *mServer;

protected:
    
    /**
     * @brief A mutable shared mutex for thread-safe access.
     *
     * This member variable provides a mutex that allows multiple threads to safely
     * access shared resources within the `NetworkServerInterface`. The `mutable`
     * keyword allows this mutex to be modified even in `const` methods, ensuring
     * thread-safe read and write operations.
     */
    
    mutable SharedMutex mMutex;
};

// Concrete implementations based on the platform

/**
 * @brief Platform-specific typedef for the `NetworkServer` type.
 *
 * This block defines the `NetworkServer` type based on the platform being compiled.
 * On Apple platforms (`__APPLE__`), `NetworkServer` is an alias for
 * `NetworkServerInterface<nw_ws_server>`, while on other platforms, it is an alias for
 * `NetworkServerInterface<cw_ws_server>`. This ensures that the correct WebSocket
 * server implementation is used depending on the platform.
 *
 * @note The use of `#ifdef` ensures platform-specific compatibility by selecting the appropriate
 * WebSocket server type.
 */

#ifdef __APPLE__
using NetworkServer = NetworkServerInterface<nw_ws_server>;
#else
using NetworkServer = NetworkServerInterface<cw_ws_server>;
#endif

#endif /* NETWORKSERVER_HPP */
