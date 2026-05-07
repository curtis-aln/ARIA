
#include "simulation/simulation.h"
#include "Utils/UI/CrashLogger.h"
#include "simulation/settings/settings.h"

extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main()
{
    Random::set_seed(0);
    // Globally available settings loaded from toml file
	load_settings(ARIA_SETTINGS_PATH);

    // Custom Debugger
    CrashLogger::set_exception_translator(); 

    try
    {
        Simulation().run_simulation();
    }
    catch (const std::exception& e) { CrashLogger::handle(e); }
    catch (...) { CrashLogger::handle(); }
}