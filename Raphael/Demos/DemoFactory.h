#pragma once
#include <memory>
#include "IDemo.h"

namespace raphael
{
    // Creates a demo instance for the requested type (DemoType lives in IDemo.h).
    // To add a demo: add a value to DemoType and a matching case in CreateDemo().
    std::unique_ptr<IDemo> CreateDemo(DemoType type);
}
