// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

namespace cix {

template<class Function, class... Args>
inline win_thread::win_thread(Function&& f, Args&&... args)
    : win_thread()
{
    auto callee = std::bind(
        cix::decay_copy(std::forward<Function>(f)),
        cix::decay_copy(std::forward<Args>(args))...);
    launch_pad_tmpl<decltype(callee)> pad(std::move(callee));
    pad.launch(this);
}


template<class Function, class... Args>
inline void win_thread::launch(Function&& f, Args&&... args)
{
    assert(!this->alive());
    auto callee = std::bind(
        cix::decay_copy(std::forward<Function>(f)),
        cix::decay_copy(std::forward<Args>(args))...);
    launch_pad_tmpl<decltype(callee)> pad(std::move(callee));
    pad.launch(this);
}


inline bool win_thread::alive() const
{
    // MSDN: When a thread terminates, the thread object attains a signaled
    // state, satisfying any threads that were waiting on the object
    return m_handle && WaitForSingleObject(m_handle, 0) == WAIT_TIMEOUT;
}


inline HANDLE win_thread::handle() const
{
    return m_handle;
}


inline std::uint32_t win_thread::id() const
{
    return this->alive() ? m_id : 0;
}


inline HANDLE win_thread::term_event() const
{
    return m_term_event;
}


inline std::uint32_t win_thread::current_id()
{
    return GetCurrentThreadId();
}


//------------------------------------------------------------------------------
template <class Function>
inline win_thread::launch_pad_tmpl<Function>::launch_pad_tmpl(Function&& rhs)
    : m_function(std::forward<Function>(rhs))
{
}


template <class Function>
inline void win_thread::launch_pad_tmpl<Function>::run()
{
    win_thread::launch_pad_tmpl<Function>::run_static(this);
}


template <class Function>
inline void win_thread::launch_pad_tmpl<Function>::run_static(
    launch_pad_tmpl* self)
{
    Function func(std::forward<Function>(self->m_function));
    SetEvent(self->m_started_event);
    func();
}

}  // namespace cix
