// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"


namespace detail
{
    // per microsoft rule, the name and display name of a service must not be
    // longer than 256 characters
    static constexpr std::size_t svc_name_maxlen = 256;

    static constexpr DWORD svc_type = SERVICE_WIN32_OWN_PROCESS;
}


std::weak_ptr<svc> svc::ms_instance;


svc::svc()
    : m_thread{nullptr}
    , m_stop_event{nullptr}
#ifdef APP_ENABLE_SERVICE
    , m_status_handle{nullptr}
#endif
{
    assert(ms_instance.expired());
}


svc::~svc()
{
    this->uninit();
    ms_instance.reset();
}


exit_t svc::init()
{
    std::wstring module_path;

    // shared_from_this() cannot be called from the constructor so we call it
    // from here, as soon as possible in the execution flow.
    assert(ms_instance.expired());
    ms_instance = this->shared_from_this();

    if (!svc::auto_name(module_path, m_name))
        return APP_EXITCODE_API;

    // CAUTION: must be manual reset!
    m_stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!m_stop_event)
        return APP_EXITCODE_API;

    return APP_EXITCODE_OK;
}


exit_t svc::run()
{
#ifdef APP_ENABLE_SERVICE
#ifdef _DEBUG
    // in debug mode and if current instance has been invoked by a debugger,
    // then run as a regular application instead of running as a service
    if (!IsDebuggerPresent())
#endif
    {
        const SERVICE_TABLE_ENTRYW ste[] = {
            { const_cast<wchar_t*>(m_name.c_str()), svc::service_main },
            { nullptr, nullptr },
        };

        // main loop
        // this call blocks until service has terminated
        if (!StartServiceCtrlDispatcher(ste))
        {
            LOGERROR(
                "StartServiceCtrlDispatcher failed (error {})",
                GetLastError());
            return APP_EXITCODE_API;
        }

        return APP_EXITCODE_OK;
    }
#ifdef _DEBUG
    else
#endif
#endif
#if !defined(APP_ENABLE_SERVICE) || defined(_DEBUG)
    {
        const auto exit_code = this->launch_worker_thread();
        if (exit_code != APP_EXITCODE_OK)
            return exit_code;

        // wait for worker thread to terminate
        WaitForSingleObject(m_thread, INFINITE);

        // get thread exit code
        DWORD thread_exit_code = 0;
        if (m_thread)
        {
            if (!GetExitCodeThread(m_thread, &thread_exit_code))
                thread_exit_code = 0;

            CloseHandle(m_thread);
            m_thread = nullptr;
        }

        return static_cast<exit_t>(thread_exit_code);
    }
#endif
}


void svc::uninit()
{
    if (m_stop_event)
    {
        CloseHandle(m_stop_event);
        m_stop_event = nullptr;
    }
}


#ifdef APP_ENABLE_SERVICE
bool svc::running_as_service() const
{
    return !!m_status_handle;
}
#endif


std::shared_ptr<svc> svc::instance()
{
    return ms_instance.lock();
}


#ifdef APP_ENABLE_SERVICE
bool svc::commit_status(state_t new_state, DWORD exit_code, DWORD wait_hint)
{
    static DWORD check_point = 0;

    if (!m_status_handle)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }

    SERVICE_STATUS status{0};

    status.dwServiceType = detail::svc_type;
    status.dwCurrentState = new_state;
    status.dwWin32ExitCode = exit_code;
    status.dwServiceSpecificExitCode = 0;
    status.dwWaitHint = wait_hint;

    if (new_state == SERVICE_START_PENDING)
    {
        status.dwControlsAccepted = 0;
    }
    else
    {
        status.dwControlsAccepted =
            SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;
    }

    if (new_state != state_start_pending &&
        new_state != state_pause_pending &&
        new_state != state_continue_pending &&
        new_state != state_stop_pending)
    {
        // check_point = 0;
        status.dwCheckPoint = 0;
    }
    else
    {
        status.dwCheckPoint = ++check_point;
    }

    if (!SetServiceStatus(m_status_handle, &status))
    {
        LOGERROR("SetServiceStatus failed (error {})", GetLastError());
        return false;
    }

    return true;
}
#endif  // #ifdef APP_ENABLE_SERVICE


exit_t svc::launch_worker_thread()
{
    HANDLE start_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!start_event)
    {
        LOGERROR("CreateEvent failed (code {})", GetLastError());
        assert(0);
        return APP_EXITCODE_API;
    }

    // ensure stop flag is not raised before we start
    ResetEvent(m_stop_event);

    // create and launch thread
    m_thread = reinterpret_cast<HANDLE>(_beginthreadex(
        NULL, 0, svc::worker_entry_point, start_event, 0, nullptr));
    if (!m_thread)
    {
        LOGERROR("_beginthreadex failed (code {})", errno);
        assert(0);
        CloseHandle(start_event);
        return APP_EXITCODE_API;
    }

    // wait for its bootstrap code to complete
    const auto wait_res = WaitForSingleObject(start_event, 3000);
    if (wait_res != WAIT_OBJECT_0)
    {
        LOGERROR(
            "failed to start worker thread (result {}; code {})",
            wait_res, GetLastError());
        assert(0);
        CloseHandle(start_event);
        return APP_EXITCODE_API;
    }

    // wait for some extra time to let the worker thread warming up
    Sleep(150);

    CloseHandle(start_event);

    return APP_EXITCODE_OK;
}


unsigned __stdcall svc::worker_entry_point(void* context)
{
    CIX_THREAD_SET_NAME_STATIC(GetCurrentThreadId(), "svc");

    auto self = svc::instance();  // CAUTION: this means worker owns self too!
    if (!self)
        return static_cast<unsigned>(APP_EXITCODE_ERROR);

    auto worker = std::make_shared<svc_worker>();

    // notify we are up
    // handle will be closed by service_main()
    {
        auto start_event = reinterpret_cast<HANDLE>(context);
        SetEvent(start_event);
    }

    auto exit_code = worker->init(self->m_stop_event, self->m_name);

    if (exit_code == APP_EXITCODE_OK)
        exit_code = worker->main_loop();

    // explicit release so that correct order is honored
    worker.reset();
    self.reset();

    return exit_code;
}


#ifdef APP_ENABLE_SERVICE
void WINAPI svc::service_main(DWORD, LPWSTR*)
{
    auto self = svc::instance();
    if (!self)
    {
        assert(0);
        return;
    }

    assert(!self->m_thread);

    self->m_status_handle = RegisterServiceCtrlHandlerExW(
        self->m_name.c_str(), svc::service_control, nullptr);
    if (!self->m_status_handle)
    {
        LOGERROR("RegisterServiceCtrlHandler failed (code {})", GetLastError());
        return;
    }

    if (!self->commit_status(state_start_pending))
    {
        assert(0);
        return;
    }

    // launch worker thread and wait for it to start
    const auto exit_code = self->launch_worker_thread();
    if (exit_code != APP_EXITCODE_OK)
    {
        self->commit_status(state_stopped, static_cast<DWORD>(exit_code));
        return;
    }

    if (!self->commit_status(state_running))
    {
        assert(0);
        self->commit_status(
            state_stopped, static_cast<DWORD>(APP_EXITCODE_API));
        return;
    }

    // wait for worker thread to terminate
    WaitForSingleObject(self->m_thread, INFINITE);

    // get thread exit code
    DWORD thread_exit_code = 0;
    if (self->m_thread)
    {
        if (!GetExitCodeThread(self->m_thread, &thread_exit_code))
            thread_exit_code = 0;

        CloseHandle(self->m_thread);
        self->m_thread = nullptr;
    }

    self->commit_status(state_stopped, thread_exit_code);
}
#endif  // #ifdef APP_ENABLE_SERVICE


#ifdef APP_ENABLE_SERVICE
DWORD WINAPI svc::service_control(DWORD control, DWORD, LPVOID, LPVOID)
{
    switch (control)
    {
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
        {
            auto instance = svc::instance();
            if (instance && instance->m_stop_event && instance->m_thread)
            {
                instance->commit_status(state_stop_pending);
                SetEvent(instance->m_stop_event);
            }
            return NO_ERROR;
        }

        // case SERVICE_CONTROL_CONTINUE:
        // case SERVICE_CONTROL_PAUSE:
        // case SERVICE_CONTROL_NETBINDADD:
        // case SERVICE_CONTROL_NETBINDDISABLE:
        // case SERVICE_CONTROL_NETBINDENABLE:
        // case SERVICE_CONTROL_NETBINDREMOVE:
        // case SERVICE_CONTROL_PARAMCHANGE:
        // case SERVICE_CONTROL_PRESHUTDOWN:
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}
#endif  // #ifdef APP_ENABLE_SERVICE


bool svc::validate_name(const std::wstring& name)
{
    if (name.empty())
        return false;

    if (name.size() > detail::svc_name_maxlen)
        return false;

    if (name.find_first_of(L"/\\") != name.npos)
        return false;

    return true;
}


bool svc::auto_name(std::wstring& out_path, std::wstring& out_name)
{
    out_path = utils::module_path(nullptr);
    if (out_path.empty())
    {
        LOGERROR("GetModuleFileName failed (error {})", GetLastError());
        out_name.clear();
        return false;
    }

    out_name = xstr::to_string(xpath::title(out_path));

    if (out_name.size() > detail::svc_name_maxlen)
        out_name.erase(detail::svc_name_maxlen);

    if (out_name.empty())
    {
        LOGERROR(L"empty service name; path was: {}", out_path);
        return false;
    }

    return true;
}


#ifdef APP_ENABLE_SERVICE
exit_t svc::install(bool start)
{
    cix::unique_sc_handle mgr_handle;
    cix::unique_sc_handle svc_handle;
    std::wstring svc_path;
    std::wstring svc_name;

    if (!svc::auto_name(svc_path, svc_name))
        return APP_EXITCODE_API;

    // TODO / FIXME:
    // * should the service be registered with a specific account?
    //   (CreateService)

    mgr_handle.reset(
        OpenSCManagerW(
            nullptr, nullptr,
            GENERIC_WRITE | (start ? SERVICE_START : 0)));
    if (!mgr_handle)
    {
        LOGERROR("OpenSCManager failed (error {})", GetLastError());
        return APP_EXITCODE_API;
    }

    svc_handle.reset(CreateServiceW(
        mgr_handle.get(), svc_name.c_str(), svc_name.c_str(),
        GENERIC_EXECUTE, detail::svc_type, SERVICE_AUTO_START,
        SERVICE_ERROR_IGNORE, svc_path.c_str(), nullptr, nullptr, nullptr,
        nullptr, nullptr));
    if (!svc_handle)
    {
        const auto error = GetLastError();
        if (error != ERROR_SERVICE_EXISTS)
        {
            LOGERROR("CreateService failed (error {})", error);
            return APP_EXITCODE_API;
        }
    }

    if (start && !StartServiceW(svc_handle.get(), 0, nullptr))
    {
        LOGERROR("StartService failed (error {})", GetLastError());
        return APP_EXITCODE_API;
    }

    // explicit release so order is correct
    svc_handle.reset();
    mgr_handle.reset();

    return APP_EXITCODE_OK;
}
#endif  // #ifdef APP_ENABLE_SERVICE


#ifdef APP_ENABLE_SERVICE
exit_t svc::uninstall(std::wstring svc_name, bool stop_first)
{
    cix::unique_sc_handle mgr_handle;
    cix::unique_sc_handle svc_handle;
    SERVICE_STATUS status{};

    if (svc_name.empty())
    {
        std::wstring svc_path;
        svc::auto_name(svc_path, svc_name);
    }
    else if (!svc::validate_name(svc_name))
    {
        return APP_EXITCODE_ARG;
    }

    mgr_handle.reset(OpenSCManagerW(nullptr, nullptr, GENERIC_WRITE));
    if (!mgr_handle)
    {
        LOGERROR("OpenSCManager failed (error {})", GetLastError());
        return APP_EXITCODE_API;
    }

    svc_handle.reset(OpenServiceW(mgr_handle.get(), svc_name.c_str(), DELETE));
    if (!svc_handle)
    {
        const auto error = GetLastError();
        if (error == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            svc_handle.reset();
            mgr_handle.reset();
            return APP_EXITCODE_OK;
        }
        LOGERROR("OpenService failed (error {})", error);
        return APP_EXITCODE_API;
    }

    if (stop_first)
    {
        const auto end_of_wait = cix::ticks_now() + 5000;

        for (;;)
        {
            status = {};

            const auto success = ControlService(
                svc_handle.get(), SERVICE_CONTROL_STOP, &status);
            const auto error = GetLastError();

            if (!success && (
                error == ERROR_SERVICE_NOT_ACTIVE ||
                error == ERROR_SHUTDOWN_IN_PROGRESS))
            {
                break;
            }
            else if (
                (success || error == ERROR_SERVICE_CANNOT_ACCEPT_CTRL) && (
                status.dwCurrentState == SERVICE_CONTINUE_PENDING ||
                status.dwCurrentState == SERVICE_PAUSE_PENDING ||
                status.dwCurrentState == SERVICE_START_PENDING ||
                status.dwCurrentState == SERVICE_STOPPED))
            {
                if (status.dwCurrentState == SERVICE_STOPPED)
                    break;

                if (cix::ticks_now() > end_of_wait)
                    break;

                Sleep(250);
                continue;
            }
            else
            {
                LOGERROR("ControlService failed (error {})", error);
                break;  // return APP_EXITCODE_API;
            }
        }
    }

    if (!DeleteService(svc_handle.get()))
    {
        const auto error = GetLastError();
        if (error != ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            LOGERROR("DeleteService failed (error {})", error);
            return APP_EXITCODE_API;
        }
    }

    // explicit release so order is correct
    svc_handle.reset();
    mgr_handle.reset();

    return APP_EXITCODE_OK;
}
#endif  // #ifdef APP_ENABLE_SERVICE


#if 0
#ifdef APP_ENABLE_SERVICE
exit_t svc::start()
{
    cix::unique_sc_handle mgr_handle;
    cix::unique_sc_handle svc_handle;
    std::wstring svc_path;
    std::wstring svc_name;

    if (!svc::auto_name(svc_path, svc_name))
        return APP_EXITCODE_API;

    mgr_handle.reset(
        OpenSCManagerW(nullptr, nullptr, SERVICE_START));
    if (!mgr_handle)
    {
        LOGERROR("OpenSCManager failed (error {})", GetLastError());
        return APP_EXITCODE_API;
    }

    svc_handle.reset(OpenServiceW(
        mgr_handle.get(), svc_name.c_str(), SERVICE_START));
    if (!svc_handle)
    {
        LOGERROR("OpenService failed (error {})", GetLastError());
        return APP_EXITCODE_API;
    }

    if (!StartServiceW(svc_handle.get(), 0, nullptr))
    {
        LOGERROR("StartService failed (error {})", GetLastError());
        return APP_EXITCODE_API;
    }

    // explicit release so order is correct
    svc_handle.reset();
    mgr_handle.reset();

    return APP_EXITCODE_OK;
}
#endif  // #ifdef APP_ENABLE_SERVICE
#endif
