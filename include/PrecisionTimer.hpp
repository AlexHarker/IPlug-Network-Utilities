
/**
 * @file PrecisionTimer.hpp
 * @brief Defines the PrecisionTimer class for high-precision time management and synchronization.
 *
 * This file contains the declaration and implementation of the PrecisionTimer class,
 * which is used for high-precision timekeeping in networked environments. The class
 * supports features such as time synchronization between peers, accurate timestamping,
 * and handling both client and server interactions for time-related operations.
 *
 * Key components of this file include:
 * - The PrecisionTimer class, which inherits from NetworkPeer and provides advanced timekeeping capabilities.
 * - Methods for setting the sampling rate, resetting the timer, processing data as both client and server,
 *   and retrieving accurate timestamps.
 * - The use of a median filter (`MedianFilter<TimeStamp, 5>`) to smooth timestamp values and improve
 *   time measurement stability.
 *
 * Additionally, the file includes several member variables such as `mLastTimeStamp`, `mOffset`, and
 * `mMonotonicCount` that aid in maintaining the accuracy and consistency of time calculations.
 *
 * @note The PrecisionTimer class is designed for use in applications where precise time management is critical,
 * such as audio processing, distributed systems, or network-based applications.
 */

#ifndef PRECISIONTIMER_HPP
#define PRECISIONTIMER_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

#include "NetworkPeer.hpp"

/**
 * @class PrecisionTimer
 * @brief A high-precision timer class that inherits from NetworkPeer.
 *
 * The PrecisionTimer class provides functionality for high-accuracy timekeeping and
 * synchronization within a networked environment. It inherits from the NetworkPeer class,
 * enabling network-related capabilities such as time synchronization across peers.
 *
 * @note This class leverages the functionality of NetworkPeer for its network operations.
 */

class PrecisionTimer : public NetworkPeer
{
    
    /**
     * @class MedianFilter
     * @tparam T The data type of the elements to be filtered.
     * @tparam Size The number of elements in the filter window.
     *
     * @brief A template class implementing a median filter.
     *
     * The MedianFilter class is used to apply a median filtering algorithm to a series of
     * data points. It maintains a sliding window of a fixed size (defined by the template
     * parameter `Size`) and calculates the median of the values in this window. Median filters
     * are useful for reducing noise in signals by smoothing data while preserving edges.
     *
     * @note The performance of the filter is dependent on the size of the window, `Size`.
     */
    
    template <class T, int Size>
    class MedianFilter
    {
        
        /**
         * @typedef ArrayType
         *
         * A type alias for a `std::array` of elements of type `T` with a fixed size of `Size`.
         *
         * @tparam T The type of elements contained in the array.
         * @tparam Size The fixed size of the array.
         *
         * This alias simplifies the declaration and usage of fixed-size arrays.
         */
        
        using ArrayType = std::array<T, Size>;
        
        /**
         * @struct CompareIndices
         *
         * @brief A comparator structure used to compare indices based on associated values.
         *
         * The CompareIndices structure is designed to facilitate comparison operations between
         * indices. It is typically used in algorithms where sorting or comparing indices
         * based on some associated data or values is required. This structure can be customized
         * to compare indices according to specific criteria, such as ascending or descending order.
         *
         * @note The exact comparison logic depends on the implementation within this structure.
         */
        
        struct CompareIndices
        {
            
            /**
             * @brief Constructor for the CompareIndices struct.
             *
             * Initializes the CompareIndices object with a reference to an array of values.
             * The array is used to compare the indices during sorting or comparison operations.
             *
             * @param array A constant reference to an array (of type `ArrayType`) that holds
             * the values on which the comparison between indices will be based.
             *
             * @note The provided array is stored as a reference and is not copied.
             */
            
            CompareIndices(const ArrayType& array)
            : mArray(array)
            {}
            
            /**
             * @brief Overloaded function call operator for comparing two indices.
             *
             * Compares the values at two given indices in the array and returns `true`
             * if the value at `idx1` is less than the value at `idx2`. This operator
             * allows the CompareIndices object to be used as a comparator in algorithms
             * such as sorting, where elements are compared based on the values in the array.
             *
             * @param idx1 The first index to compare.
             * @param idx2 The second index to compare.
             * @return `true` if the value at `idx1` is less than the value at `idx2`,
             * otherwise `false`.
             *
             * @note This operation does not modify the array and is marked as `const`.
             */
            
            bool operator()(int idx1, int idx2) const
            {
                return mArray[idx1] < mArray[idx2];
            }
            
            /**
             * @brief A constant reference to the array used for index comparison.
             *
             * This member holds a reference to an array of values (of type `ArrayType`)
             * that is used by the `CompareIndices` structure to compare indices. The values
             * in the array determine the sorting or comparison results when the comparison
             * operator is invoked.
             *
             * @note Since this is a constant reference, the array cannot be modified through
             * this member.
             */
            
            const ArrayType& mArray;
        };
        
    public:
        
        /**
         * @brief Default constructor for the MedianFilter class.
         *
         * Initializes a new instance of the MedianFilter class. This constructor sets up
         * the internal data structures but does not perform any filtering until data is added.
         *
         * @note The filter window will be initialized, but it will remain empty until elements
         * are added for filtering.
         */
        
        MedianFilter()
        {
            Reset();
        }
          
        /**
         * @brief Overloaded function call operator to apply the median filter to an input value.
         *
         * This operator processes the input value by adding it to the filter's internal buffer
         * and then computes the median of the values within the filter window. The median value
         * is returned as the result of the filtering operation.
         *
         * @param in A constant reference to the input value of type `T` to be added to the filter.
         * @return A constant reference to the median value of the current filter window.
         *
         * @note The filter window updates with each new input value, and the returned value is the
         * median of the values in the window.
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
         * @brief Resets the MedianFilter to its initial state.
         *
         * This method clears the internal buffer of the MedianFilter, effectively
         * removing all previously added values. After calling this method, the filter
         * will act as if it has just been constructed, with no data in its window.
         *
         * @note This method should be used when you want to discard the current
         * filter state and start fresh with new data.
         */
        
        void Reset()
        {
            mMemory.fill(T(0));
            mCount = 0;
        }
        
    private:
        
        /**
         * @brief An array used to store the values in the filter window.
         *
         * The `mMemory` member holds the data currently being processed by the
         * MedianFilter. It stores the values that are used to compute the median
         * within the filter window. The size of this array is fixed and defined by
         * the template parameter `Size` of the MedianFilter class.
         *
         * @note This array represents the internal memory of the filter, and its
         * contents are updated as new values are added.
         */
        
        ArrayType mMemory;
        
        /**
         * @brief A counter that tracks the number of elements added to the filter.
         *
         * The `mCount` member stores the number of values that have been added to the
         * MedianFilter so far. This is used to keep track of how many valid entries
         * are currently in the filter window, especially when the filter is still filling
         * up after being reset or constructed.
         *
         * @note Once `mCount` reaches the size of the filter window, it indicates that the filter
         * is fully populated, and median calculations will include all elements in the window.
         */
        
        int mCount;
    };
    
public:
    
    /**
     * @brief Constructor for the PrecisionTimer class.
     *
     * Initializes a new instance of the PrecisionTimer class with a specified registration name and port number.
     * The constructor also initializes the base class `NetworkPeer` with the same parameters.
     *
     * @param regname A C-string representing the registration name for the network peer.
     * @param port The port number used for network communication. Defaults to `8001` if not specified.
     *
     * The constructor also initializes the internal timestamp `mLastTimeStamp` to 0,
     * setting up the timer for future time-based operations.
     *
     * @note The registration name is passed to the `NetworkPeer` base class, which handles the network initialization.
     */
    
    PrecisionTimer(const char *regname, uint16_t port = 8001)
    : NetworkPeer(regname, port)
    , mLastTimeStamp(0)
    {}
    
    /**
     * @brief Resets the PrecisionTimer to its initial state with an optional count value.
     *
     * This method resets the internal state of the PrecisionTimer, allowing the timer to start fresh.
     * The optional `count` parameter can be used to set the initial value of the internal counter.
     *
     * @param count An optional parameter (default is `0`) that represents the initial value of the counter
     * or timestamp after the reset. If not provided, the counter will be reset to 0.
     *
     * @note This method is useful when you need to restart the timer or adjust the initial state
     * of the internal timestamp.
     */
    
    void Reset(uintptr_t count = 0)
    {
        mCount = count;
        mMonotonicCount = 0;
        mLastTimeStamp = TimeStamp(0);
        mFilter.Reset();
    }
    
    /**
     * @brief Updates the internal state of the PrecisionTimer with the given count value.
     *
     * This method progresses the PrecisionTimer by updating its internal counter or timestamp
     * based on the provided `count` value. It is used to track the passage of time or
     * synchronize with external events by adjusting the internal state.
     *
     * @param count The current value used to update the internal counter or timestamp.
     *
     * @note The `Progress` method is typically called periodically or when an external
     * event occurs, to keep the timer synchronized with the passage of time or system events.
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
     * @brief Retrieves the current value of the internal counter or timestamp.
     *
     * This method returns the current value of the internal counter or timestamp maintained
     * by the PrecisionTimer. It provides read-only access to the current state without modifying
     * the internal value.
     *
     * @return The current counter or timestamp value as an unsigned integral type (`uintptr_t`).
     *
     * @note Since this method is marked as `const`, it does not modify any member variables
     * of the PrecisionTimer.
     */
    
    uintptr_t Count() const
    {
        return mCount;
    }
    
    /**
     * @brief Retrieves the current monotonic time in seconds.
     *
     * This method returns the current monotonic time, which represents a continuously increasing
     * time value that is not affected by system clock adjustments (such as changes in time zone
     * or daylight saving time). It is useful for measuring elapsed time in a reliable manner.
     *
     * @return The current monotonic time as a `double`, in seconds.
     *
     * @note Since this method is marked as `const`, it does not modify any member variables
     * of the PrecisionTimer.
     */
    
    double MonotonicTime() const
    {
        return mMonotonicCount / mSamplingRate;
    }
    
    /**
     * @brief Converts and retrieves the current internal timestamp as a TimeStamp object.
     *
     * This method returns the current state of the PrecisionTimer's internal counter
     * or timestamp in the form of a `TimeStamp` object. The returned value can be used
     * for precise time-based calculations or comparisons.
     *
     * @return A `TimeStamp` object representing the current internal timestamp.
     *
     * @note Since this method is marked as `const`, it does not modify the internal state
     * of the PrecisionTimer.
     */
    
    TimeStamp AsTime() const
    {
        return mOffset + TimeStamp::AsTime(mCount, mSamplingRate);
    }
    
    /**
     * @brief Retrieves the current internal timestamp as a number of samples.
     *
     * This method converts the current internal timestamp of the PrecisionTimer
     * into a number of samples, typically used in audio or signal processing contexts
     * where time is represented in sample units.
     *
     * @return The current timestamp as an `intptr_t`, representing the number of samples.
     *
     * @note Since this method is marked as `const`, it does not modify the internal state
     * of the PrecisionTimer.
     */
    
    intptr_t AsSamples() const
    {
        return mOffset.AsSamples(mSamplingRate) + static_cast<intptr_t>(mCount);
    }
    
    /**
     * @brief Synchronizes the PrecisionTimer with an external time source or event.
     *
     * This method synchronizes the internal state of the PrecisionTimer, allowing it
     * to align with an external time reference or event. This is useful in distributed
     * systems or networked environments where multiple peers need to synchronize their
     * timers for accurate timekeeping or coordination.
     *
     * @note The exact behavior of the synchronization depends on the underlying implementation
     * and the external time source being used.
     */
    
    void Sync()
    {
        if (IsConnectedAsClient())
            SendFromClient("Sync", GetTimeStamp());
    }
    
    /**
     * @brief Evaluates or adjusts the stability of the PrecisionTimer.
     *
     * This method is used to assess or enhance the stability of the PrecisionTimer,
     * ensuring that it operates consistently over time without fluctuations or drifts
     * in its timekeeping accuracy. This can involve internal checks or adjustments
     * based on the current state of the timer or external synchronization sources.
     *
     * @note The exact behavior of this method depends on the underlying implementation
     * and may involve hardware or software adjustments to maintain stable timing.
     */
    
    void Stability()
    {
        if (MonotonicTime() < 0.1)
        {
            //DBGMSG("NON-MONOTONIC\n");
        }
    }
    
    /**
     * @brief Retrieves the current timestamp from the PrecisionTimer.
     *
     * This method returns the current timestamp as a `TimeStamp` object, representing
     * the precise moment in time according to the internal state of the PrecisionTimer.
     * The timestamp can be used for time-based calculations, logging, or synchronization
     * tasks.
     *
     * @return A `TimeStamp` object representing the current time.
     *
     * @note This method is marked as `const`, meaning it does not modify the internal state
     * of the PrecisionTimer.
     */
    
    TimeStamp GetTimeStamp() const
    {
        return AsTime();//CPUTimeStamp() - mReference;
    }
    
    /**
     * @brief Sets the sampling rate for the PrecisionTimer.
     *
     * This method allows you to define the sampling rate, `sr`, used by the PrecisionTimer.
     * The sampling rate is essential in contexts such as audio processing or other
     * time-sensitive applications, where time is often represented in terms of samples
     * per second.
     *
     * @param sr The new sampling rate to be set, expressed in samples per second (Hz).
     *
     * @note Changing the sampling rate may affect the way time or sample calculations are performed
     * within the PrecisionTimer, so it is important to ensure the correct sampling rate is set
     * for your specific use case.
     */
    
    void SetSamplingRate(double sr)
    {
        mSamplingRate = sr;
    }
    
protected:
    
    /**
     * @brief Processes incoming data from a client connection when acting as a server.
     *
     * This method handles the processing of data received from a client identified by
     * the `id` parameter. The data is provided through the `NetworkByteStream` object
     * `stream`. The method is typically used when the PrecisionTimer is operating as a
     * server, processing requests or synchronizing time with clients.
     *
     * @param id The unique identifier (`ConnectionID`) for the client connection.
     * @param stream A reference to a `NetworkByteStream` object that contains the data
     * sent by the client.
     * @return `true` if the processing was successful, `false` otherwise.
     *
     * @note This method is crucial for server-side operations, ensuring that the server
     * processes incoming data and responds appropriately.
     */
    
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
    
    /**
     * @brief Processes outgoing data when acting as a client.
     *
     * This method handles the preparation and processing of data that the PrecisionTimer sends
     * to a server when operating as a client. The data to be transmitted is provided through the
     * `NetworkByteStream` object `stream`.
     *
     * @param stream A reference to a `NetworkByteStream` object that contains the data
     * to be sent to the server.
     * @return `true` if the processing and data transmission were successful, `false` otherwise.
     *
     * @note This method is essential for client-side operations, ensuring that the client
     * prepares and transmits data to the server appropriately.
     */
    
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
    
    /**
     * @brief Retrieves the current CPU timestamp in seconds.
     *
     * This static method returns a high-resolution timestamp based on the CPU clock.
     * The CPU timestamp is useful for measuring elapsed time with high precision,
     * particularly for performance profiling and timing operations.
     *
     * @return A `double` representing the current CPU timestamp in seconds.
     *
     * @note Since this method is static, it can be called without an instance of
     * PrecisionTimer and is commonly used for precise time measurements across the system.
     */
    
    static double CPUTimeStamp()
    {
        static auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    
    /**
     * @brief Calculates the time offset based on a set of timestamps from a client-server interaction.
     *
     * This static method computes the time offset between two systems (e.g., client and server)
     * by using four timestamps. These timestamps typically represent the moments when requests
     * and responses were sent and received, allowing for synchronization between systems.
     *
     * @param t1 The timestamp representing the time the request was sent from the client.
     * @param t2 The timestamp representing the time the request was received by the server.
     * @param t3 The timestamp representing the time the response was sent from the server.
     * @param t4 The timestamp representing the time the response was received by the client.
     *
     * @return A `TimeStamp` object representing the calculated offset between the two systems.
     *
     * @note This method is static, meaning it can be called without an instance of PrecisionTimer,
     * and it is useful for time synchronization across networked systems.
     */
    
    static TimeStamp CalculateOffset(TimeStamp t1, TimeStamp t2, TimeStamp t3, TimeStamp t4)
    {
        return Half(t2 - t1 - t4 + t3);
    }
     
    /**
     * @brief Handles incoming data when acting as a server.
     *
     * This method is called to process data received from a client connection when
     * the PrecisionTimer is acting as a server. It overrides the base class method
     * to provide server-specific functionality. The method processes the data contained
     * in the `NetworkByteStream` and performs any necessary actions, such as updating
     * the internal state or responding to the client.
     *
     * @param id The unique identifier (`ConnectionID`) for the client connection.
     * @param stream A reference to a `NetworkByteStream` object that contains the data
     * sent by the client.
     *
     * @note This method overrides a base class method to implement server-specific
     * data handling logic.
     */
    
    void ReceiveAsServer(ConnectionID id, NetworkByteStream& stream) override
    {
        ProcessAsServer(id, stream);
    }
    
    /**
     * @brief Handles incoming data when acting as a client.
     *
     * This method is called to process data received from the server when the PrecisionTimer
     * is acting as a client. It overrides the base class method to provide client-specific
     * functionality. The method processes the data contained in the `NetworkByteStream`,
     * typically to update the client's internal state or synchronize with the server.
     *
     * @param stream A reference to a `NetworkByteStream` object that contains the data
     * received from the server.
     *
     * @note This method overrides a base class method to implement client-specific
     * data handling logic.
     */
    
    void ReceiveAsClient(NetworkByteStream& stream) override
    {
        ProcessAsClient(stream);
    }
    
    /**
     * @brief The sampling rate used by the PrecisionTimer, initialized to 44.1 kHz.
     *
     * This member variable stores the sampling rate, which determines how time is
     * measured in terms of samples per second. The default value is set to 44,100 Hz
     * (44.1 kHz), a common sampling rate used in audio processing.
     *
     * @note The sampling rate can be changed using the `SetSamplingRate` method if
     * a different rate is required for specific applications.
     */
    
    double mSamplingRate = 44100;
    
    /**
     * @brief An internal counter used to track the current count or timestamp.
     *
     * This member variable stores an unsigned integer that represents the internal
     * count or timestamp value used by the PrecisionTimer. It is used to measure
     * the passage of time or track events based on sample counts or other time-related
     * values.
     *
     * @note The value of `mCount` can be accessed or modified by various methods,
     * such as `Progress()` and `Reset()`, to update or reset the timer's internal state.
     */
    
    uintptr_t mCount;
    
    /**
     * @brief An internal counter used to track the monotonic time count.
     *
     * This member variable stores an unsigned integer representing the monotonic
     * time count. Monotonic time ensures a steadily increasing count that is unaffected
     * by system clock changes, such as time zone adjustments or daylight saving time.
     *
     * @note The value of `mMonotonicCount` is typically used for precise time measurements
     * and synchronization tasks, where a continuous and stable time count is required.
     */
    
    uintptr_t mMonotonicCount;
    
    /**
     * @brief Stores the time offset used for synchronization.
     *
     * This member variable holds a `TimeStamp` object that represents the offset
     * between the local time and a reference time, typically used for synchronizing
     * the PrecisionTimer with an external time source (e.g., a server or another peer).
     *
     * @note The value of `mOffset` is used in time calculations to adjust the local
     * time based on the offset, ensuring proper synchronization with external systems.
     */
    
    TimeStamp mOffset;
    
    /**
     * @brief Stores the last recorded timestamp.
     *
     * This member variable holds a `TimeStamp` object representing the most recent
     * timestamp recorded by the PrecisionTimer. It is used to track the time of
     * the last event or update, allowing for calculations of elapsed time or
     * synchronization with future timestamps.
     *
     * @note `mLastTimeStamp` is updated as new events occur, and it plays a critical
     * role in maintaining the timer's accuracy and consistency.
     */
    
    TimeStamp mLastTimeStamp;
    
    /**
     * @brief Stores a reference time or value for time calculations.
     *
     * This member variable holds a `double` representing a reference time or value used
     * for various time-based calculations within the PrecisionTimer. It serves as a
     * baseline or point of comparison for measuring elapsed time, synchronizing with
     * external time sources, or other time-dependent operations.
     *
     * @note The value of `mReference` is typically set during initialization or synchronization
     * and is used in conjunction with other time variables like `mLastTimeStamp` to ensure
     * accurate timekeeping.
     */
    
    double mReference;
    
    /**
     * @brief A median filter used to smooth out time-related data.
     *
     * This member variable is an instance of `MedianFilter` that operates on `TimeStamp`
     * values with a fixed window size of 5. The median filter helps reduce noise and
     * fluctuations in time measurements by computing the median of the last 5 timestamps.
     * It is useful for ensuring more stable and accurate time synchronization.
     *
     * @note The filter's window size is fixed at 5, meaning it will maintain the last
     * 5 timestamps for processing and calculating the median.
     */
    
    MedianFilter<TimeStamp, 5> mFilter;
};

#endif /* PRECISIONTIMER_HPP */
