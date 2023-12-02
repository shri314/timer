/**
 * ScopedExit
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#pragma once

#include "ScopedAction.hpp"

#include <utility>

template<class ExitFuncT>
struct ScopedExit : ScopedAction<ExitFuncT>
{
    ScopedExit(ExitFuncT exit_func)
        : ScopedAction<ExitFuncT>{
                []() { },
                std::move(exit_func)
            }
    {
    }
};
