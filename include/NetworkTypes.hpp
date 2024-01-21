
#ifndef NETWORKTYPES_HPP
#define NETWORKTYPES_HPP

#include "../dependencies/websocket-tools/websocket-tools.hpp"

#include <mutex.h>

struct NetworkTypes
{
protected:
    
    using ConnectionID = ws_connection_id;

    using RecursiveMutex = WDL_Mutex;
    using RecursiveLock = WDL_MutexLock;
    using SharedMutex = WDL_SharedMutex;
    using SharedLock = WDL_MutexLockShared;
    using ExclusiveLock = WDL_MutexLockExclusive;
    
    class VariableLock
    {
    public:
        
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
        
        ~VariableLock()
        {
            Destroy();
        }
        
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
        
        void Promote()
        {
            if (mMutex && mShared)
            {
                mMutex->SharedToExclusive();
                mShared = false;
            }
        }
        
        void Demote()
        {
            if (mMutex && !mShared)
            {
                mMutex->ExclusiveToShared();
                mShared = true;
            }
        }
        
        VariableLock(VariableLock const& rhs) = delete;
        VariableLock(VariableLock const&& rhs) = delete;
        void operator = (VariableLock const& rhs) = delete;
        void operator = (VariableLock const&& rhs) = delete;
        
    private:
        
        SharedMutex *mMutex;
        bool mShared;
    };
};

#endif /* NETWORKTYPES_HPP */
