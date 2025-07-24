
#ifndef NETWORKTIMING_HPP
#define NETWORKTIMING_HPP

#include <algorithm>
#include <chrono>
#include <cmath>

// A timer based on CPU time that can report intervals between start and polling

class CPUTimer
{
public:
    
    CPUTimer()
    {
        Start();
    }
    
    void Start()
    {
        mStart = std::chrono::steady_clock::now();
    }
    
    double Interval() const
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - mStart).count();
    }
    
private:
    
    std::chrono::steady_clock::time_point mStart;
};

// A polling helper that can be used to execute regular operations based on a given time interval

class IntervalPoll
{
public:
    
    IntervalPoll(double intervalMS)
    : mInterval(intervalMS / 1000.0)
    , mLastTime(mTimer.Interval() - mInterval)
    {}
    
    bool operator()()
    {
        double time = mTimer.Interval();
        
        if (time >= mLastTime + mInterval)
        {
            mLastTime = time;
            return true;
        }
        
        return false;
    }
    
    double Until()
    {
        double time = mTimer.Interval();
        
        return std::max(0.0, ((mLastTime + mInterval) - time) * 1000.0);
    }
    
    void Reset()
    {
        mTimer.Start();
        mLastTime = mTimer.Interval() - mInterval;
    }
    
private:
    
    CPUTimer mTimer;
    double mInterval;
    double mLastTime;
};

// A timestamp for accurate timing

class TimeStamp
{
public:
    
    TimeStamp() : mTime(0) {}
    TimeStamp(double time) : mTime(time){}
    
    friend TimeStamp operator + (TimeStamp a, TimeStamp b)
    {
        return TimeStamp(a.mTime + b.mTime);
    }
    
    friend TimeStamp operator - (TimeStamp a, TimeStamp b)
    {
        return TimeStamp(a.mTime - b.mTime);
    }
    
    friend bool operator < (TimeStamp a, TimeStamp b)
    {
        return a.mTime < b.mTime;
    }
    
    friend bool operator > (TimeStamp a, TimeStamp b)
    {
        return a.mTime > b.mTime;
    }
    
    friend bool operator <= (TimeStamp a, TimeStamp b)
    {
        return !(a > b);
    }
    
    friend bool operator >= (TimeStamp a, TimeStamp b)
    {
        return !(a < b);
    }
    
    friend bool operator == (TimeStamp a, TimeStamp b)
    {
        return a.mTime == b.mTime;
    }
    
    friend bool operator != (TimeStamp a, TimeStamp b)
    {
        return !(a == b);
    }
    
    friend TimeStamp Half(TimeStamp a)
    {
        return TimeStamp(a.mTime * 0.5);
    }
    
    static TimeStamp AsTime(uintptr_t count, double sr)
    {
        return TimeStamp(count / sr);
    }
    
    intptr_t AsSamples(double sr) const
    {
        return std::round(mTime * sr);
    }
    
    double AsDouble() const { return mTime; }
    
    
private:
    
    double mTime;
};

#endif /* NETWORKTIMING_HPP */
