
/**
 * @file NetworkTypes.hpp
 *
 * @brief Defines various types and utilities for managing network-related operations.
 *
 * This header file contains type definitions, aliases, and structures used
 * throughout the network communication components of the system. It includes
 * mutex and lock abstractions to handle concurrency control, ensuring thread-safe
 * access to shared resources during network operations. The types defined here
 * streamline network communication by providing clear and reusable abstractions
 * for managing connections, synchronization, and resource access.
 *
 * Key components include:
 * - Type aliases for mutex and lock management (RecursiveMutex, SharedMutex, etc.)
 * - The VariableLock class, which allows for flexible lock behavior (shared or exclusive)
 * - Concurrency primitives for thread-safe network interactions
 */

#ifndef NETWORKTYPES_HPP
#define NETWORKTYPES_HPP

#include "../dependencies/websocket-tools/include/websocket-tools.hpp"

#include <mutex.h>

/**
 * @struct NetworkTypes
 *
 * @brief Defines a collection of types used in network communication.
 *
 * This structure serves as a container for various type definitions
 * and utilities related to network operations. It provides type
 * aliases, constants, and other elements necessary for handling
 * network data and connections within the system.
 */

struct NetworkTypes
{
protected:
    
    /**
     * @typedef ConnectionID
     *
     * @brief Alias for the ws_connection_id type.
     *
     * This type alias is used to represent a WebSocket connection identifier.
     * It simplifies the code by providing a more descriptive name for ws_connection_id.
     */
    
    using ConnectionID = ws_connection_id;

    /**
     * @typedef RecursiveMutex
     *
     * @brief Alias for the WDL_Mutex type, representing a recursive mutex.
     *
     * This type alias simplifies the usage of WDL_Mutex for cases where
     * a recursive mutex is needed. A recursive mutex allows the same thread
     * to acquire the lock multiple times without causing a deadlock, which
     * can be useful in certain network operations and concurrency control scenarios.
     */
    
    using RecursiveMutex = WDL_Mutex;
    
    /**
     * @typedef RecursiveLock
     *
     * @brief Alias for the WDL_MutexLock type, representing a lock for a recursive mutex.
     *
     * This type alias is used to simplify the management of locking mechanisms
     * for recursive mutexes. It ensures that a recursive mutex (WDL_Mutex)
     * is properly locked and unlocked in a scoped manner, promoting thread
     * safety during network operations or other concurrent processes.
     */
    
    using RecursiveLock = WDL_MutexLock;
    
    /**
     * @typedef SharedMutex
     *
     * @brief Alias for the WDL_SharedMutex type, representing a shared mutex.
     *
     * This type alias is used for managing shared access to resources.
     * A shared mutex allows multiple threads to read from the same resource
     * simultaneously while ensuring that only one thread can write to the resource
     * at any given time. It is particularly useful in scenarios where read-heavy
     * operations need to be optimized for concurrency without compromising data integrity.
     */
    
    using SharedMutex = WDL_SharedMutex;
    
    /**
     * @typedef SharedLock
     *
     * @brief Alias for the WDL_MutexLockShared type, representing a lock for shared mutexes.
     *
     * This type alias is used to manage shared locks on resources protected by a shared mutex
     * (WDL_SharedMutex). SharedLock allows multiple threads to acquire a read lock concurrently,
     * ensuring that read operations can occur in parallel, while still preventing write access
     * during these operations. It is useful in situations where read-heavy operations require
     * efficient concurrency control.
     */
    
    using SharedLock = WDL_MutexLockShared;
    
    /**
     * @typedef ExclusiveLock
     *
     * @brief Alias for the WDL_MutexLockExclusive type, representing an exclusive lock.
     *
     * This type alias is used to manage exclusive locks on resources protected by a shared mutex
     * (WDL_SharedMutex). An ExclusiveLock ensures that only one thread can acquire the lock at a time,
     * allowing write access to the resource while preventing any other thread from accessing it concurrently.
     * It is critical for maintaining data integrity during write operations that require exclusive access.
     */
    
    using ExclusiveLock = WDL_MutexLockExclusive;
    
    /**
     * @class VariableLock
     *
     * @brief A class that provides a flexible locking mechanism.
     *
     * The VariableLock class is designed to offer variable locking behavior,
     * allowing either shared or exclusive access to resources, depending on
     * the context. It simplifies the process of managing locks in scenarios
     * where different levels of access (shared or exclusive) are needed.
     * This class is useful in multi-threaded environments where resource
     * access must be carefully controlled to maintain data integrity and
     * prevent race conditions.
     */
    
    class VariableLock
    {
    public:
        
        /**
         * @brief Constructs a VariableLock object with the specified mutex and lock type.
         *
         * This constructor initializes a VariableLock object, allowing it to acquire either
         * a shared or exclusive lock based on the provided parameters. By default, a shared
         * lock is acquired unless otherwise specified.
         *
         * @param mutex A pointer to the SharedMutex that will be locked. This mutex will
         *              control access to the shared resource.
         * @param shared A boolean flag indicating whether to acquire a shared lock (true) or
         *               an exclusive lock (false). Defaults to true (shared lock).
         */
        
        VariableLock(SharedMutex* mutex, bool shared = true)
        : mMutex(mutex)
        , mShared(shared)
        {
            if (mMutex)
            {
                if (shared)
                    mMutex->LockShared();
                else
                    mMutex->LockExclusive();
            }
        }
        
        /**
         * @brief Destroys the VariableLock object and releases the acquired lock.
         *
         * This destructor ensures that the lock (either shared or exclusive) held by the
         * VariableLock object is properly released when the object goes out of scope.
         * It helps maintain thread safety by automatically unlocking the associated
         * SharedMutex upon object destruction.
         */
        
        ~VariableLock()
        {
            Destroy();
        }
        
        /**
         * @brief Releases the lock held by the VariableLock object.
         *
         * The Destroy() method explicitly releases the lock (either shared or exclusive)
         * held by the VariableLock object. It provides an option to manually unlock
         * the associated SharedMutex, allowing for more fine-grained control over
         * the lock's lifecycle. This method should be called when the lock is no
         * longer needed before the VariableLock object goes out of scope.
         */
        
        void Destroy()
        {
            if (mMutex)
            {
                if (mShared)
                    mMutex->UnlockShared();
                else
                    mMutex->UnlockExclusive();
                    
                mMutex = nullptr;
            }
        }
        
        /**
         * @brief Promotes a shared lock to an exclusive lock.
         *
         * The Promote() method upgrades the current lock from a shared lock to an exclusive lock.
         * This allows the thread to gain exclusive access to the resource after initially
         * holding a shared lock. It is useful in situations where a thread needs read access
         * but later requires write access to the resource without releasing the lock.
         *
         * @note This operation may block if another thread already holds an exclusive lock.
         */
        
        void Promote()
        {
            if (mMutex && mShared)
            {
                mMutex->SharedToExclusive();
                mShared = false;
            }
        }
        
        /**
         * @brief Demotes an exclusive lock to a shared lock.
         *
         * The Demote() method downgrades the current lock from an exclusive lock to a shared lock.
         * This allows the thread to continue holding a shared lock (read access) after it no longer
         * requires exclusive access (write access) to the resource. It is useful for optimizing
         * performance in scenarios where a thread initially needs to modify the resource but
         * later only needs to read from it.
         *
         * @note This operation permits other threads to acquire shared locks once the demotion is complete.
         */
        
        void Demote()
        {
            if (mMutex && !mShared)
            {
                mMutex->ExclusiveToShared();
                mShared = true;
            }
        }
        
        /**
         * @brief Deleted copy constructor for VariableLock.
         *
         * This copy constructor is explicitly deleted to prevent copying of VariableLock objects.
         * Since a lock is a unique resource that cannot be safely duplicated across multiple
         * instances, copying could lead to undefined behavior or race conditions.
         * The VariableLock object must be used exclusively by one owner.
         *
         * @param rhs The VariableLock object to be copied (not allowed).
         */
        
        VariableLock(VariableLock const& rhs) = delete;
        
        /**
         * @brief Deleted move constructor for VariableLock.
         *
         * This move constructor is explicitly deleted to prevent moving of VariableLock objects.
         * Since moving a lock could result in transferring ownership of a resource lock in
         * a way that might cause issues with thread safety, this is disallowed. The VariableLock
         * object must be used exclusively by the original owner, ensuring proper management of the lock.
         *
         * @param rhs The rvalue reference to the VariableLock object to be moved (not allowed).
         */
        
        VariableLock(VariableLock const&& rhs) = delete;
        
        /**
         * @brief Deleted copy assignment operator for VariableLock.
         *
         * This copy assignment operator is explicitly deleted to prevent assigning one
         * VariableLock object to another. Since locks represent unique resources,
         * assigning a lock could lead to unsafe behavior, such as multiple objects
         * managing the same lock, causing race conditions or deadlocks.
         * The VariableLock object must not be assigned to another instance.
         *
         * @param rhs The VariableLock object to be assigned (not allowed).
         */
        
        void operator = (VariableLock const& rhs) = delete;
        
        /**
         * @brief Deleted move assignment operator for VariableLock.
         *
         * This move assignment operator is explicitly deleted to prevent moving the ownership
         * of a VariableLock object. Moving a lock can result in transferring the management
         * of a lock resource in an unsafe manner, potentially causing thread safety issues
         * such as race conditions or deadlocks. As a result, VariableLock objects cannot
         * be moved or reassigned.
         *
         * @param rhs The rvalue reference to the VariableLock object to be moved (not allowed).
         */
        
        void operator = (VariableLock const&& rhs) = delete;
        
    private:
        
        /**
         * @brief Pointer to the shared mutex used by the VariableLock.
         *
         * This member variable holds a pointer to the SharedMutex object that the
         * VariableLock manages. The mutex is used to control access to shared resources,
         * ensuring that threads either acquire a shared or exclusive lock as needed.
         * The VariableLock class uses this pointer to lock or unlock the resource
         * during its lifetime.
         */
        
        SharedMutex *mMutex;
        
        /**
         * @brief Indicates whether the lock is shared or exclusive.
         *
         * This boolean member variable determines the type of lock held by the VariableLock object.
         * If true, the lock is a shared lock, allowing multiple threads to read from the resource
         * concurrently. If false, the lock is exclusive, granting write access to only one thread
         * at a time. This flag helps control the lock behavior during the VariableLock's lifetime.
         */
        
        bool mShared;
    };
};

#endif /* NETWORKTYPES_HPP */
