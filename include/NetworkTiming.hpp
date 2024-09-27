
/**
 * @file NetworkTiming.hpp
 * @brief Provides classes and utilities for time management and polling intervals.
 *
 * This file contains several classes that offer functionality for measuring
 * time, tracking intervals, and performing time-based comparisons. It includes
 * CPU-based timing, time stamps for tracking and comparing moments in time,
 * and utilities for polling intervals.
 *
 * The key classes in this file are:
 * - CPUTimer: A class for measuring time using the CPU clock.
 * - IntervalPoll: A class for polling specific time intervals.
 * - TimeStamp: A class for storing and comparing time values.
 *
 * These tools are essential for any application requiring time management or
 * performance monitoring.
 */

#ifndef NETWORKTIMING_HPP
#define NETWORKTIMING_HPP

#include <algorithm>
#include <chrono>
#include <cmath>

// A timer based on CPU time that can report intervals between start and polling

/**
 * @brief A class for CPU-based timing operations.
 *
 * The CPUTimer class provides methods for measuring time intervals using the
 * CPU clock. It can be used to track the duration of operations or for
 * performance monitoring.
 */

class CPUTimer
{
public:
    
    /**
     * @brief Constructs a new CPUTimer object.
     *
     * This constructor initializes the CPUTimer instance, setting up any internal
     * states required for timing operations. By default, the timer is ready to start
     * tracking time intervals.
     */
    
    CPUTimer()
    {
        Start();
    }
    
    /**
     * @brief Starts the network timing mechanism.
     *
     * This function initiates the network timing system. It is expected to set up
     * any required resources or states for timing to begin.
     */
    
    void Start()
    {
        mStart = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Retrieves the elapsed time interval.
     *
     * This method returns the amount of time, in seconds, that has passed since the
     * timer was started. It measures the interval based on the CPU clock.
     *
     * @return The elapsed time interval in seconds as a double.
     */
    
    double Interval() const
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - mStart).count();
    }
    
private:
    
    /**
     * @brief Stores the start time point of the timer.
     *
     * This member holds the time point when the timer was started, using
     * `std::chrono::steady_clock`. It is used to calculate the elapsed time
     * in the `Interval()` method.
     */
    
    std::chrono::steady_clock::time_point mStart;
};

// A polling helper that can be used to execute regular operations based on a given time interval


/**
 * @brief A class for polling time intervals.
 *
 * The IntervalPoll class provides functionality to periodically check or poll
 * for elapsed time intervals. This can be used in scenarios where actions need
 * to be triggered or monitored based on time progression.
 */

class IntervalPoll
{
public:
    
    /**
     * @brief Constructs a new IntervalPoll object with a specified polling interval.
     *
     * This constructor initializes the IntervalPoll object with a given time interval
     * in milliseconds. It converts the provided interval from milliseconds to seconds
     * and sets up the internal timer accordingly.
     *
     * @param intervalMS The polling interval in milliseconds.
     */
    
    IntervalPoll(double intervalMS)
    : mInterval(intervalMS / 1000.0)
    , mLastTime(mTimer.Interval() - mInterval)
    {}
    
    /**
     * @brief Checks if the specified time interval has passed.
     *
     * This operator function is used to poll whether the defined time interval has
     * elapsed since the last check. If the interval has passed, it returns true; otherwise,
     * it returns false.
     *
     * @return `true` if the time interval has elapsed, `false` otherwise.
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
     * @brief Calculates the remaining time until the next interval.
     *
     * This method returns the amount of time, in seconds, remaining until the next
     * polling interval is reached. It can be used to determine how much time is left
     * before the next interval-based action should be triggered.
     *
     * @return The remaining time in seconds as a double.
     */
    
    double Until()
    {
        double time = mTimer.Interval();
        
        return std::max(0.0, ((mLastTime + mInterval) - time) * 1000.0);
    }
    
    /**
     * @brief Resets the interval timer.
     *
     * This method resets the internal timer, starting a new time interval from the
     * current point in time. It is useful when the timing needs to be restarted or
     * recalibrated for a new interval.
     */
    
    void Reset()
    {
        mTimer.Start();
        mLastTime = mTimer.Interval() - mInterval;
    }
    
private:
    
    /**
     * @brief A CPUTimer instance used for tracking time intervals.
     *
     * This member variable is an instance of the CPUTimer class, which is used to
     * measure and track the elapsed time between operations. It provides timing
     * information for polling and interval-related operations.
     */
    
    CPUTimer mTimer;
    
    /**
     * @brief The time interval for polling, in seconds.
     *
     * This member variable stores the duration of the time interval, specified in
     * seconds, that is used for polling operations. It defines how frequently the
     * polling should occur.
     */
    
    double mInterval;
    
    /**
     * @brief Stores the time of the last interval check.
     *
     * This member variable holds the elapsed time at which the last successful
     * interval check occurred. It is used to calculate the time difference for
     * the next polling operation.
     */
    
    double mLastTime;
};

// A timestamp for accurate timing

/**
 * @brief A class for recording and comparing time stamps.
 *
 * The TimeStamp class provides functionality for capturing the current time
 * and comparing time stamps. It is useful for logging events, measuring
 * durations, or tracking when specific actions occur.
 */

class TimeStamp
{
public:
    
    /**
     * @brief Constructs a new TimeStamp object with an initial time of zero.
     *
     * This constructor initializes the TimeStamp object and sets the internal time
     * to zero. It can be used to start tracking time from a defined initial point.
     */
    
    TimeStamp() : mTime(0) {}
    
    /**
     * @brief Constructs a new TimeStamp object with a specified time.
     *
     * This constructor initializes the TimeStamp object with the provided time value.
     * The time value represents a specific point in time, which can be used for
     * comparison or logging purposes.
     *
     * @param time The initial time value to set for the timestamp, in seconds.
     */
    
    TimeStamp(double time) : mTime(time){}
    
    /**
     * @brief Adds two TimeStamp objects together.
     *
     * This friend function allows the addition of two TimeStamp objects, returning
     * a new TimeStamp that represents the sum of their time values. This can be used
     * to combine time durations or calculate a future time based on two TimeStamps.
     *
     * @param a The first TimeStamp object.
     * @param b The second TimeStamp object.
     * @return A new TimeStamp object representing the sum of the two time values.
     */
    
    friend TimeStamp operator + (TimeStamp a, TimeStamp b)
    {
        return TimeStamp(a.mTime + b.mTime);
    }
    
    /**
     * @brief Subtracts one TimeStamp from another.
     *
     * This friend function allows the subtraction of two TimeStamp objects, returning
     * a new TimeStamp that represents the difference between their time values. This
     * can be used to calculate the time elapsed between two time points.
     *
     * @param a The first TimeStamp object (minuend).
     * @param b The second TimeStamp object (subtrahend).
     * @return A new TimeStamp object representing the difference between the two time values.
     */
    
    friend TimeStamp operator - (TimeStamp a, TimeStamp b)
    {
        return TimeStamp(a.mTime - b.mTime);
    }
    
    /**
     * @brief Compares two TimeStamp objects to determine if one is earlier than the other.
     *
     * This friend function allows the comparison of two TimeStamp objects, returning
     * `true` if the first TimeStamp is earlier (has a smaller time value) than the second.
     *
     * @param a The first TimeStamp object.
     * @param b The second TimeStamp object.
     * @return `true` if the first TimeStamp is earlier than the second, `false` otherwise.
     */
    
    friend bool operator < (TimeStamp a, TimeStamp b)
    {
        return a.mTime < b.mTime;
    }
    
    /**
     * @brief Compares two TimeStamp objects to determine if one is later than the other.
     *
     * This friend function allows the comparison of two TimeStamp objects, returning
     * `true` if the first TimeStamp is later (has a larger time value) than the second.
     *
     * @param a The first TimeStamp object.
     * @param b The second TimeStamp object.
     * @return `true` if the first TimeStamp is later than the second, `false` otherwise.
     */
    
    friend bool operator > (TimeStamp a, TimeStamp b)
    {
        return a.mTime > b.mTime;
    }
    
    /**
     * @brief Compares two TimeStamp objects to determine if one is earlier than or equal to the other.
     *
     * This friend function allows the comparison of two TimeStamp objects, returning
     * `true` if the first TimeStamp is earlier than or equal to the second.
     *
     * @param a The first TimeStamp object.
     * @param b The second TimeStamp object.
     * @return `true` if the first TimeStamp is earlier than or equal to the second, `false` otherwise.
     */
    
    friend bool operator <= (TimeStamp a, TimeStamp b)
    {
        return !(a < b);
    }
    
    /**
     * @brief Compares two TimeStamp objects to determine if one is later than or equal to the other.
     *
     * This friend function allows the comparison of two TimeStamp objects, returning
     * `true` if the first TimeStamp is later than or equal to the second.
     *
     * @param a The first TimeStamp object.
     * @param b The second TimeStamp object.
     * @return `true` if the first TimeStamp is later than or equal to the second, `false` otherwise.
     */
    
    friend bool operator >= (TimeStamp a, TimeStamp b)
    {
        return !(a > b);
    }
    
    /**
     * @brief Returns a TimeStamp that represents half of the given TimeStamp value.
     *
     * This friend function takes a TimeStamp object and returns a new TimeStamp
     * that represents half of its time value. It can be used to perform operations
     * where halving a time duration is required.
     *
     * @param a The TimeStamp object to halve.
     * @return A new TimeStamp object representing half of the given time value.
     */
    
    friend TimeStamp Half(TimeStamp a)
    {
        return TimeStamp(a.mTime * 0.5);
    }
    
    /**
     * @brief Converts a sample count and sample rate into a TimeStamp object.
     *
     * This static function takes a sample count and a sample rate, and converts them
     * into a TimeStamp object that represents the equivalent time duration. This is
     * useful for converting between time-based and sample-based representations in
     * audio or other sampled data contexts.
     *
     * @param count The number of samples (as uintptr_t).
     * @param sr The sample rate, in samples per second (as a double).
     * @return A TimeStamp object representing the equivalent time duration for the given sample count and sample rate.
     */
    
    static TimeStamp AsTime(uintptr_t count, double sr)
    {
        return TimeStamp(count / sr);
    }
    
    /**
     * @brief Converts the current TimeStamp into a sample count based on a given sample rate.
     *
     * This method converts the time value stored in the TimeStamp object into an
     * equivalent number of samples, using the provided sample rate. It is useful
     * for translating time durations into a sample-based representation.
     *
     * @param sr The sample rate, in samples per second (as a double).
     * @return The equivalent number of samples as an intptr_t.
     */
    
    intptr_t AsSamples(double sr) const
    {
        return std::round(mTime * sr);
    }
    
    /**
     * @brief Retrieves the time value as a double.
     *
     * This method returns the current time value stored in the TimeStamp object
     * as a double. It provides a way to access the time in a basic floating-point
     * format for further calculations or comparisons.
     *
     * @return The time value as a double.
     */
    
    double AsDouble() const { return mTime; }
    
    
private:
    
    /**
     * @brief Stores the time value for the TimeStamp object.
     *
     * This member variable holds the time value, represented as a double, for the
     * current TimeStamp. It is used in various operations and methods to track
     * and manipulate time durations.
     */
    
    double mTime;
};

#endif /* NETWORKTIMING_HPP */
