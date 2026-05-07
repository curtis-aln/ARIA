#include "settings.h"
#include <toml++/toml.hpp>

#include "world/world_settings.h"
#include "managers/cell_manager/cell_manager_settings.h"
#include "managers/food_manager/food_manager_settings.h"
#include "entities/spring/spring_settings.h"
#include "entities/cell/cell_settings.h"

template<typename T>
static T require_value(const toml::node_view<toml::node>& node, std::string_view path)
{
    std::optional<T> val = node.value<T>();
    if (!val.has_value())
        throw std::runtime_error("Missing required config key: " + std::string(path));
    return *val;
}

#define REQUIRE(member, node) member = require_value<decltype(member)>(node, #node)

static void load_simulation_settings(toml::table& tbl)
{
    REQUIRE(SimulationSettings::full_screen, tbl["simulation"]["full_screen"]);
    REQUIRE(SimulationSettings::vsync, tbl["simulation"]["vsync"]);
    REQUIRE(SimulationSettings::max_fps, tbl["simulation"]["max_fps"]);
    REQUIRE(SimulationSettings::ui_scale_percent, tbl["simulation"]["ui_scale_percent"]);
}

static void load_graphical_settings(toml::table& tbl)
{
    REQUIRE(GraphicalSettings::spring_outline_thickness, tbl["graphical"]["spring_outline_thickness"]);
    REQUIRE(GraphicalSettings::spring_thickness, tbl["graphical"]["spring_thickness"]);
    REQUIRE(GraphicalSettings::cell_outline_thickness, tbl["graphical"]["cell_outline_thickness"]);
    REQUIRE(GraphicalSettings::food_transparency, tbl["graphical"]["food_transparency"]);
}

static void load_world_settings(toml::table& tbl)
{
    REQUIRE(WorldSettings::bounds_radius, tbl["world"]["bounds_radius"]);
    REQUIRE(WorldSettings::border_repulsion_magnitude, tbl["world"]["border_repulsion_magnitude"]);
    REQUIRE(WorldSettings::cells_x, tbl["world"]["cells_x"]);
    REQUIRE(WorldSettings::cells_y, tbl["world"]["cells_y"]);
    REQUIRE(WorldSettings::cell_max_capacity, tbl["world"]["cell_max_capacity"]);
}

static void load_cell_manager_settings(toml::table& tbl)
{
	REQUIRE(CellManagerSettings::max_protozoa, tbl["cell_manager"]["max_protozoa"]);
	REQUIRE(CellManagerSettings::initial_protozoa, tbl["cell_manager"]["initial_protozoa"]);
}

static void load_food_manager_settings(toml::table& tbl)
{
	REQUIRE(FoodManagerSettings::max_food, tbl["food_manager"]["max_food"]);
	REQUIRE(FoodManagerSettings::initial_food, tbl["food_manager"]["initial_food"]);
}

static void load_cell_settings(toml::table& tbl)
{
    REQUIRE(CellSettings::max_cells, tbl["protozoa"]["max_cells"]);
    REQUIRE(CellSettings::spawn_radius, tbl["protozoa"]["spawn_radius"]);
    REQUIRE(CellSettings::energy_decay_rate, tbl["protozoa"]["energy_decay_rate"]);
    REQUIRE(CellSettings::reproductive_cooldown, tbl["protozoa"]["reproductive_cooldown"]);
    REQUIRE(CellSettings::initial_energy, tbl["protozoa"]["initial_energy"]);
    REQUIRE(CellSettings::wander_threshold, tbl["protozoa"]["wander_threshold"]);
    REQUIRE(CellSettings::digestive_time, tbl["protozoa"]["digestive_time"]);
}

static void load_spring_settings(toml::table& tbl)
{
    REQUIRE(SpringSettings::spring_work_const, tbl["protozoa"]["spring_work_const"]);
    REQUIRE(SpringSettings::breaking_length, tbl["protozoa"]["breaking_length"]);
    REQUIRE(SpringSettings::maximum_extension, tbl["protozoa"]["maximum_extension"]);
}


static void load_food_settings(toml::table& tbl)
{
    REQUIRE(FoodManagerSettings::food_radius, tbl["food"]["food_radius"]);
    REQUIRE(FoodManagerSettings::friction, tbl["food"]["friction"]);
    REQUIRE(FoodManagerSettings::death_age, tbl["food"]["death_age"]);
    REQUIRE(FoodManagerSettings::reproductive_cooldown, tbl["food"]["reproductive_cooldown"]);
    REQUIRE(FoodManagerSettings::reproductive_threshold, tbl["food"]["reproductive_threshold"]);
    REQUIRE(FoodManagerSettings::initial_nutrients, tbl["food"]["initial_nutrients"]);
    REQUIRE(FoodManagerSettings::final_nutrients, tbl["food"]["final_nutrients"]);
    REQUIRE(FoodManagerSettings::nutrient_development_time, tbl["food"]["nutrient_development_time"]);
    REQUIRE(FoodManagerSettings::food_spawn_distance, tbl["food"]["food_spawn_distance"]);
    REQUIRE(FoodManagerSettings::spawn_proportionality_constant, tbl["food"]["spawn_proportionality_constant"]);
    REQUIRE(FoodManagerSettings::death_age_chance, tbl["food"]["death_age_chance"]);
    REQUIRE(FoodManagerSettings::kFoodVisibilityRampFrames, tbl["food"]["food_visibility_ramp_frames"]);
    REQUIRE(FoodManagerSettings::kFoodMaxAlpha, tbl["food"]["food_max_alpha"]);
    REQUIRE(FoodManagerSettings::vibration_strength, tbl["food"]["vibration_strength"]);
    REQUIRE(FoodManagerSettings::update_freq, tbl["food"]["update_freq"]);
    REQUIRE(FoodManagerSettings::initial_food, tbl["food"]["initial_food"]);
    REQUIRE(FoodManagerSettings::max_food, tbl["food"]["max_food"]);
    REQUIRE(FoodManagerSettings::cell_max_capacity, tbl["food"]["cell_max_capacity"]);
    REQUIRE(FoodManagerSettings::cells_x, tbl["food"]["cells_x"]);
    REQUIRE(FoodManagerSettings::cells_y, tbl["food"]["cells_y"]);
}


void load_settings(const std::string& path)
{
    std::string full_path = std::string(PROJECT_ROOT) + "/" + path;

    toml::table tbl;

    try
    {
        tbl = toml::parse_file(path);
    }
    catch (const toml::parse_error& err)
    {
        std::cerr
            << "Error parsing file '" << err.source().path
            << "':\n" << err.description()
            << "\n (" << err.source().begin << ")\n";
        return;
    }

    std::cout << "Loading settings from: "
        << std::filesystem::absolute(path) << '\n';


    load_simulation_settings(tbl);
    load_graphical_settings(tbl);
    load_world_settings(tbl);
    load_cell_settings(tbl);
	load_spring_settings(tbl);
    load_food_settings(tbl);
    load_cell_manager_settings(tbl);
    load_food_manager_settings(tbl);
}