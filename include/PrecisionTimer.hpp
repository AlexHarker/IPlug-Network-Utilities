
#ifndef PRECISIONTIMER_HPP
#define PRECISIONTIMER_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

#include "NetworkPeer.hpp"

class PrecisionTimer : public NetworkPeer
{
    template <class T, int Size>
    class MedianFilter
    {
        using ArrayType = std::array<T, Size>;
        
        struct CompareIndices
        {
            CompareIndices(const ArrayType& array)
            : mArray(array)
            {}
            
            bool operator()(int idx1, int idx2) const
            {
                return mArray[idx1] < mArray[idx2];
            }
            
            const ArrayType& mArray;
        };
        
    public:
        
        MedianFilter()
        {
            Reset();
        }
                
        const T& operator()(const T& in)
        {
            mMemory[mCount] = in;
            
            if (++mCount >= Size)
                mCount = 0;
            
            std::array<int, Size> order;
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), CompareIndices(mMemory));

            return mMemory[order[Size >> 1]];
        }
        
        void Reset()
        {
            mMemory.fill(T(0));
            mCount = 0;
        }
        
    private:
        
        ArrayType mMemory;
        int mCount;
    };
    
public:
    
    PrecisionTimer(const char *regname, uint16_t port = 8001)
    : NetworkPeer(regname, port)
    , mLastTimeStamp(0)
    {}
    
    void Reset(uintptr_t count = 0)
    {
        mCount = count;
        mMonotonicCount = 0;
        mLastTimeStamp = TimeStamp(0);
        mFilter.Reset();
    }
    
    void Progress(uintptr_t count)
    {
        if (!mCount)
            mReference = CPUTimeStamp();
        
        mCount += count;
        
        if (AsTime() <= mLastTimeStamp)
            mMonotonicCount = 0;
        else
            mMonotonicCount += count;
            
        mLastTimeStamp = AsTime();
    }
    
    uintptr_t Count() const
    {
        return mCount;
    }
    
    double MonotonicTime() const
    {
        return mMonotonicCount / mSamplingRate;
    }
    
    TimeStamp AsTime() const
    {
        return mOffset + TimeStamp::AsTime(mCount, mSamplingRate);
    }
    
    intptr_t AsSamples() const
    {
        return mOffset.AsSamples(mSamplingRate) + static_cast<intptr_t>(mCount);
    }
    
    void Sync()
    {
        if (IsConnectedAsClient())
            SendFromClient("Sync", GetTimeStamp());
    }
    
    void Stability()
    {
        if (MonotonicTime() < 0.1)
        {
            //DBGMSG("NON-MONOTONIC\n");
        }
    }
    
    TimeStamp GetTimeStamp() const
    {
        return AsTime();//CPUTimeStamp() - mReference;
    }
    
    void SetSamplingRate(double sr)
    {
        mSamplingRate = sr;
    }
    
protected:
    
    bool ProcessAsServer(ConnectionID id, NetworkByteStream& stream)
    {
        if (stream.IsNextTag("Sync"))
        {
            TimeStamp t1, t2;
            
            stream.Get(t1);
            t2 = GetTimeStamp();
            
            //DBGMSG("discrepancy %lf ms\n", (t2.AsDouble() - AsTime().AsDouble()) * 1000.0);

            SendToClient(id, "Respond", t1, t2);
            
            return true;
        }
        
        return false;
    }
    
    bool ProcessAsClient(NetworkByteStream& stream)
    {
        if (stream.IsNextTag("Respond"))
        {
            TimeStamp t1, t2, t3;
            
            stream.Get(t1, t2);
            
            t3 = GetTimeStamp();
            //auto ts = t3.AsDouble() + mReference;
            
            auto offset = CalculateOffset(t1, t2, t2, t3).AsDouble();
            
            TimeStamp alterRaw = offset * std::max(0.1, std::min(1.0, std::abs(offset)));
            auto compare = std::abs(mFilter(alterRaw).AsDouble()) * 8.0;
            
            TimeStamp alter = std::min(compare, std::max(-compare, alterRaw.AsDouble()));
            
            //if (alterRaw.AsDouble() != alter.AsDouble())
            //    DBGMSG("CLIP\n");
            
            //DBGMSG("Clock offset %+.3lf ms / altered %+.3lf ms / roundtrip %.3lfms\n", offset * 1000.0, alter.AsDouble() * 1000.0, (t3-t1).AsDouble() * 1000.0);
            
            mOffset = mOffset + alter;
            mReference = -mOffset.AsDouble();
            
            return true;
        }
        
        return false;
    }
    
private:
    
    static double CPUTimeStamp()
    {
        static auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    
    static TimeStamp CalculateOffset(TimeStamp t1, TimeStamp t2, TimeStamp t3, TimeStamp t4)
    {
        return Half(t2 - t1 - t4 + t3);
    }
     
    void ReceiveAsServer(ConnectionID id, NetworkByteStream& stream) override
    {
        ProcessAsServer(id, stream);
    }
    
    void ReceiveAsClient(NetworkByteStream& stream) override
    {
        ProcessAsClient(stream);
    }
    
    double mSamplingRate = 44100;
    uintptr_t mCount;
    uintptr_t mMonotonicCount;
    TimeStamp mOffset;
    TimeStamp mLastTimeStamp;
    double mReference;
    MedianFilter<TimeStamp, 5> mFilter;
};

#endif /* PRECISIONTIMER_HPP */
