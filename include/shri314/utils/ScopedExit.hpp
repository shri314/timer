/**
 * shri314::utils::ScopedExit
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#pragma once

#include "shri314/utils/ScopedAction.hpp"

#include <utility>

namespace shri314::utils
{

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

}
