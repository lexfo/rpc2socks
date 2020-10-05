// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

#if CIX_PLATFORM == CIX_PLATFORM_WINDOWS

namespace cix {

// threads can be named when using MSVC's debugger
#if defined(_DEBUG) && defined(_MSC_VER)
    #define CIX_THREAD_HAS_NAME  1
    #define CIX_THREAD_SET_NAME(obj_ref, name)    (obj_ref).set_name(name)
    #define CIX_THREAD_SET_NAME_STATIC(id, name)  ::cix::win_thread::set_name(id, name)
#else
    #define CIX_THREAD_HAS_NAME  0
    #define CIX_THREAD_SET_NAME(obj_ref, name)    ((void)0)
    #define CIX_THREAD_SET_NAME_STATIC(id, name)  ((void)0)
#endif


class win_thread
{
public:
    enum priority_t : int
    {
        prio_low = THREAD_PRIORITY_LOWEST,
        prio_normal = THREAD_PRIORITY_NORMAL,
        prio_high = THREAD_PRIORITY_HIGHEST,
    };

    enum exit_code_t : unsigned
    {
        ok = 0,  // graceful termination
        error = 1,  // generic error code
        force_terminated = 2,  // terminated by join() or terminate()
        reserved_3 = 3,
        reserved_4 = 4,
        reserved_5 = 5,
        reserved_6 = 6,
        reserved_7 = 7,
        reserved_8 = 8,
        reserved_9 = 9,
        reserved_10 = 10,
        user_base = 11,  // client code may use the range [user_base, ...]
    };

public:
    win_thread();
    template<class Function, class... Args>
    explicit win_thread(Function&& f, Args&&... args);
    ~win_thread();

    template<class Function, class... Args>
    void launch(Function&& f, Args&&... args);

    // non-copyable
    win_thread(const win_thread&) = delete;
    win_thread& operator=(const win_thread&) = delete;
    win_thread& operator=(win_thread&&) = delete;

    // observers
    bool alive() const;
    HANDLE handle() const;
    std::uint32_t id() const;
    HANDLE term_event() const;

    // operations
    bool set_priority(priority_t prio) const;
    bool request_termination() const;
    bool termination_requested() const;
    bool join(std::uint32_t wait_ms=INFINITE, bool force_terminate=false);
    void terminate();  // same as join(0, true)

    // name (debug only; msvc feature)
#if CIX_THREAD_HAS_NAME
    void set_name(const char* name) const;
    static void set_name(std::uint32_t id, const char* name);
#endif

    // static utils
    static std::uint32_t current_id();
    static std::size_t hardware_concurrency();

private:
    HANDLE m_handle;
    std::uint32_t m_id;
    HANDLE m_term_event;


private:
    // implementation detail

    class launch_pad
    {
    public:
        launch_pad();
        ~launch_pad();

        void launch(win_thread* thread);
        virtual void run() = 0;

    private:
        static unsigned __stdcall static_entry_point(void* param);

    protected:
        friend class win_thread;
        HANDLE m_created_event;
        HANDLE m_started_event;
    };

    template <class Function>
    class launch_pad_tmpl : public launch_pad
    {
    public:
        launch_pad_tmpl(Function&& func);
        void run();

    private:
        static void run_static(launch_pad_tmpl* self);

    private:
        Function m_function;
    };
};

}  // namespace cix

#include "win_thread.inl.h"

#endif  // #if CIX_PLATFORM == CIX_PLATFORM_WINDOWS
