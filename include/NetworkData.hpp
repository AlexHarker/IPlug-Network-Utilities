
/**
 * @file NetworkData.hpp
 * @brief This file contains the definition of the NetworkByteChunk and NetworkByteStream classes.
 *        These classes provide functionality for manipulating and streaming byte data in a network context,
 *        allowing for the addition and retrieval of various data types to/from byte chunks.
 *
 *        - NetworkByteChunk: A wrapper for iplug::IByteChunk that allows construction with multiple items.
 *        - NetworkByteStream: A class for managing the streaming of byte data, supporting reading from and writing to byte streams.
 */

#ifndef NETWORKDATA_HPP
#define NETWORKDATA_HPP

#include <wdlstring.h>

#include "IPlugStructs.h"

// A wrapper for iplug::IByteChunk that can be constructued with its contents
// Multiple items can also be added at a time later

/**
 * @brief A struct that extends the iplug::IByteChunk class to allow construction
 *        with its contents and the ability to add multiple items later.
 */

struct NetworkByteChunk : public iplug::IByteChunk
{
public:

    /**
     * @brief Constructor that allows initialization of the NetworkByteChunk with a variable number of arguments.
     *
     * @tparam Args Variadic template arguments that can be passed to initialize the chunk.
     * @param args The arguments to be added to the byte chunk.
     */
    
    template <typename ...Args>
    NetworkByteChunk(const Args& ...args)
    {
        Add(std::forward<const Args>(args)...);
    }
    
    /**
     * @brief Adds a WDL_String object to the byte chunk.
     *
     * @param str The WDL_String object to be added.
     */
    
    inline void Add(const WDL_String& str)
    {
        PutStr(str.Get());
    }
    
    /**
     * @brief Adds a null-terminated C-string to the byte chunk.
     *
     * @param str The C-string to be added.
     */
    
    inline void Add(const char* str)
    {
        PutStr(str);
    }
    
    /**
     * @brief Adds another IByteChunk object to the current byte chunk.
     *
     * @param chunk The IByteChunk object to be added.
     */
    
    inline void Add(const iplug::IByteChunk& chunk)
    {
        PutChunk(&chunk);
    }
    
    /**
     * @brief Adds a generic value to the byte chunk.
     *
     * @tparam T The type of the value to be added.
     * @param value The value to be added to the byte chunk.
     */
    
    template <typename T>
    inline void Add(const T& value)
    {
        Put(&value);
    }
    
    /**
     * @brief Adds a series of values to the byte chunk. The first value is added,
     *        followed by recursively adding the remaining arguments.
     *
     * @tparam First The type of the first value to be added.
     * @tparam Args Variadic template for the remaining values to be added.
     * @param value The first value to be added to the byte chunk.
     * @param args The remaining values to be added recursively.
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
 * @class NetworkByteStream
 * @brief A class that handles the streaming of network byte data.
 *        It provides mechanisms to read from and write to a stream of bytes.
 */

class NetworkByteStream
{
public:
    
    /**
     * @brief Constructs a NetworkByteStream object from an existing byte stream with an optional starting position.
     *
     * @param stream The byte stream to be wrapped by the NetworkByteStream.
     * @param startPos The initial position in the stream. Defaults to 0.
     */
    
    NetworkByteStream(const iplug::IByteStream& stream, int startPos = 0)
    : mStream(stream)
    , mPos(startPos)
    {}
    
    /**
     * @brief Returns the current position in the byte stream.
     *
     * @return The current position as an integer.
     */
    
    inline int Tell() const
    {
        return mPos;
    }
    
    /**
     * @brief Sets the current position in the byte stream to the specified position.
     *
     * @param pos The new position in the byte stream, as an integer.
     */
    
    inline void Seek(int pos)
    {
        mPos = pos;
    }
    
    /**
     * @brief Reads data from the byte stream into the specified value.
     *
     * @tparam T The type of the value to be read from the stream.
     * @param value A reference to the variable where the data will be stored.
     */
    
    template <class T>
    inline void Get(T& value)
    {
        mPos = mStream.Get(&value, mPos);
    }
    
    /**
     * @brief Reads multiple values from the byte stream. The first value is read, followed by recursively reading the remaining arguments.
     *
     * @tparam First The type of the first value to be read from the stream.
     * @tparam Args Variadic template for the remaining values to be read.
     * @param value A reference to the first variable where the data will be stored.
     * @param args References to the remaining variables where the subsequent data will be stored.
     */
    
    template <class First, class ...Args>
    inline void Get(First& value, Args& ...args)
    {
        mPos = mStream.Get(&value, mPos);
        Get(args...);
    }
    
    /**
     * @brief Reads a WDL_String from the byte stream and stores it in the provided string object.
     *
     * @param str A reference to the WDL_String object where the read data will be stored.
     */
    
    inline void Get(WDL_String& str)
    {
        mPos = mStream.GetStr(str, mPos);
    }
    
    // Look to see if the next item is a tag matching the input
    // Advance if the tag is matched
    
    /**
     * @brief Checks if the next item in the byte stream matches the specified tag.
     *
     * @param tag A C-string representing the tag to compare against the next item in the stream.
     * @return true if the next item matches the tag, false otherwise.
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
     * @brief A reference to the byte stream being read from or written to.
     *        This represents the underlying data stream for the NetworkByteStream.
     */
    
    const iplug::IByteStream& mStream;
    
    /**
     * @brief The current position within the byte stream.
     *        This integer tracks where the next read or write operation will occur.
     */
    
    int mPos;
};

#endif /* NETWORKDATA_HPP */
