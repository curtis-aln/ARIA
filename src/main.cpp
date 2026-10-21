
#include "simulation/settings/settings.h"
#include "simulation/simulation.h"
#include "Utils/UI/CrashLogger.h"


extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

int main()
{

#ifdef _WIN32
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);

	std::cout << "ARIA - Artificial Realistic Intelligent Agents\n";

	Random::set_seed(0);

	// Globally available settings loaded from toml file
	load_settings(ARIA_SETTINGS_PATH);
	std::cout << WorldSettings::updating_threads << " threads will be used for updating the simulation.\n";

	// Custom Debugger
	//CrashLogger::set_exception_translator(); todo

	//try
	//{
	Simulation().run_simulation();
	//}
	//catch (const std::exception& e) { CrashLogger::handle(e); }
	//catch (...) { CrashLogger::handle(); }
}