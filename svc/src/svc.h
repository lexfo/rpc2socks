// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#pragma once


class svc : public std::enable_shared_from_this<svc>
{
private:
    enum state_t : DWORD
    {
        state_stopped = SERVICE_STOPPED,
        state_start_pending = SERVICE_START_PENDING,
        state_running = SERVICE_RUNNING,
        state_pause_pending = SERVICE_PAUSE_PENDING,
        state_paused = SERVICE_PAUSED,
        state_continue_pending = SERVICE_CONTINUE_PENDING,
        state_stop_pending = SERVICE_STOP_PENDING,
    };

public:
    svc();
    ~svc();

    exit_t init();
    exit_t run();
    void uninit();

#ifdef APP_ENABLE_SERVICE
    bool running_as_service() const;
#endif

    static std::shared_ptr<svc> instance();

    // static utils
    static bool validate_name(const std::wstring& name);
    static bool auto_name(std::wstring& out_path, std::wstring& out_name);

    // static utils - service
#ifdef APP_ENABLE_SERVICE
    static exit_t install(bool start);
    static exit_t uninstall(std::wstring name, bool stop_first);
    // static exit_t start();
#endif

private:
    // worker thread
    exit_t launch_worker_thread();
    static unsigned __stdcall worker_entry_point(void* context);

    // service manager interface
#ifdef APP_ENABLE_SERVICE
    bool commit_status(
        state_t new_state, DWORD exit_code=0, DWORD wait_hint=3000);
    static void WINAPI service_main(DWORD argc, LPWSTR* argv);
    static DWORD WINAPI service_control(
        DWORD control, DWORD event_type, LPVOID event_data, LPVOID context);
#endif

private:
    static std::weak_ptr<svc> ms_instance;

private:
    // service properties
    std::wstring m_name;

    // worker thread
    HANDLE m_thread;
    HANDLE m_stop_event;

    // service state
#ifdef APP_ENABLE_SERVICE
    SERVICE_STATUS_HANDLE m_status_handle;
#endif
};
