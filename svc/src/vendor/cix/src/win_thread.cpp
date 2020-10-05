// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS

namespace cix {

namespace detail
{
#if CIX_THREAD_HAS_NAME
#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD dwType;     // Must be 0x1000
        LPCSTR szName;    // Pointer to name (in user addr space)
        DWORD dwThreadID; // Thread ID (-1 = caller thread)
        DWORD dwFlags;    // Reserved for future use, must be zero
    } THREADNAME_INFO;
#pragma pack(pop)
#endif
}  // namespace detail


win_thread::win_thread()
    : m_handle{nullptr}
    , m_id{0}
    , m_term_event{nullptr}
{
}


win_thread::~win_thread()
{
    assert(!this->alive());  // request_termination() and join() not called
    this->terminate();
}


bool win_thread::set_priority(priority_t prio) const
{
    assert(this->alive());
    return FALSE != SetThreadPriority(m_handle, prio);
}


bool win_thread::request_termination() const
{
    if (m_term_event)
        return FALSE != SetEvent(m_term_event);

    SetLastError(0);
    return false;
}


bool win_thread::termination_requested() const
{
    return
        m_term_event &&
        WAIT_OBJECT_0 == WaitForSingleObject(m_term_event, 0);
}


bool win_thread::join(std::uint32_t wait_ms, bool force_terminate)
{
    // deadlock detection
    if (m_id && m_id == GetCurrentThreadId())
    {
        assert(0);
        return false;
    }

    // wait for the thread to finish
    if (m_handle)
    {
        // when a thread terminates, the thread object attains a signaled state
        const DWORD res = WaitForSingleObject(m_handle, wait_ms);
        if (res == WAIT_TIMEOUT)
        {
            if (!force_terminate)
                return false;

            // the thread is still running, terminate it forcefully
            TerminateThread(m_handle, exit_code_t::force_terminated);
            while (WaitForSingleObject(m_handle, 10) == WAIT_TIMEOUT) { ; }
        }

        CloseHandle(m_handle);
        m_handle = nullptr;
    }

    // here, either m_handle is null or thread terminated

    m_id = 0;

    if (m_term_event)
    {
        CloseHandle(m_term_event);
        m_term_event = nullptr;
    }

    return true;
}


void win_thread::terminate()
{
    this->request_termination();
    this->join(100, true);
}


#if CIX_THREAD_HAS_NAME
void win_thread::set_name(const char* name) const
{
    if (m_id)
        win_thread::set_name(m_id, name);
}
#endif


#if CIX_THREAD_HAS_NAME
void win_thread::set_name(std::uint32_t id, const char* name)
{
    // How to: Set a Thread Name in Native Code
    // http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx

    // MSDN quote: Note that the thread name is copied to the thread so that the
    // memory for the threadName parameter can be released.

    //if (!IsDebuggerPresent())
    //    return;

    detail::THREADNAME_INFO info;

    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = id;
    info.dwFlags = 0;

    __try
    {
        RaiseException(
            0x406D1388, 0,
            sizeof(info) / sizeof(ULONG_PTR),
            reinterpret_cast<ULONG_PTR*>(&info));
    }
    __except(EXCEPTION_EXECUTE_HANDLER) //EXCEPTION_CONTINUE_EXECUTION
    {
    }
}
#endif


std::size_t win_thread::hardware_concurrency()
{
    DWORD_PTR proc_mask;
    DWORD_PTR sys_mask;
    std::size_t count = 0;

    if (!GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask))
    {
        assert(0);
        return 1;
    }

    for (std::size_t idx = 0; idx < 8 * sizeof(proc_mask); ++idx)
    {
        if (proc_mask & (DWORD_PTR(1) << idx))
            ++count;
    }

    return !count ? 1 : count;
}


//------------------------------------------------------------------------------
win_thread::launch_pad::launch_pad()
    : m_created_event(CreateEvent(nullptr, TRUE, FALSE, nullptr))
    , m_started_event(CreateEvent(nullptr, TRUE, FALSE, nullptr))
{
}


win_thread::launch_pad::~launch_pad()
{
    CloseHandle(m_started_event);
    m_started_event = nullptr;

    CloseHandle(m_created_event);
    m_created_event = nullptr;
}


void win_thread::launch_pad::launch(win_thread* thread)
{
    if (thread->m_handle)
    {
        if (thread->alive())
            CIX_THROW_LOGIC("thread already launched and running");

        CloseHandle(thread->m_handle);
        thread->m_handle = nullptr;
    }

    thread->m_id = 0;

    if (thread->m_term_event)
    {
        CloseHandle(thread->m_term_event);
        thread->m_term_event = nullptr;
    }

    thread->m_term_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!thread->m_term_event)
        CIX_THROW_WINERR("failed to create win_thread's terminate event object");

    thread->m_handle = reinterpret_cast<HANDLE>(_beginthreadex(
        nullptr, 0, win_thread::launch_pad::static_entry_point, this, 0,
        &thread->m_id));
    if (!thread->m_handle)
        CIX_THROW_CRTERR("failed to create thread");

    SetEvent(m_created_event);

    const DWORD wait_res = WaitForSingleObject(m_started_event, INFINITE);
    if (wait_res != WAIT_OBJECT_0)
    {
        const DWORD error = GetLastError();
        CIX_THROW_RUNTIME(
            "failed to wait for thread to start (result {:#08X}; error {})",
            wait_res, error);
    }
}


unsigned __stdcall win_thread::launch_pad::static_entry_point(void* param)
{
    try
    {
        auto pad = reinterpret_cast<win_thread::launch_pad*>(param);

        // first ensure the call to _beginthreadex() returned
        const DWORD wait_res = WaitForSingleObject(
            pad->m_created_event, INFINITE);
        if (wait_res != WAIT_OBJECT_0)
        {
            const DWORD error = GetLastError();
            CIX_THROW_RUNTIME(
                "failed to wait for thread to be created "
                "(result {:#08X}; error {})",
                wait_res, error);
        }

        pad->run();

        // graceful termination
        return exit_code_t::ok;
    }
    catch (const std::exception&)
    {
        assert(0);
        // CIX_LOGFATAL(
        //     WSTR("exception caught in thread #{}: {}"),
        //     GetCurrentThreadId(), e.what());
        return exit_code_t::error;
    }
    catch (...)
    {
        assert(0);
        // CIX_LOGFATAL(
        //     WSTR("unknown exception caught in thread {}"),
        //     GetCurrentThreadId());
        return exit_code_t::error;
    }
}


}  // namespace cix

#endif  // #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
