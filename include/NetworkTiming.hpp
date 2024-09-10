
/**
 * @file NetworkTiming.hpp
 * @brief Provides utility classes for timing and polling operations.
 *
 * This file contains the implementation of various classes designed for managing time-related operations
 * such as CPU timing, time stamps, and polling intervals. The classes allow for precise time measurement
 * and interval management, useful for applications that require time tracking and regular polling.
 *
 * Classes included:
 * - CPUTimer: A class for measuring time intervals using a steady clock.
 * - IntervalPoll: A class for managing polling intervals and determining when to perform an operation.
 * - TimeStamp: A class for capturing specific points in time and performing time-based calculations.
 *
 * The utility of these classes is in handling time-sensitive operations, with easy-to-use interfaces
 * for starting timers, checking intervals, and managing time values.
 */

#ifndef NETWORKTIMING_HPP
#define NETWORKTIMING_HPP

#include <algorithm>
#include <chrono>
#include <cmath>

// A timer based on CPU time that can report intervals between start and polling

/**
 * @class CPUTimer
 * @brief A simple CPU Timer class for measuring time intervals.
 *
 * This class provides functionality to measure the elapsed time
 * between two events using the steady clock.
 */

class CPUTimer
{
public:
    
    /**
     * @brief Default constructor for the CPUTimer class.
     *
     * Initializes the timer by calling the Start() method to capture the start time.
     */
    
    CPUTimer()
    {
        Start();
    }
    
    /**
     * @brief Starts or resets the timer.
     *
     * This method records the current time as the start time, which can be used
     * to measure the time interval until a subsequent event.
     */
    
    void Start()
    {
        mStart = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Calculates the elapsed time since the timer was started.
     *
     * This method computes the time interval in seconds between the moment the timer was started
     * (or last reset) and the current time.
     *
     * @return The elapsed time in seconds as a double.
     */
    
    double Interval()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - mStart).count();
    }
    
private:
    
    /**
     * @brief Stores the starting point of the timer.
     *
     * This variable holds the time when the timer was last started or reset,
     * using the steady clock to ensure that the time measurement is not affected
     * by adjustments to the system clock.
     */
    
    std::chrono::steady_clock::time_point mStart;
};

// A polling helper that can be used to execute regular operations based on a given time interval

/**
 * @class IntervalPoll
 * @brief A class designed for polling operations at regular intervals.
 *
 * The IntervalPoll class provides functionality for checking elapsed time
 * to determine if a certain polling interval has passed. It is useful for
 * operations that need to be executed at fixed time intervals.
 */

class IntervalPoll
{
public:
    
    /**
     * @brief Constructs an IntervalPoll object with a specified interval.
     *
     * This constructor initializes the polling interval and sets the last poll time.
     * The interval is converted from milliseconds to seconds, and the last poll time
     * is set relative to the current time, allowing immediate polling on the next check.
     *
     * @param intervalMS The polling interval in milliseconds.
     */
    
    IntervalPoll(double intervalMS)
    : mInterval(intervalMS / 1000.0)
    , mLastTime(mTimer.Interval() - mInterval)
    {}
    
    /**
     * @brief Checks if the specified polling interval has elapsed.
     *
     * This operator overload allows the IntervalPoll object to be used as a callable.
     * It returns true if the time since the last poll exceeds the set interval, and
     * updates the last poll time. Otherwise, it returns false.
     *
     * @return True if the polling interval has passed, false otherwise.
     */
    
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
    
    /**
     * @brief Calculates the remaining time until the next poll is allowed.
     *
     * This method returns the time (in seconds) left before the next polling interval
     * is reached. If the interval has already passed, it returns 0.
     *
     * @return The remaining time in seconds until the next poll, or 0 if the interval has passed.
     */
    
    double Until()
    {
        double time = mTimer.Interval();
        
        return std::max(0.0, ((mLastTime + mInterval) - time) * 1000.0);
    }
    
    /**
     * @brief Resets the polling timer to the current time.
     *
     * This method resets the last poll time to the current time, effectively starting
     * a new polling interval. It can be used to manually control when the next poll should begin.
     */
    
    void Reset()
    {
        mTimer.Start();
        mLastTime = mTimer.Interval() - mInterval;
    }
    
private:
    
    /**
     * @brief An instance of CPUTimer used to measure time intervals.
     *
     * This timer is used to track elapsed time between polling intervals,
     * helping determine when the next poll is allowed based on the set interval.
     */
    
    CPUTimer mTimer;
    
    /**
     * @brief The time interval for polling, in seconds.
     *
     * This variable stores the duration between polls. It is set when the
     * IntervalPoll object is initialized and determines how often polling is allowed.
     */
    
    double mInterval;
    
    /**
     * @brief The time when the last poll occurred, in seconds.
     *
     * This variable stores the time of the last successful poll, measured by the timer.
     * It is used to calculate whether enough time has passed to allow the next poll
     * based on the set polling interval.
     */
    
    double mLastTime;
};

// A timestamp for accurate timing

/**
 * @class TimeStamp
 * @brief A class representing a specific point in time.
 *
 * The TimeStamp class is used to capture and store a specific moment in time.
 * It provides functionality for comparing time points and calculating the duration
 * between them.
 */

class TimeStamp
{
public:
    
    /**
     * @brief Default constructor for the TimeStamp class.
     *
     * Initializes a TimeStamp object with a time value of 0, representing the default or
     * initial time state.
     */
    
    TimeStamp() : mTime(0) {}
    
    /**
     * @brief Constructs a TimeStamp object with a specified time.
     *
     * This constructor initializes the TimeStamp object with the given time value.
     *
     * @param time The time value (in seconds) to initialize the TimeStamp object with.
     */
    
    TimeStamp(double time) : mTime(time){}
    
    /**
     * @brief Overloads the addition operator for TimeStamp objects.
     *
     * This friend function allows two TimeStamp objects to be added together.
     * It returns a new TimeStamp representing the sum of the time values of
     * both TimeStamp objects.
     *
     * @param a The first TimeStamp object.
     * @param b The second TimeStamp object.
     * @return A new TimeStamp object representing the sum of the time values of a and b.
     */
    
    friend TimeStamp operator + (TimeStamp a, TimeStamp b)
    {
        return TimeStamp(a.mTime + b.mTime);
    }
    
    /**
     * @brief Overloads the subtraction operator for TimeStamp objects.
     *
     * This friend function allows one TimeStamp object to be subtracted from another.
     * It returns a new TimeStamp representing the difference between the time values
     * of the two TimeStamp objects.
     *
     * @param a The first TimeStamp object (minuend).
     * @param b The second TimeStamp object (subtrahend).
     * @return A new TimeStamp object representing the difference between the time values of a and b.
     */
    
    friend TimeStamp operator - (TimeStamp a, TimeStamp b)
    {
        return TimeStamp(a.mTime - b.mTime);
    }
    
    /**
     * @brief Overloads the less-than operator for TimeStamp objects.
     *
     * This friend function compares two TimeStamp objects and returns true if the time value of the
     * first TimeStamp is less than that of the second.
     *
     * @param a The first TimeStamp object to compare.
     * @param b The second TimeStamp object to compare.
     * @return True if the time value of a is less than b, false otherwise.
     */
    
    friend bool operator < (TimeStamp a, TimeStamp b)
    {
        return a.mTime < b.mTime;
    }
    
    /**
     * @brief Returns a TimeStamp representing half the value of the given TimeStamp.
     *
     * This friend function takes a TimeStamp object and returns a new TimeStamp with
     * a time value equal to half of the original TimeStamp's value.
     *
     * @param a The TimeStamp object to halve.
     * @return A new TimeStamp object representing half of the time value of a.
     */
    
    friend TimeStamp Half(TimeStamp a)
    {
        return TimeStamp(a.mTime * 0.5);
    }
    
    /**
     * @brief Converts a sample count and sample rate into a TimeStamp.
     *
     * This static method calculates the time based on a given sample count and sample rate.
     * It returns a TimeStamp object representing the time corresponding to the number of samples
     * and the provided sample rate.
     *
     * @param count The number of samples.
     * @param sr The sample rate in samples per second.
     * @return A TimeStamp object representing the equivalent time for the given sample count and rate.
     */
    
    static TimeStamp AsTime(uintptr_t count, double sr)
    {
        return TimeStamp(count / sr);
    }
    
    /**
     * @brief Converts the TimeStamp into a sample count based on the given sample rate.
     *
     * This method calculates the number of samples corresponding to the current TimeStamp
     * value, using the provided sample rate. It returns the sample count as an integer.
     *
     * @param sr The sample rate in samples per second.
     * @return The number of samples corresponding to the TimeStamp value.
     */
    
    intptr_t AsSamples(double sr) const
    {
        return std::round(mTime * sr);
    }
    
    /**
     * @brief Returns the time value of the TimeStamp as a double.
     *
     * This method provides the stored time value of the TimeStamp object as a double-precision
     * floating-point number.
     *
     * @return The time value of the TimeStamp as a double.
     */
    
    double AsDouble() const { return mTime; }
    
    
private:
    
    /**
     * @brief The time value stored in the TimeStamp object.
     *
     * This variable holds the time value in seconds, represented as a double-precision
     * floating-point number. It is used for time-related calculations and comparisons.
     */
    
    double mTime;
};

#endif /* NETWORKTIMING_HPP */
