
/**
 * @file NetworkData.hpp
 * @brief Defines classes and methods for handling network byte streams and data chunks.
 *
 * This file contains the definition of the NetworkByteChunk and NetworkByteStream classes,
 * which are used to manage and manipulate byte data for network transmission and reception.
 * It includes methods for adding, retrieving, and processing chunks of data, as well as utilities
 * for navigating through byte streams. The classes and methods in this file facilitate efficient
 * handling of network-specific byte operations, including serialization, deserialization, and
 * position tracking within byte streams.
 */

#ifndef NETWORKDATA_HPP
#define NETWORKDATA_HPP

#include <wdlstring.h>

#include "IPlugStructs.h"

// A wrapper for iplug::IByteChunk that can be constructued with its contents
// Multiple items can also be added at a time later

/**
 * @brief A structure representing a chunk of bytes for network operations.
 *
 * This structure extends the functionality of the iplug::IByteChunk class to handle network-specific byte operations.
 * It may be used to store, manage, and manipulate data that is transmitted over the network.
 *
 * @see iplug::IByteChunk
 */

struct NetworkByteChunk : public iplug::IByteChunk
{
public:

    /**
     * @brief Templated constructor for the NetworkByteChunk structure.
     *
     * This constructor allows the creation of a NetworkByteChunk object by passing a variable number of arguments.
     * The arguments are forwarded to the base class constructor or used for internal initialization, enabling flexible
     * instantiation based on different types and quantities of arguments.
     *
     * @tparam Args Variadic template parameters representing the types of the arguments passed to the constructor.
     * @param args A variable number of arguments used to initialize the NetworkByteChunk.
     */
    
    template <typename ...Args>
    NetworkByteChunk(const Args& ...args)
    {
        Add(std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Adds a given string to the internal storage.
     *
     * This method adds the content of the provided string to the internal data structure or concatenates it to the existing string data.
     *
     * @param str A constant reference to a WDL_String object representing the string to be added.
     */
    
    inline void Add(const WDL_String& str)
    {
        PutStr(str.Get());
    }
    
    /**
     * @brief Adds a C-style string to the internal storage.
     *
     * This method appends or inserts the given null-terminated C-string to the internal data structure.
     *
     * @param str A pointer to a null-terminated C-string to be added.
     */
    
    inline void Add(const char* str)
    {
        PutStr(str);
    }
    
    /**
     * @brief Adds a byte chunk to the internal storage.
     *
     * This method appends or incorporates the data from the provided IByteChunk object into the current byte structure.
     * It is typically used to handle or concatenate chunks of data for network transmission or storage.
     *
     * @param chunk A constant reference to an iplug::IByteChunk object representing the byte chunk to be added.
     */
    
    inline void Add(const iplug::IByteChunk& chunk)
    {
        PutChunk(&chunk);
    }
    
    /**
     * @brief Adds a network byte chunk to the internal storage.
     *
     * This method appends or merges the data from the provided NetworkByteChunk object into the current byte structure.
     * It is typically used for handling and managing network-specific data chunks during transmission or reception.
     *
     * @param chunk A constant reference to a NetworkByteChunk object representing the network byte chunk to be added.
     */
    
    inline void Add(const NetworkByteChunk& chunk)
    {
        PutChunk(&chunk);
    }
    
    /**
     * @brief Adds a value of any type to the internal storage.
     *
     * This method allows the addition of data of any type, provided it is compatible with the internal storage mechanism.
     * The data is typically serialized or converted to a byte representation and added to the current structure.
     *
     * @tparam T The type of the value to be added. This can be any type that supports serialization or conversion to bytes.
     * @param value A constant reference to the value of type T to be added.
     */
    
    template <typename T>
    inline void Add(const T& value)
    {
        Put(&value);
    }
    
    /**
     * @brief Adds a series of values to the internal storage.
     *
     * This method allows the addition of a sequence of values, where the first value is processed immediately and
     * the remaining values are processed recursively. Each value is typically serialized or converted to a byte
     * representation and added to the current structure.
     *
     * @tparam First The type of the first value to be added.
     * @tparam Args Variadic template parameters representing the types of the additional values to be added.
     * @param value A constant reference to the first value to be added.
     * @param args A variable number of additional values to be added.
     */
    
    template <typename First, typename ...Args>
    inline void Add(const First& value, const Args& ...args)
    {
        Add(value);
        Add(std::forward<const Args>(args)...);
    }
    
};

// A wrapper for iplug::IByteStream that tracks its own position

/**
 * @brief A class for handling byte streams in network operations.
 *
 * The NetworkByteStream class is designed to manage the reading, writing, and transmission of byte streams
 * over a network. It provides methods for manipulating and processing raw byte data for network communication.
 * This class may handle tasks such as serialization, deserialization, and buffering of data for network protocols.
 */

class NetworkByteStream
{
public:
    
    /**
     * @brief Constructs a NetworkByteStream object with a given byte stream and starting position.
     *
     * This constructor initializes the NetworkByteStream with a reference to an IByteStream object,
     * allowing the management of byte data starting from a specified position.
     * It is useful for initializing the stream and setting the read/write position for subsequent operations.
     *
     * @param stream A constant reference to an iplug::IByteStream object representing the byte stream to be used.
     * @param startPos An optional starting position within the stream. Defaults to 0 if not specified.
     */
    
    NetworkByteStream(const iplug::IByteStream& stream, int startPos = 0)
    : mStream(stream)
    , mPos(startPos)
    {}
    
    /**
     * @brief Returns the current position in the byte stream.
     *
     * This method retrieves the current position in the stream, indicating where the next read or write operation
     * will occur. It is useful for tracking the progress of data manipulation within the stream.
     *
     * @return The current position in the byte stream as an integer.
     */
    
    inline int Tell() const
    {
        return mPos;
    }
    
    /**
     * @brief Sets the current position in the byte stream.
     *
     * This method updates the current position within the byte stream to the specified value.
     * It is typically used to jump to a specific location in the stream for subsequent read or write operations.
     *
     * @param pos The new position in the byte stream as an integer.
     */
    
    inline void Seek(int pos)
    {
        mPos = pos;
    }
    
    /**
     * @brief Retrieves a value of the specified type from the byte stream.
     *
     * This method extracts data from the byte stream and stores it in the provided variable.
     * The method assumes the byte data can be interpreted as the type specified by the template parameter.
     *
     * @tparam T The type of the value to be retrieved from the byte stream.
     * @param value A reference to a variable where the extracted value will be stored.
     */
    
    template <class T>
    inline void Get(T& value)
    {
        mPos = mStream.Get(&value, mPos);
    }
    
    /**
     * @brief Retrieves a WDL_String object from the byte stream.
     *
     * This method extracts data from the byte stream and stores it in the provided WDL_String object.
     * It interprets the byte data as a string and converts it into a WDL_String format.
     *
     * @param str A reference to a WDL_String object where the extracted string data will be stored.
     */
    
    inline void Get(WDL_String& str)
    {
        mPos = mStream.GetStr(str, mPos);
    }
    
    /**
     * @brief Retrieves a series of values from the byte stream.
     *
     * This method extracts multiple values from the byte stream in sequence, starting with the first value and
     * then recursively retrieving the remaining values. Each value is interpreted according to its type and
     * stored in the corresponding variable.
     *
     * @tparam First The type of the first value to be retrieved from the byte stream.
     * @tparam Args Variadic template parameters representing the types of additional values to be retrieved.
     * @param value A reference to the first variable where the extracted data will be stored.
     * @param args A variable number of references to additional variables for storing the subsequent extracted values.
     */
    
    template <class First, class ...Args>
    inline void Get(First& value, Args& ...args)
    {
        Get(value);
        Get(args...);
    }
    
    // Look to see if the next item is a tag matching the input
    // Advance if the tag is matched
    
    /**
     * @brief Checks if the next sequence in the byte stream matches the specified tag.
     *
     * This method compares the upcoming data in the byte stream with the provided tag (a C-style string).
     * It returns true if the next portion of the stream matches the given tag, otherwise false.
     * This is typically used for identifying specific markers or tags within the stream.
     *
     * @param tag A pointer to a null-terminated C-string representing the tag to be checked.
     * @return True if the next portion of the byte stream matches the tag, otherwise false.
     */
    
    bool IsNextTag(const char* tag)
    {
        WDL_String nextTag;
        
        int pos = mStream.GetStr(nextTag, mPos);
                
        if (strcmp(nextTag.Get(), tag) == 0)
        {
            mPos = pos;
            return true;
        }
        
        return false;
    }
    
private:
    
    /**
     * @brief A reference to the byte stream being managed.
     *
     * This member stores a constant reference to an iplug::IByteStream object, which represents the byte stream
     * that the NetworkByteStream operates on. It is used for reading from or writing to the underlying byte data.
     */
    
    const iplug::IByteStream& mStream;
    
    /**
     * @brief The current position within the byte stream.
     *
     * This member variable keeps track of the current position in the byte stream, indicating where the next
     * read or write operation will occur. It is updated as data is processed within the stream.
     */
    
    int mPos;
};

#endif /* NETWORKDATA_HPP */
