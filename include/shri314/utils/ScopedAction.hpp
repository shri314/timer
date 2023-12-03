/**
 * shri314::utils::ScopedAction
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#pragma once

#include <exception>
#include <utility>

namespace shri314::utils
{

template<class ExitFuncT>
struct ScopedAction
{
    template<class InitFuncT>
    ScopedAction(InitFuncT&& init_func, ExitFuncT exit_func)
        : m_exit_func(std::move(exit_func))
    {
        init_func();
    }

    ~ScopedAction() noexcept(noexcept(std::declval<ExitFuncT&>()))
    {
        if constexpr (!noexcept(std::declval<ExitFuncT&>()))
        {
            try
            {
                m_exit_func();
            }
            catch(...)
            {
                if( std::uncaught_exceptions() > 0 )
                {
                    // mute exception thrown by m_func, as
                    // there is already one in progress.
                    return;
                }

                throw;
            }
        }
        else
        {
            m_exit_func();
        }
    }

private:
    ExitFuncT m_exit_func;
};

}
