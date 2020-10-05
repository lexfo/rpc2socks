// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

#include "main.h"


static exit_t main_stub_default()
{
    exit_t exit_code;
    auto service = std::make_shared<svc>();

    exit_code = service->init();
    if (exit_code != APP_EXITCODE_OK)
        return exit_code;

    exit_code = service->run();

    service->uninit();

    service.reset();
    assert(!svc::instance());

    return exit_code;
}


#ifdef APP_ENABLE_SERVICE
static exit_t main_stub_service(std::vector<std::wstring_view>& args)
{
    enum action_t { action_default, action_install, action_uninstall };

    action_t action = action_default;

    for (std::size_t idx = 1; idx < args.size(); ++idx)
    {
        if (args[idx] == L"--install")
        {
            if (action != action_default)
            {
                LOGERROR("one action allowed per call");
                return APP_EXITCODE_ARG;
            }

            action = action_install;
        }
        else if (args[idx] == L"--uninstall")
        {
            if (action != action_default)
            {
                LOGERROR("one action allowed per call");
                return APP_EXITCODE_ARG;
            }

            action = action_uninstall;
        }
        else
        {
            LOGERROR(L"unknown arg: {}", args[idx]);
            return APP_EXITCODE_ARG;
        }
    }

    if (action == action_install)
        return svc::install(true);
    else if (action == action_uninstall)
        return svc::uninstall(std::wstring(), true);
    else if (action == action_default)
        return main_stub_default();

    assert(0);
    return APP_EXITCODE_ERROR;
}
#endif  // #ifdef APP_ENABLE_SERVICE


int wmain(int argc, wchar_t* argv[])
{
    // heap setup
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    // memory leaks monitoring
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); //| _CRTDBG_CHECK_ALWAYS_DF);
#endif

    // windows debug output enabled by default in debug mode
#if defined(_DEBUG) && defined(APP_LOGGING_ENABLED)
    logging::enable_dbgout(true);
#endif

    int exit_code = APP_EXITCODE_OK;

    try
    {
        cix::wincon::init(cix::wincon::non_intrusive);
        cix::wincon::set_title(
            xstr::to_string(xpath::title(utils::module_path(nullptr))));

#ifdef APP_ENABLE_SERVICE
        std::vector<std::wstring_view> args;

        if (argc > 0)
        {
            args.resize(static_cast<std::size_t>(argc));
            for (std::size_t idx = 0; idx < static_cast<std::size_t>(argc); ++idx)
                args[idx] = std::wstring_view(argv[idx]);
        }

        exit_code = main_stub_service(args);
#else
        CIX_UNVAR(argc);
        CIX_UNVAR(argv);
        exit_code = main_stub_default();
#endif
    }
    catch (const std::exception& exc)
    {
        assert(0);
        CIX_UNVAR(exc);
        LOGCRITICAL("unhandled exception occurred! {}", exc.what());
        exit_code = APP_EXITCODE_ERROR;
    }
    catch (...)
    {
        assert(0);
        LOGCRITICAL("UNKNOWN exception occurred!");
        exit_code = APP_EXITCODE_ERROR;
    }

    cix::wincon::release();
    // logging::enable_sysevent(false);

    return exit_code;
}
