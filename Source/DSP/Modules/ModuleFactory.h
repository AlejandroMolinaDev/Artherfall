#pragma once

#include <memory>
#include "DSP/Chain/IModule.h"

namespace aetherfall
{

/** Creates the DSP instance of a chain module. Modules not implemented yet
    return a pass-through stub (the chain still works and the project always
    compiles while phases F3/F4 progress). */
std::unique_ptr<IModule> createModule (ModuleType type);

/** True once the real DSP of a module type exists (used by tests and later by
    the GUI to flag unfinished modules during development). */
bool isModuleImplemented (ModuleType type) noexcept;

} // namespace aetherfall
