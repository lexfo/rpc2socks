// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {

template <typename MutexT>
class lock_guard : private noncopyable
{
    // implements the lock() and unlock() methods in addition of the semantics
    // of std::lock_guard

public:
    typedef MutexT mutex_type;

public:
    lock_guard() = delete;

    explicit lock_guard(MutexT& mutex)
        : m_mutex(mutex)
        , m_locked{true}
#ifdef CIX_DEBUG
        , m_tid(std::this_thread::get_id())
#endif
        { m_mutex.lock(); }

    // construct but do not lock
    explicit lock_guard(MutexT& mutex, std::defer_lock_t)
        : m_mutex(mutex)
        , m_locked{false}
#ifdef CIX_DEBUG
        , m_tid(std::this_thread::get_id())
#endif
        { }

    // assume current thread owns this mutex
    explicit lock_guard(MutexT& mutex, std::adopt_lock_t)
        : m_mutex(mutex)
        , m_locked{true}
#ifdef CIX_DEBUG
        , m_tid(std::this_thread::get_id())
#endif
        { }

    ~lock_guard()
    {
#ifdef CIX_DEBUG
        assert(std::this_thread::get_id() == m_tid);
#endif
        this->unlock();
    }

    void lock()
    {
#ifdef CIX_DEBUG
        assert(std::this_thread::get_id() == m_tid);
        assert(m_locked == false);
#endif

        if (!m_locked)
        {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock()
    {
#ifdef CIX_DEBUG
        assert(std::this_thread::get_id() == m_tid);
#endif

        if (m_locked)
        {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    MutexT& m_mutex;

    // NOTE: we do not need to make this member atomic since this class is meant
    // to always be used from the same thread, in the same scope.
    bool m_locked;
#ifdef CIX_DEBUG
    std::thread::id m_tid;
#endif
};


}  // namespace cix
