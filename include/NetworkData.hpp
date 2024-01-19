
#ifndef NETWORKDATA_HPP
#define NETWORKDATA_HPP

#include <wdlstring.h>

#include "IPlugStructs.h"

// A wrapper for iplug::IByteChunk that can be constructued with its contents
// Multiple items can also be added at a time later

struct NetworkByteChunk : public iplug::IByteChunk
{
public:

    template <typename ...Args>
    NetworkByteChunk(const Args& ...args)
    {
        Add(std::forward<const Args>(args)...);
    }
    
    inline void Add(const WDL_String& str)
    {
        PutStr(str.Get());
    }
    
    inline void Add(const char* str)
    {
        PutStr(str);
    }
    
    inline void Add(const iplug::IByteChunk& chunk)
    {
        PutChunk(&chunk);
    }
    
    inline void Add(const NetworkByteChunk& chunk)
    {
        PutChunk(&chunk);
    }
    
    template <typename T>
    inline void Add(const T& value)
    {
        Put(&value);
    }
    
    template <typename First, typename ...Args>
    inline void Add(const First& value, const Args& ...args)
    {
        Add(value);
        Add(std::forward<const Args>(args)...);
    }
    
};

// A wrapper for iplug::IByteStream that tracks its own position

class NetworkByteStream
{
public:
    
    NetworkByteStream(const iplug::IByteStream& stream, int startPos = 0)
    : mStream(stream)
    , mPos(startPos)
    {}
    
    inline int Tell() const
    {
        return mPos;
    }
    
    inline void Seek(int pos)
    {
        mPos = pos;
    }
    
    template <class T>
    inline void Get(T& value)
    {
        mPos = mStream.Get(&value, mPos);
    }
    
    inline void Get(WDL_String& str)
    {
        mPos = mStream.GetStr(str, mPos);
    }
    
    template <class First, class ...Args>
    inline void Get(First& value, Args& ...args)
    {
        Get(value);
        Get(args...);
    }
    
    // Look to see if the next item is a tag matching the input
    // Advance if the tag is matched
    
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
    
    const iplug::IByteStream& mStream;
    int mPos;
};

#endif /* NETWORKDATA_HPP */
