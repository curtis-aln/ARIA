cmake_minimum_required(VERSION 3.21)
project(ProjectARIA LANGUAGES CXX)

# ── Language standard ─────────────────────────────────────────────────────────
# Tell CMake to use C++23. MSVC will receive /std:c++latest automatically.
# We set EXTENSIONS OFF to avoid compiler-specific GNU extensions.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generates compile_commands.json — useful for clangd / IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies (FetchContent) ───────────────────────────────────────────────
# FetchContent downloads and builds dependencies at configure time.
# Order matters: ImGui must be available before ImGui-SFML and ImPlot,
# because both need to find imgui headers.

include(FetchContent)

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.91.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(ImGui)
FetchContent_GetProperties(ImGui SOURCE_DIR IMGUI_DIR)
set(IMGUI_SFML_FIND_SFML OFF)

FetchContent_Declare(ImGui-SFML
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml
    GIT_TAG v3.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(ImGui-SFML)


FetchContent_Declare(ImPlot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.17
)
FetchContent_MakeAvailable(ImPlot)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)


# Disable SFML fetching its own system deps (freetype etc.) — we want static
set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)

# Suppress a CMake policy warning from older dependency CMakeLists files
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# ImGui-SFML needs to know where imgui source lives before it configures
set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)




# ── Executable sources ────────────────────────────────────────────────────────
# Listed explicitly (no glob) so CMake always detects added/removed files.
add_executable(ProjectARIA
    # ── Entry point
    src/main.cpp
    src/pch.cpp

    # ── Food system
    src/Food/FoodManager.cpp
    src/Food/FoodManager_spawn.cpp
    src/Food/FoodManager_update.cpp

    # ── Protozoa behaviours (split across extension files)
    
    src/Protozoa/extension/mutations.cpp
    src/Protozoa/extension/organic.cpp
    src/Protozoa/extension/physics.cpp

    # ── Simulation core
    src/simulation/events.cpp
    src/simulation/simulation.cpp

    # ── ImGui control panel + tab implementations
    src/simulation/imgui/control_panel.cpp
    src/simulation/imgui/imgui_handling.cpp
    src/simulation/imgui/helpers/toast_system.cpp
    src/simulation/imgui/tabs/display_tab.cpp
    src/simulation/imgui/tabs/graphs_tab.cpp
    src/simulation/imgui/tabs/grid_tab.cpp
    src/simulation/imgui/tabs/organism_tab.cpp
    src/simulation/imgui/tabs/simulation_tab.cpp
    src/simulation/imgui/tabs/statistics_tab.cpp
    src/simulation/imgui/tabs/tagged_tab.cpp
    src/simulation/imgui/tabs/tools_tab.cpp

    # ── Utilities
    src/Utils/Graphics/spatial_grid/spatial_grid_renderer.cpp
    src/settings.cpp

    # ── World / simulation update loop
    src/world/collision_resolution.cpp
    src/world/food_resolution.cpp
    src/world/world.cpp
    src/world/world_updating.cpp

    # ── ImPlot (header-only library — sources must be compiled as part of our target)
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
 "src/settings.cpp" "src/world/world_state.h" "src/simulation/sim_snapshot.h" "src/simulation/triple_buffer.h" "src/simulation/sim_command.h" "src/world/zones/zone_base.h" "src/world/zones/radiation_zone.h" "src/world/zones/gravity_zone.h" "src/world/food_claim_buffer.h" "src/world/draw_cell_debug.cpp")

 target_compile_definitions(ProjectARIA PRIVATE
    ARIA_SETTINGS_PATH="${CMAKE_SOURCE_DIR}/media/aria_settings.toml"
)


# ── Include directories ───────────────────────────────────────────────────────
# PRIVATE means these paths are only used when compiling ProjectARIA itself,
# not exposed to anything that links against it.
target_include_directories(ProjectARIA PRIVATE
    src                          # allows #include "settings.h" etc. from anywhere
    ${implot_SOURCE_DIR}         # allows #include <implot.h>
)

# ── Linked libraries ──────────────────────────────────────────────────────────
target_link_libraries(ProjectARIA PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    ImGui-SFML::ImGui-SFML
    $<$<CXX_COMPILER_ID:MSVC>:dbghelp>   # MSVC only — stack trace support
)

target_link_libraries(ProjectARIA PRIVATE tomlplusplus::tomlplusplus)


# ── Preprocessor definitions ──────────────────────────────────────────────────
target_compile_definitions(ProjectARIA PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"   # lets code reference assets by absolute path
    IMGUI_DEFINE_MATH_OPERATORS          # required by ImPlot: enables ImVec2 +/-/* operators
    $<$<BOOL:${SFML_STATIC_LIBRARIES}>:SFML_STATIC>
)

# ── Precompiled header ────────────────────────────────────────────────────────
# Speeds up compilation by pre-parsing heavy headers (imgui, STL) once.
# We skip ImPlot's own .cpp files because they have their own include order
# and don't expect a forced external PCH injected before their own includes.
target_precompile_headers(ProjectARIA PRIVATE src/pch.h)

set_source_files_properties(
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# ── MSVC-specific compiler options ───────────────────────────────────────────
if(MSVC)
    # Remove the /EHsc that CMake injects by default before we add our own,
    # otherwise both /EHsc and /EHa end up on the command line causing D9025.
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    target_compile_options(ProjectARIA PRIVATE
        /Zc:__cplusplus      # makes MSVC report the real __cplusplus value, not 199711L
        /EHa                 # enable SEH exceptions (needed for some SFML/Windows interop)
        /FS                  # force synchronous PDB writes (required for parallel builds)
        /W3                  # warning level 3 (reasonable default)
        $<$<CONFIG:Debug>:/Zi>           # full debug info in Debug builds
        $<$<CONFIG:RelWithDebInfo>:/Zi>  # also in RelWithDebInfo
        $<$<CONFIG:Release>:/Z7>         # embed debug info in .obj for Release (no .pdb)
    )
endif()

# ── Post-build: copy media assets next to the executable ─────────────────────
add_custom_command(TARGET ProjectARIA POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/media"
        "$<TARGET_FILE_DIR:ProjectARIA>/media"
    COMMENT "Copying media assets..."
)