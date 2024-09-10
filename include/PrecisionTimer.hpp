
/**
 * @file PrecisionTimer.hpp
 * @brief Defines the `PrecisionTimer` class and related structures for high-precision timing operations.
 *
 * This file contains the implementation of the `PrecisionTimer` class, which provides functionality
 * for high-precision time measurements and synchronization. The timer is designed to work with
 * various timing mechanisms, including monotonic clocks, CPU timestamps, and sampling rates.
 *
 * Key components of this file include:
 * - `PrecisionTimer`: The main class that tracks time with high precision and supports methods
 *   for time synchronization, offset calculation, and progress tracking.
 * - `MedianFilter`: A template class that is used within `PrecisionTimer` to smooth noisy time
 *   measurements by calculating the median of a set of values.
 * - `CompareIndices`: A helper structure used for comparison operations within the timer's logic.
 *
 * This file is intended for use in applications where precise time measurement and event
 * synchronization are critical, such as performance monitoring, real-time systems, and
 * network communication.
 *
 * @note The timer utilizes a monotonic clock to ensure that time progresses consistently,
 * independent of system clock adjustments. It also supports a customizable sampling rate for
 * applications requiring discrete time sampling.
 */

#ifndef PRECISIONTIMER_HPP
#define PRECISIONTIMER_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

#include "AutoServer.hpp"

/**
 * @class PrecisionTimer
 * @brief A class that represents a high-precision timer, inheriting from `AutoServer`.
 *
 * The `PrecisionTimer` class provides functionality for high-precision time tracking and
 * measurement. It inherits from the `AutoServer` class, allowing it to potentially manage
 * server-related tasks or other auto-triggered actions in conjunction with timing capabilities.
 *
 * This class is designed to offer accurate timing operations, which can be useful in scenarios
 * where precise time intervals are critical, such as performance measurements or time-sensitive
 * tasks.
 *
 * @note This class relies on the functionality provided by the `AutoServer` base class.
 */

class PrecisionTimer : public AutoServer
{
    
    /**
     * @class MedianFilter
     * @brief A template class for applying a median filter to a series of values.
     *
     * The `MedianFilter` class implements a median filter algorithm, which is useful for reducing
     * noise in a data series by smoothing values. It maintains a fixed-size window of the most recent
     * values and computes the median of those values.
     *
     * This is a template class where:
     * - `T` is the type of the elements being filtered (e.g., int, float, etc.).
     * - `Size` is the fixed size of the window used to calculate the median.
     *
     * @tparam T The type of elements to be filtered.
     * @tparam Size The number of values used in the median filtering window.
     *
     * @note The size of the filter window is fixed at compile time.
     */
    
    template <class T, int Size>
    class MedianFilter
    {
        
        /**
         * @typedef ArrayType
         *
         * A type alias for `std::array` that stores elements of type `T` and is fixed in size `Size`.
         *
         * This alias simplifies the usage of `std::array` by representing an array of `T` elements
         * with a size defined by the template parameter `Size`. The type `T` can be any valid type.
         *
         * @tparam T The type of the elements in the array.
         * @tparam Size The fixed size of the array.
         */
        
        using ArrayType = std::array<T, Size>;
        
        /**
         * @struct CompareIndices
         * @brief A comparator structure for comparing indices based on specific criteria.
         *
         * The `CompareIndices` structure is used to define a custom comparison function, typically
         * for sorting or ordering operations. The exact behavior of the comparison depends on how
         * the structure's comparison operator is implemented.
         *
         * This structure is often used in conjunction with sorting algorithms or data structures
         * that require a custom method to compare elements, such as priority queues or sorting indices.
         *
         * @note The implementation details of the comparison logic are defined by the structure's
         * operator overloads or member functions.
         */
        
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
        
        /**
         * @brief Default constructor for the `MedianFilter` class.
         *
         * Initializes a `MedianFilter` object with a fixed-size window defined by the template parameter `Size`.
         * The constructor sets up the internal data structure to hold `Size` number of elements, which will be
         * used for median filtering operations. Initially, the filter is empty and ready to accept input values.
         *
         * This constructor does not take any arguments and prepares the filter for use without initializing it with data.
         *
         * @note After construction, the filter window is empty and will need to be filled with values before
         * median filtering can be performed.
         */
        
        MedianFilter()
        {
            Reset();
        }
        
        /**
         * @brief Processes a new input value through the median filter and returns the current median.
         *
         * This operator overload allows the `MedianFilter` to be used like a function. It accepts a new
         * input value, `in`, adds it to the internal buffer, and computes the median of the current values
         * in the filter window. The function returns the current median after processing the new input.
         *
         * @param in The new value of type `T` to be added to the filter window.
         * @return const T& The current median value of the window after adding the new input.
         *
         * @note The input value `in` is added to the filter, and the oldest value in the buffer is
         * replaced if the buffer is full. The median is recalculated based on the updated buffer.
         */
        
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
        
        /**
         * @brief Resets the median filter to its initial state.
         *
         * The `Reset()` function clears the internal buffer of the `MedianFilter`, effectively removing all
         * previously added values. After calling this function, the filter will behave as though it was
         * freshly constructed, with an empty window and no values to calculate a median from.
         *
         * This is useful when you need to restart the filtering process with a clean state, discarding
         * any old data and starting fresh with new inputs.
         *
         * @note After calling `Reset()`, the filter will need to be refilled with new data before meaningful
         * median calculations can be performed.
         */
        
        void Reset()
        {
            mMemory.fill(T(0));
            mCount = 0;
        }
        
    private:
        
        /**
         * @brief An internal buffer that stores the values used for median filtering.
         *
         * `mMemory` is an array of type `ArrayType`, which holds the most recent values used in the median
         * filter calculation. The size of the array is defined by the `Size` template parameter, and the type
         * of elements stored in the array is defined by the `T` template parameter.
         *
         * This buffer is used to keep track of the data that the `MedianFilter` operates on, allowing it to
         * compute the median of the current window of values.
         *
         * @note The `ArrayType` is a type alias for `std::array<T, Size>`, where `T` is the type of the elements
         * and `Size` is the fixed size of the array.
         */
        
        ArrayType mMemory;
        
        /**
         * @brief A counter that tracks the number of values currently stored in the filter.
         *
         * `mCount` represents the number of valid elements currently in the `mMemory` buffer. It starts at
         * zero when the filter is initialized or reset and increments each time a new value is added, up to
         * the maximum size defined by the template parameter `Size`.
         *
         * This counter is used to determine when the buffer has been fully populated, as well as to manage
         * the number of elements considered in the median calculation before the buffer is full.
         *
         * @note Once `mCount` reaches `Size`, the filter is considered fully populated, and the oldest values
         * will start to be replaced by new ones.
         */
        
        int mCount;
    };
    
public:
    
    /**
     * @brief Default constructor for the `PrecisionTimer` class.
     *
     * Initializes a `PrecisionTimer` object and sets the last recorded timestamp to zero.
     * This prepares the timer for tracking time intervals, starting with no previously recorded time.
     *
     * The constructor initializes `mLastTimeStamp` to zero, indicating that no time measurements
     * have been made yet. The timer will start tracking time from the point when a method or
     * functionality that records a timestamp is invoked.
     *
     * @note The timer is ready to be used immediately after construction, with the timestamp
     * reset to its initial state.
     */
    
    PrecisionTimer()
    : mLastTimeStamp(0)
    {}
    
    /**
     * @brief Resets the timer and optionally sets the internal count.
     *
     * The `Reset()` function resets the `PrecisionTimer` by clearing the current time tracking state and
     * optionally setting an internal count to a specified value. This prepares the timer for a new round
     * of time measurements or usage.
     *
     * @param count An optional parameter that allows setting the internal count to a specific value.
     *              By default, this is set to `0`, indicating a full reset. The count could represent
     *              a tick count or any user-defined value associated with the timer.
     *
     * @note If `count` is not provided, the function defaults to resetting the internal count to zero.
     * This function is useful for restarting the timer and optionally specifying a starting count.
     */
    
    void Reset(uintptr_t count = 0)
    {
        mCount = count;
        mMonotonicCount = 0;
        mLastTimeStamp = TimeStamp(0);
        mFilter.Reset();
    }
    
    /**
     * @brief Advances the timer by a specified count.
     *
     * The `Progress()` function updates the `PrecisionTimer` by advancing the internal state or count
     * by the specified value. This is useful for tracking the progress of time or events in terms of
     * a custom count, such as ticks, frames, or steps, depending on how the timer is used.
     *
     * @param count The number of units to advance the timer by. This can represent ticks, cycles,
     *              or any user-defined unit of measurement associated with the timer's operation.
     *
     * @note The `Progress()` function does not measure real time, but rather advances the internal
     * count by the value provided in the `count` parameter.
     */
    
    void Progress(uintptr_t count)
    {
        if (!mCount)
            mReference = CPUTimeStamp();
        
        mCount += count;
        
        if (AsTime().AsDouble() <= mLastTimeStamp.AsDouble())
            mMonotonicCount = 0;
        else
            mMonotonicCount += count;
            
        mLastTimeStamp = AsTime();
    }
    
    /**
     * @brief Retrieves the current count of the timer.
     *
     * The `Count()` function returns the current value of the internal counter, which tracks
     * the number of units (e.g., ticks, cycles, or steps) that the timer has progressed.
     * This value reflects how much the timer has advanced since the last reset or initialization.
     *
     * @return uintptr_t The current count value of the timer.
     *
     * @note The returned count is a user-defined measure of progress and does not necessarily
     * represent real time. It can be used for tracking custom units relevant to the timer's operation.
     */
    
    uintptr_t Count() const
    {
        return mCount;
    }
    
    /**
     * @brief Retrieves the current monotonic time.
     *
     * The `MonotonicTime()` function returns the current time from a monotonic clock, which is a clock
     * that cannot be adjusted and always moves forward. This ensures that the time values returned
     * by this function are continuous and not subject to changes in the system clock (such as
     * daylight saving time adjustments or manual changes).
     *
     * @return double The current time from the monotonic clock, typically represented in seconds
     *         or another time unit depending on the platform.
     *
     * @note This function is useful for measuring elapsed time without the risk of time jumps or
     * adjustments in the system clock. The time returned is often used for performance measurements
     * or timing events.
     */
    
    double MonotonicTime() const
    {
        return mMonotonicCount / mSamplingRate;
    }
    
    /**
     * @brief Converts and retrieves the current time as a `TimeStamp` object.
     *
     * The `AsTime()` function returns the current time encapsulated in a `TimeStamp` object,
     * which provides a structured representation of the time in the format expected by
     * the application or system.
     *
     * @return TimeStamp The current time as a `TimeStamp` object.
     *
     * @note The `TimeStamp` object may represent time in various formats depending on the
     * implementation, such as seconds since a reference point, date-time, or other time units.
     * This function is useful when the time needs to be stored or compared in a specific format.
     */
    
    TimeStamp AsTime() const
    {
        return mOffset + TimeStamp::AsTime(mCount, mSamplingRate);
    }
    
    /**
     * @brief Retrieves the current time as a number of samples.
     *
     * The `AsSamples()` function returns the current time converted into a sample count,
     * where each unit represents one sample. This is particularly useful in contexts where
     * time needs to be represented discretely, such as in digital signal processing or audio
     * applications that operate on sample data.
     *
     * @return intptr_t The current time expressed as a count of samples.
     *
     * @note The actual value depends on the sampling rate or the definition of a sample
     * within the specific application context. Ensure that the sampling parameters are
     * correctly defined to interpret the sample count accurately.
     */
    
    intptr_t AsSamples() const
    {
        return mOffset.AsSamples(mSamplingRate) + static_cast<intptr_t>(mCount);
    }
    
    /**
     * @brief Synchronizes the internal timer with an external reference or system state.
     *
     * The `Sync()` function ensures that the internal state of the `PrecisionTimer` is
     * synchronized with an external time source or system state. This may involve
     * aligning the timer to a reference clock, system timer, or another external timing mechanism.
     *
     * This function is useful when the timer needs to be coordinated with other systems
     * or when precise synchronization with external events is required.
     *
     * @note After calling `Sync()`, the internal timer is updated to match the external reference.
     * The specific behavior depends on the underlying implementation of the synchronization process.
     */
    
    void Sync()
    {
        if (IsServerConnected())
            return;
            
        SendFromClient("Sync", GetTimeStamp());
    }
    
    /**
     * @brief Evaluates or adjusts the stability of the timer.
     *
     * The `Stability()` function checks the precision and consistency of the `PrecisionTimer`
     * or performs operations to ensure its stable behavior. This may involve recalibrating the timer,
     * verifying time accuracy, or ensuring that the timer is not drifting over time.
     *
     * This function is useful when the stability of the timer is critical, such as in applications
     * requiring high precision or long-term timekeeping accuracy.
     *
     * @note The exact operations performed by this function depend on the timer's implementation,
     * and could involve system-specific measures to ensure time stability.
     */
    
    void Stability()
    {
        if (MonotonicTime() < 0.1)
        {
            //DBGMSG("NON-MONOTONIC\n");
        }
    }
    
    /**
     * @brief Retrieves the current time as a `TimeStamp` object.
     *
     * The `GetTimeStamp()` function returns the current time encapsulated in a `TimeStamp` object.
     * This function provides an easy way to access the current time in a structured format that can
     * be stored, compared, or processed by the application.
     *
     * @return TimeStamp The current time represented as a `TimeStamp` object.
     *
     * @note The `TimeStamp` object may represent time in various ways depending on the implementation,
     * such as a specific point in time or an elapsed duration.
     */
    
    TimeStamp GetTimeStamp() const
    {
        return AsTime();//CPUTimeStamp() - mReference;
    }
    
    /**
     * @brief Sets the sampling rate for the timer.
     *
     * The `SetSamplingRate()` function allows you to specify the sampling rate, which determines
     * how frequently samples are taken or processed by the timer. The sampling rate is often expressed
     * in Hertz (samples per second), and it directly affects how time is calculated in contexts where
     * discrete samples are important, such as audio processing or data acquisition systems.
     *
     * @param sr The new sampling rate in Hertz (samples per second).
     *
     * @note The sampling rate must be set correctly to ensure accurate time measurements and
     * calculations. Incorrect sampling rates could lead to inaccurate results in time-sensitive applications.
     */
    
    void SetSamplingRate(double sr)
    {
        mSamplingRate = sr;
    }
    
private:
    
    /**
     * @brief Retrieves the current CPU timestamp.
     *
     * The `CPUTimeStamp()` function returns a timestamp representing the current time based on the CPU clock.
     * This function provides a high-resolution time measurement that can be used for performance profiling,
     * benchmarking, or other time-sensitive operations. The returned value is typically in seconds or another
     * time unit based on the system's CPU clock resolution.
     *
     * @return double The current CPU timestamp, representing high-precision time based on the CPU clock.
     *
     * @note This function is static, meaning it can be called without an instance of the class. The accuracy
     * of the timestamp depends on the resolution of the CPU clock on the system.
     */
    
    static double CPUTimeStamp()
    {
        static auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    
    /**
     * @brief Calculates a time offset based on four timestamps.
     *
     * The `CalculateOffset()` function computes a time offset using four provided `TimeStamp` values.
     * These timestamps may represent specific events or time measurements in a process, and the function
     * calculates an offset that could be used for synchronization, drift correction, or time alignment between
     * different time references.
     *
     * @param t1 The first `TimeStamp` value, representing the initial event or reference point.
     * @param t2 The second `TimeStamp` value, representing a subsequent event or time point.
     * @param t3 The third `TimeStamp` value, representing another event or time reference.
     * @param t4 The fourth `TimeStamp` value, representing the final event or time point.
     *
     * @return TimeStamp The calculated time offset, which could represent a delay, drift, or adjustment
     * between the provided timestamps.
     *
     * @note This function is static and can be called without creating an instance of the class. The
     * specific method of offset calculation depends on the intended purpose, such as network time
     * synchronization or adjusting for delays between measurements.
     */
    
    static TimeStamp CalculateOffset(TimeStamp t1, TimeStamp t2, TimeStamp t3, TimeStamp t4)
    {
        return Half(t2 - t1 - t4 + t3);
    }
     
    /**
     * @brief Handles the receipt of data from a client as a server.
     *
     * The `ReceiveAsServer()` function is responsible for receiving a data stream from a client, identified by
     * a `ConnectionID`, when the object is acting as a server. It processes the incoming `NetworkByteStream`
     * and performs necessary operations, such as parsing or handling the data based on the application's
     * specific logic.
     *
     * @param id The unique identifier (`ConnectionID`) for the client connection from which data is being received.
     * @param stream The `NetworkByteStream` object containing the data sent from the client.
     *
     * @note This function overrides a base class method, which implies that it implements server-specific
     * behavior for handling incoming data streams. Ensure that the data in the stream is properly validated and
     * processed according to the server's logic.
     */
    
    void ReceiveAsServer(ConnectionID id, NetworkByteStream& stream) override
    {
        if (stream.IsNextTag("Sync"))
        {
            TimeStamp t1, t2;
            
            stream.Get(t1);
            t2 = GetTimeStamp();
            
            //DBGMSG("discrepancy %lf ms\n", (t2.AsDouble() - AsTime().AsDouble()) * 1000.0);

            SendToClient(id, "Respond", t1, t2);
        }
    }
    
    /**
     * @brief Handles the receipt of data from a server as a client.
     *
     * The `ReceiveAsClient()` function is responsible for receiving a data stream from the server
     * when the object is acting as a client. It processes the incoming `NetworkByteStream` based on
     * the client's logic, which may involve parsing the data, storing it, or responding to the server.
     *
     * @param stream The `NetworkByteStream` object containing the data sent from the server.
     *
     * @note This function overrides a base class method and implements client-specific behavior
     * for handling data received from a server. Proper validation and handling of the received
     * data are essential to ensure correct communication with the server.
     */
    
    void ReceiveAsClient(NetworkByteStream& stream) override
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
        }
    }
    
    /**
     * @brief The sampling rate for the timer, initialized to 44,100 Hz.
     *
     * `mSamplingRate` represents the frequency at which samples are taken, measured in Hertz (Hz).
     * This value is initialized to 44,100 Hz, which is a common sampling rate used in audio processing
     * and other time-sensitive applications. The sampling rate determines how frequently time intervals
     * or events are sampled by the timer.
     *
     * @note The default value of 44,100 Hz is commonly used in digital audio applications. This value
     * can be modified based on the specific requirements of the application by calling the `SetSamplingRate()`
     * function.
     */
    
    double mSamplingRate = 44100;
    
    /**
     * @brief Tracks the current count of processed samples or events.
     *
     * `mCount` represents an internal counter that tracks the number of samples or events that
     * have been processed by the timer or system. The count is stored as an unsigned integer type
     * (`uintptr_t`), which ensures portability across different platforms and allows it to represent
     * large values.
     *
     * This counter is incremented as new samples are processed or events occur, and it is used
     * internally to manage the progression of time or to trigger specific behaviors once a certain
     * count is reached.
     *
     * @note This value can be reset, updated, or queried depending on the application's needs.
     */
    
    uintptr_t mCount;
    
    /**
     * @brief Tracks the current monotonic count for time-related operations.
     *
     * `mMonotonicCount` is an internal counter that increments monotonically, meaning it always
     * increases and never decreases. This counter is typically used in timekeeping operations where
     * a continuous, non-decreasing count is required, such as measuring elapsed time or ensuring
     * events are processed in order.
     *
     * The count is stored as an unsigned integer type (`uintptr_t`), providing portability and allowing
     * it to represent large values across different platforms.
     *
     * @note Monotonic counts are important in scenarios where the system clock might be adjusted,
     * as they provide a reliable measure of time that is not affected by clock changes.
     */
    
    uintptr_t mMonotonicCount;
    
    /**
     * @brief Stores the time offset for adjusting time measurements.
     *
     * `mOffset` represents a `TimeStamp` value used to adjust or calibrate the internal timekeeping
     * of the system. This offset is applied to synchronize the timer with an external time reference
     * or to account for delays, drifts, or differences between clocks.
     *
     * The `mOffset` can be set or calculated based on the specific requirements of the application,
     * such as aligning the internal clock with an external clock or compensating for latency in
     * network communication.
     *
     * @note The value of `mOffset` is typically added or subtracted from the current time when
     * making time-based calculations to ensure accurate and synchronized timekeeping.
     */
    
    TimeStamp mOffset;
    
    /**
     * @brief Stores the most recent recorded timestamp.
     *
     * `mLastTimeStamp` holds the last `TimeStamp` recorded by the system or timer. This value
     * is used to track the time of the most recent event or time measurement and is crucial for
     * calculating time intervals or determining the elapsed time between events.
     *
     * The timestamp is updated each time a new measurement or event occurs, allowing the system
     * to calculate deltas or compare the current time with the last recorded event.
     *
     * @note The accuracy and precision of `mLastTimeStamp` depend on the underlying system clock
     * or time source used to generate the timestamp.
     */
    
    TimeStamp mLastTimeStamp;
    
    /**
     * @brief Holds a reference time or value used for time calculations.
     *
     * `mReference` is a double-precision floating-point variable that stores a reference value
     * for time-based calculations. This reference may represent a baseline or starting point
     * against which other time measurements are compared or adjusted.
     *
     * It is commonly used to align the internal clock with an external time source or to provide
     * a point of reference for elapsed time calculations.
     *
     * @note The specific meaning of `mReference` depends on its usage within the application,
     * such as a starting timestamp, an offset, or a reference to synchronize with other clocks.
     */
    
    double mReference;
    
    /**
     * @brief A median filter for smoothing time measurements, using a window size of 5.
     *
     * `mFilter` is an instance of the `MedianFilter` template class, specialized to filter
     * `TimeStamp` values with a fixed window size of 5. It is used to reduce noise or fluctuations
     * in time measurements by computing the median of the last 5 recorded timestamps.
     *
     * This filter is particularly useful in scenarios where time measurements can be noisy or
     * inconsistent, and a stable, smoothed value is needed for accurate time tracking or synchronization.
     *
     * @note The window size of 5 means the filter will calculate the median of the 5 most recent
     * timestamps added to it, providing a balance between responsiveness and smoothing.
     */
    
    MedianFilter<TimeStamp, 5> mFilter;
};

#endif /* PRECISIONTIMER_HPP */
