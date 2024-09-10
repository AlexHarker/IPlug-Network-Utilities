
/**
 * @file AutoServer.hpp
 * @brief Defines the AutoServer class, which integrates server and client functionalities, along with utility classes for network communication.
 *
 * This file contains the definition of the AutoServer class, which inherits from both NetworkServer and NetworkClient to handle
 * network communication, server discovery, and peer-to-peer connections. It includes support for managing connection data,
 * broadcasting messages, and handling network discovery using Bonjour and the DiscoverablePeer class.
 *
 * The file also defines helper classes like NextServer, which manages host, port, and timeout information for the next server connection.
 */

#ifndef AUTOSERVER_HPP
#define AUTOSERVER_HPP

#include <chrono>
#include <cstring>
#include <utility>

#include "DiscoverablePeer.hpp"
#include "NetworkData.hpp"
#include "NetworkTiming.hpp"
#include "NetworkUtilities.hpp"

/**
 * @class AutoServer
 * @brief A server class that integrates both server and client functionalities, inheriting from NetworkServer and NetworkClient.
 *
 * The AutoServer class manages network connections by acting as both a server and a client. It offers discovery of peers,
 * managing server/client interactions, and handles network timeouts using an internal NextServer class.
 *
 * @note This class inherits from both NetworkServer and NetworkClient, allowing it to operate in both roles simultaneously.
 */

class AutoServer : public NetworkServer, NetworkClient
{
    
    /**
     * @class NextServer
     * @brief A utility class within AutoServer to manage the next server connection details.
     *
     * The NextServer class is responsible for storing and retrieving the next server's host and port information.
     * It also manages the timeout logic to ensure server discovery actions are performed within a specific time frame.
     *
     * This class helps AutoServer track the next potential server for communication during network discovery.
     */
    
    class NextServer
    {
    public:
        
        /**
         * @brief Sets the next server's host and port information.
         *
         * This method updates the internal host and port variables for the next server,
         * and also starts the timeout timer to track the duration for which this information is valid.
         *
         * @param host The hostname of the next server as a C-string.
         * @param port The port number of the next server.
         */
        
        void Set(const char *host, uint32_t port)
        {
            mHost.Set(host);
            mPort = port;
            mTimeOut.Start();
        }
        
        /**
         * @brief Retrieves the next server's host and port information.
         *
         * This method returns the host and port of the next server as a pair.
         * If the timeout interval exceeds 4 seconds, it returns an empty host and a port of 0.
         * Otherwise, it returns the currently stored host and port.
         *
         * @return A `std::pair` containing the host (`WDL_String`) and port (`uint32_t`).
         * If the timeout exceeds 4 seconds, the host will be empty and the port will be 0.
         */
        
        std::pair<WDL_String, uint32_t> Get()
        {
            if (mTimeOut.Interval() > 4)
                return { WDL_String(), 0 };
            else
                return { mHost, mPort };
        }
        
    private:
        
        /**
         * @brief The hostname of the next server.
         *
         * This member variable stores the hostname of the next server as a `WDL_String`.
         * It is updated by the `Set` method and used by the `Get` method to return the host information.
         */
        
        WDL_String mHost;
        
        /**
         * @brief The port number of the next server.
         *
         * This member variable stores the port number of the next server as a `uint32_t`.
         * It is set by the `Set` method and used by the `Get` method to return the port information.
         */
        
        uint32_t mPort;
        
        /**
         * @brief Timer used to track the timeout for the next server connection.
         *
         * This member variable is an instance of `CPUTimer` and is used to track the time elapsed since
         * the next server's host and port were set. It helps determine if the information is still valid
         * based on a predefined interval (e.g., 4 seconds).
         */
        
        CPUTimer mTimeOut;
    };
    
public:
    
    /**
     * @brief Destructor for the AutoServer class.
     *
     * This destructor ensures that when an instance of AutoServer is destroyed,
     * the discoverable peer is stopped and the server is properly shut down.
     * It ensures proper cleanup of resources related to the server and client connections.
     */
    
    ~AutoServer()
    {
        mDiscoverable.Stop();
        StopServer();
    }
    
    /**
     * @brief Initiates the discovery process for finding available servers.
     *
     * This method triggers the discovery process, allowing the AutoServer to search for available servers
     * on the network. If a client is already connected, the discovery process is skipped.
     *
     * @note This method is a no-op if the client is already connected to a server.
     */
    
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
    
    /**
     * @brief Retrieves the name of the server.
     *
     * This method fills the provided `WDL_String` reference with the name of the server.
     * It is used to obtain the current server's name as part of the server information.
     *
     * @param str A reference to a `WDL_String` object that will be populated with the server name.
     */
    
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
    
    /**
     * @brief Retrieves the names of connected peers.
     *
     * This method fills the provided `WDL_String` reference with the names of all currently connected peers.
     * It is used to obtain a list or representation of the peers that are connected to the server.
     *
     * @param peersNames A reference to a `WDL_String` object that will be populated with the names of connected peers.
     */
    
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
    
    /**
     * @brief Sends data to a specific client using a WebSocket connection.
     *
     * This templated method allows sending a variable number of arguments (`Args`) to a client identified by its WebSocket connection ID.
     * The arguments can be of various types and are forwarded to the client.
     *
     * @tparam Args The types of the arguments to send. This allows for flexible and varied data to be sent.
     * @param id The WebSocket connection ID of the client to which the data is sent.
     * @param args The data or arguments to be sent to the client. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendToClient(ws_connection_id id, const Args& ...args)
    {
        SendTaggedToClient(GetDataTag(), id, std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends data from the server to all connected clients.
     *
     * This templated method allows the server to send a variable number of arguments (`Args`) to all connected clients.
     * The arguments can be of various types, making this method flexible for sending different types of data to clients.
     *
     * @tparam Args The types of the arguments to send. This allows for flexible and varied data to be sent from the server.
     * @param args The data or arguments to be sent to all clients. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendFromServer(const Args& ...args)
    {
        SendTaggedFromServer(GetDataTag(), std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends data from the client to the server.
     *
     * This templated method allows the client to send a variable number of arguments (`Args`) to the server.
     * The arguments can be of various types, making this method flexible for sending different types of data from the client to the server.
     *
     * @tparam Args The types of the arguments to send. This allows for flexible and varied data to be sent from the client.
     * @param args The data or arguments to be sent to the server. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendFromClient(const Args& ...args)
    {
        SendTaggedFromClient(GetDataTag(), std::forward<const Args>(args)...);
    }

private:
    
    /**
     * @brief Waits for the server or client to stop operations.
     *
     * This method blocks the current thread until the server or client has completely stopped its operations.
     * It ensures that any ongoing tasks or connections are gracefully terminated before returning control.
     */
    
    void WaitToStop()
    {
        std::chrono::duration<double, std::milli> ms(500);
        std::this_thread::sleep_for(ms);
    }
    
    /**
     * @brief Retrieves the connection tag for identifying the server or client.
     *
     * This `constexpr` static method returns a constant C-string that serves as a connection tag,
     * which can be used to identify the server or client during network communication.
     * The tag is likely a unique identifier or label associated with the connection.
     *
     * @return A constant C-string representing the connection tag.
     */
    
    constexpr static const char *GetConnectionTag()
    {
        return "~";
    }
    
    /**
     * @brief Retrieves the data tag used to identify data in network communication.
     *
     * This `constexpr` static method returns a constant C-string that represents a data tag,
     * which is used to label or identify specific data being transmitted in network communications.
     *
     * @return A constant C-string representing the data tag.
     */
    
    constexpr static const char *GetDataTag()
    {
        return "-";
    }
    
    /**
     * @brief Sends connection-specific data from the server to a specified client.
     *
     * This templated method allows the server to send a variable number of arguments (`Args`) to a client,
     * identified by its WebSocket connection ID. The data being sent is related to the connection and can
     * include various types of information depending on the arguments provided.
     *
     * @tparam Args The types of the arguments to send. This allows flexibility in the type and number of data items to be sent.
     * @param id The WebSocket connection ID of the client to which the connection data is being sent.
     * @param args The connection data or arguments to send to the client. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendConnectionDataFromServer(ws_connection_id id, const Args& ...args)
    {
        SendTaggedToClient(GetConnectionTag(), id, std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends connection-specific data from the server to all connected clients.
     *
     * This templated method allows the server to broadcast a variable number of arguments (`Args`) to all connected clients.
     * The data being sent is related to the connection and can include various types of information depending on the arguments provided.
     *
     * @tparam Args The types of the arguments to send. This allows flexibility in the type and number of data items to be sent.
     * @param args The connection data or arguments to send to all clients. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendConnectionDataFromServer(const Args& ...args)
    {
        SendTaggedFromServer(GetConnectionTag(), std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Sends connection-specific data from the client to the server.
     *
     * This templated method allows the client to send a variable number of arguments (`Args`) to the server.
     * The data being sent is related to the connection and can include various types of information depending on the arguments provided.
     *
     * @tparam Args The types of the arguments to send. This allows flexibility in the type and number of data items to be sent.
     * @param args The connection data or arguments to send to the server. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendConnectionDataFromClient(const Args& ...args)
    {
        SendTaggedFromClient(GetConnectionTag(), std::forward<const Args>(args)...);
    }

    /**
     * @brief Sends tagged data to a specific client identified by a WebSocket connection ID.
     *
     * This templated method allows the server to send a variable number of arguments (`Args`) to a client,
     * identified by its WebSocket connection ID. The data is associated with a specific tag, which provides
     * a label or identifier for the transmitted data.
     *
     * @tparam Args The types of the arguments to send. This allows flexibility in the type and number of data items.
     * @param tag A constant C-string representing the tag associated with the data being sent.
     * @param id The WebSocket connection ID of the client to which the tagged data is being sent.
     * @param args The data or arguments to send to the client. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendTaggedToClient(const char *tag, ws_connection_id id, const Args& ...args)
    {
        SendDataToClient(id, NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    /**
     * @brief Sends tagged data from the server to all connected clients.
     *
     * This templated method allows the server to broadcast a variable number of arguments (`Args`) to all connected clients.
     * The data is associated with a specific tag, which serves as a label or identifier for the transmitted data.
     *
     * @tparam Args The types of the arguments to send. This allows flexibility in the type and number of data items.
     * @param tag A constant C-string representing the tag associated with the data being sent.
     * @param args The data or arguments to send to all clients. The number and type of arguments can vary.
     */
    
    template <class ...Args>
    void SendTaggedFromServer(const char *tag, const Args& ...args)
    {
        SendDataFromServer(NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    /**
     * @brief Sends tagged data from the client to the server.
     *
     * This templated method allows the client to send a variable number of arguments (`Args`) to the server,
     * with the data being associated with a specific tag. The tag serves as an identifier for the transmitted data,
     * and the data is packaged into a `NetworkByteChunk` before being sent to the server.
     *
     * @tparam Args The types of the arguments to send. This allows flexibility in the type and number of data items.
     * @param tag A constant C-string representing the tag associated with the data being sent.
     * @param args The data or arguments to send to the server. These are forwarded and packaged in a `NetworkByteChunk`.
     */
    
    template <class ...Args>
    void SendTaggedFromClient(const char *tag, const Args& ...args)
    {
        SendDataFromClient(NetworkByteChunk(tag, std::forward<const Args>(args)...));
    }
    
    /**
     * @brief Attempts to establish a connection to a server using the specified host and port.
     *
     * This method tries to connect to a server at the given host and port. It returns a boolean value indicating
     * whether the connection was successful or not.
     *
     * @param host The hostname or IP address of the server as a C-string.
     * @param port The port number of the server to connect to.
     * @return `true` if the connection is successfully established, `false` otherwise.
     */
    
    bool TryConnect(const char *host, uint32_t port)
    {
        if (Connect(host, port))
        {
            SendConnectionDataFromServer("SwitchServer", host, port);
            
            WaitToStop();
            mDiscoverable.Stop();
            StopServer();
            
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Handles incoming connection data from a client and processes it on the server.
     *
     * This method processes the data received from a client through the provided `NetworkByteStream`.
     * It is intended to handle and interpret connection-specific data that is sent from the client to the server.
     *
     * @param stream A reference to a `NetworkByteStream` object that contains the data sent by the client.
     */
    
    void HandleConnectionDataToServer(NetworkByteStream& stream) {}
    
    /**
     * @brief Handles incoming connection data from the server and processes it on the client.
     *
     * This method processes the data received from the server through the provided `NetworkByteStream`.
     * It is intended to handle and interpret connection-specific data that is sent from the server to the client.
     *
     * @param stream A reference to a `NetworkByteStream` object that contains the data sent by the server.
     */
    
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
    
    /**
     * @brief Handles incoming data sent from a client to the server.
     *
     * This method is called when data is received from a client. It processes the data identified by the `ConnectionID`
     * and uses the provided `IByteStream` object to access the raw data. The method is marked as `final`, indicating that
     * it cannot be overridden in derived classes.
     *
     * @param id The connection ID representing the client that sent the data.
     * @param data A reference to an `iplug::IByteStream` object containing the data sent from the client.
     */
    
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
    
    /**
     * @brief Handles incoming data sent from the server to the client.
     *
     * This method is called when data is received from the server. It processes the data using the provided
     * `IByteStream` object, which contains the raw data sent by the server. The method is marked as `final`,
     * indicating that it cannot be overridden in derived classes.
     *
     * @param data A reference to an `iplug::IByteStream` object containing the data sent from the server.
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
            DBGMSG("Unknown network message to client");
        }
    }
    
    /**
     * @brief Virtual method to handle data received by the server from a client.
     *
     * This method is intended to be overridden in derived classes to process data sent from a specific client
     * to the server. The `ConnectionID` identifies the client, and the `NetworkByteStream` contains the data
     * sent by the client.
     *
     * @param id The connection ID representing the client that sent the data.
     * @param data A reference to a `NetworkByteStream` object containing the data sent from the client.
     */
    
    virtual void ReceiveAsServer(ConnectionID id, NetworkByteStream& data) {}
    
    /**
     * @brief Virtual method to handle data received by the client from the server.
     *
     * This method is intended to be overridden in derived classes to process data sent from the server
     * to the client. The `NetworkByteStream` object contains the data transmitted by the server.
     *
     * @param data A reference to a `NetworkByteStream` object containing the data sent from the server.
     */
    
    virtual void ReceiveAsClient(NetworkByteStream& data) {}

    /**
     * @brief An instance of the NextServer class used to manage the next server's connection details.
     *
     * This member variable holds an instance of the `NextServer` class, which manages the host, port,
     * and timeout information for the next potential server that the AutoServer may connect to or interact with.
     */
    
    NextServer mNextServer;
    
    /**
     * @brief A timer used to manage the restart intervals for the Bonjour service.
     *
     * This member variable is an instance of `CPUTimer`, and it tracks the time intervals for restarting
     * the Bonjour service, which is used for network service discovery in AutoServer.
     */
    
    CPUTimer mBonjourRestart;
    
    /**
     * @brief An instance of the DiscoverablePeer class used for network service discovery.
     *
     * This member variable holds an instance of `DiscoverablePeer`, which enables the AutoServer
     * to advertise itself and discover other peers on the network. It is used for managing
     * peer-to-peer network discovery.
     */
    
    DiscoverablePeer mDiscoverable;
};

#endif /* AUTOSERVER_HPP */
