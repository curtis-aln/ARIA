#include "settings.h"
#include <toml++/toml.hpp>

#include "world/world_settings.h"
#include "managers/cell_manager/cell_manager_settings.h"
#include "managers/food_manager/food_manager_settings.h"
#include "entities/spring/spring_settings.h"
#include "entities/cell/cell_settings.h"
#include "entities/food/food.h"

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

    REQUIRE(SimulationSettings::initial_frame_rate_updating, tbl["simulation"]["max_fps_updating"]);
    REQUIRE(SimulationSettings::initial_frame_rate_rendering, tbl["simulation"]["max_fps_rendering"]);

	REQUIRE(SimulationSettings::max_iterations, tbl["simulation"]["max_iterations"]);
    REQUIRE(SimulationSettings::max_simulation_time, tbl["simulation"]["max_runtime"]);

    REQUIRE(SimulationSettings::ui_scale_percent, tbl["simulation"]["ui_scale_percent"]);
}

static void load_world_settings(toml::table& tbl)
{
    REQUIRE(WorldSettings::bounds_radius, tbl["world"]["bounds_radius"]);
    REQUIRE(WorldSettings::border_repulsion_magnitude, tbl["world"]["border_repulsion_magnitude"]);
    
	REQUIRE(WorldSettings::collision_grid_power, tbl["world"]["collision_grid_power"]);
	REQUIRE(WorldSettings::collision_max_capacity, tbl["world"]["collision_grid_max_capacity"]);

	REQUIRE(FoodManagerSettings::pheromone_grid_power, tbl["world"]["food_pheromone_grid_power"]);
    REQUIRE(FoodManagerSettings::cell_max_capcity, tbl["world"]["food_resolution_grid_max_capacity"]);

	REQUIRE(WorldSettings::birth_cell_power, tbl["world"]["birth_cell_power"]);
	REQUIRE(WorldSettings::birth_max_capacity, tbl["world"]["birth_grid_max_capacity"]);

	REQUIRE(WorldSettings::updating_threads, tbl["world"]["updating_threads"]);
	REQUIRE(WorldSettings::tick_sim_multiplier, tbl["world"]["tick_sim_multiplier"]);
}

static void load_cell_manager_settings(toml::table& tbl)
{
    REQUIRE(CellManagerSettings::max_cells, tbl["cell_manager"]["max_cells"]);
    REQUIRE(CellManagerSettings::initial_protozoa, tbl["cell_manager"]["initial_protozoa"]);
    REQUIRE(CellManagerSettings::auto_reset_on_extinction, tbl["cell_manager"]["auto_reset_on_extinction"]); 
	REQUIRE(CellManagerSettings::speed_energy_tax, tbl["cell_manager"]["speed_energy_tax"]);
}

static void load_cell_settings(toml::table& tbl)
{
    REQUIRE(CellSettings::spawn_radius, tbl["cell"]["spawn_radius"]);   
    REQUIRE(CellSettings::repro_cooldown, tbl["cell"]["reproductive_cooldown"]);
    REQUIRE(CellSettings::initial_energy, tbl["cell"]["initial_energy"]);
    REQUIRE(CellSettings::digestive_time, tbl["cell"]["digestive_time"]);
	REQUIRE(CellSettings::bite_amount, tbl["cell"]["bite_amount"]);

    REQUIRE(CellSettings::infant_time, tbl["cell"]["infant_time"]);

	REQUIRE(CellSettings::integrity_conversion_rate, tbl["cell"]["integrity_conversion_rate"]);
	REQUIRE(CellSettings::nutrients_conversion_rate, tbl["cell"]["nutrients_conversion_rate"]);

	REQUIRE(CellSettings::friction_energy_loss_const, tbl["cell"]["friction_energy_loss_const"]);
}

static void load_spring_settings(toml::table& tbl)
{
    REQUIRE(SpringSettings::spring_work_const, tbl["spring"]["spring_work_const"]);  
    REQUIRE(SpringSettings::maximum_extension, tbl["spring"]["maximum_extension"]); 

	REQUIRE(SpringSettings::spring_break_force, tbl["spring"]["spring_break_force"]);
	REQUIRE(SpringSettings::spring_break_length_factor, tbl["spring"]["spring_break_length_factor"]);
	REQUIRE(SpringSettings::spring_damage_threshold, tbl["spring"]["spring_damage_threshold"]);
	REQUIRE(SpringSettings::fully_developed_age, tbl["spring"]["fully_developed_age"]);
	REQUIRE(SpringSettings::nutrients_transfer_loss, tbl["spring"]["nutrients_transfer_loss"]);
	REQUIRE(SpringSettings::stress_damage_const, tbl["spring"]["stress_damage_const"]);
}
static void load_food_manager_settings(toml::table& tbl)
{
	REQUIRE(FoodManagerSettings::update_freq, tbl["food_manager"]["update_freq"]);
	REQUIRE(FoodManagerSettings::max_food, tbl["food_manager"]["max_food"]);
	REQUIRE(FoodManagerSettings::initial_food, tbl["food_manager"]["initial_food"]);
	REQUIRE(FoodManagerSettings::friction, tbl["food_manager"]["friction"]);
	REQUIRE(FoodManagerSettings::kFoodVisibilityRampFrames, tbl["food_manager"]["food_visibility_ramp_frames"]);
	REQUIRE(FoodManagerSettings::kFoodMaxAlpha, tbl["food_manager"]["food_max_alpha"]);
	REQUIRE(FoodManagerSettings::food_random_spawn_per_frame, tbl["food_manager"]["food_random_spawn_per_frame"]);
	REQUIRE(FoodManagerSettings::food_random_spawn_chance, tbl["food_manager"]["food_random_spawn_chance"]);
	REQUIRE(FoodManagerSettings::spawn_proportionality_constant, tbl["food_manager"]["spawn_proportionality_constant"]);
	REQUIRE(FoodManagerSettings::food_spawn_distance, tbl["food_manager"]["food_spawn_distance"]);
	REQUIRE(FoodManagerSettings::death_age_chance, tbl["food_manager"]["death_age_chance"]);
	REQUIRE(FoodManagerSettings::fade_start_nutrients, tbl["food_manager"]["fade_start_nutrients"]);
	REQUIRE(FoodManagerSettings::food_launch_strength, tbl["food_manager"]["food_launch_strength"]);
	REQUIRE(FoodManagerSettings::food_launch_chance, tbl["food_manager"]["food_launch_chance"]);
}

static void load_food_settings(toml::table& tbl)
{
	REQUIRE(FoodSettings::repro_cooldown, tbl["food"]["reproductive_cooldown"]);
	REQUIRE(FoodSettings::nutrient_reproductive_threshold, tbl["food"]["nutrient_reproductive_threshold"]);
	REQUIRE(FoodSettings::initial_nutrients, tbl["food"]["initial_nutrients"]);
	REQUIRE(FoodSettings::final_nutrients, tbl["food"]["final_nutrients"]);
	REQUIRE(FoodSettings::nutrient_development_time, tbl["food"]["nutrient_development_time"]);
	REQUIRE(FoodSettings::spawn_immunity, tbl["food"]["spawn_immunity"]);
	REQUIRE(FoodSettings::vibrate_freq, tbl["food"]["vibrate_freq"]);
	REQUIRE(FoodSettings::death_age, tbl["food"]["death_age"]);
	REQUIRE(FoodSettings::nutrients_to_radius_scale, tbl["food"]["nutrients_to_radius_scale"]);
	REQUIRE(FoodSettings::vibration_strength, tbl["food"]["vibration_strength"]);
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
    load_world_settings(tbl);
    load_cell_settings(tbl);
	load_spring_settings(tbl);
    load_food_settings(tbl);
    load_cell_manager_settings(tbl);
    load_food_manager_settings(tbl);
}