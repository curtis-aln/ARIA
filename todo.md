----------------------------------------------------------------------
##### ImGUI TODO
- add infant mortality stat (chance of dying before reaching frame 50)
- Clone: duplicate an organism exactly, spawn the copy nearby
- add the ability to pinch, pin, and throw organisms around 
- add a "Navigate to most sucessfull organism" button which locks on to the organism which has reporoduced the most

- Track collision resolutions per frame

-----------------------------------------------------------------------
#### Saving And Loading TODO
- Worlds should be able to be saved and loaded
- add the ability to save organisms to file
- add the ability to spawn a saved organism
- add the ability to fill the world with a saved organism (with mutations optional) or the protozoa, food line graph

-----------------------------------------------------------------------
#### Food To Do
- Ability to change the food reproductive settings in IMGUI
- Ability to change the food update settings in IMGUI
- A food tab in IMGUI which shows all the food settings and statistics

-----------------------------------------------------------------------
#### Simulation AutoReset
- if off, create a popup that says "All organisms have died, reset simulation?" with yes and no options, if yes is selected reset the simulation, 
  if no is selected pause the simulation and display a message that says "Simulation paused, press play to continue"
- ability to change autoreset on extinction
- add all settings to a json file and have the program read from it on starrtup, then add a "save settings" button to write the current settings back to the file. 
- Reset Simulation Button with controls for world size, initial protozoa count, food spawn rate, and mutation rate/range


-----------------------------------------------------------------------
##### world TODO
- add the ability to change the worldsize in real time, regardless of if the spatial hash grid can change with it yet, as well as an auto world size expansion
- Add radiation zones which mutates an organism based on their proximity to the center of the zone, the closer they are the higher the mutation rate and range, but also the higher the energy cost for existing in that zone
- Add black holes which can pull in protozoa and food, but not kill them, just make them orbit until they get pulled out by another protozoa or food item
  can be modified by the user. if the gravitational pull is negative it becomes a white hole which pushes things away instead of pulling them in
- Add wind Currents Using a grid of vectors which can be queried by the cells and food.
- Add Obsticals which are a collection of circles and a grid to allow cells and food to query it.
+-------- Create a forcefield around the world border which pushes protozoa and food back into the world instead of just clamping them

-----------------------------------------------------------------------

##### Simulation TODO
- if zoomed too far out you cant select on protozoa
- Add ambient music with mild bubble sound effects
- std::cout debug prints in production code - Add a constexpr bool DEBUG_LOGGING = false flag in settings and gate all std::cout behind it, or use a proper logger.
- orginize settings files

-----------------------------------------------------------------------

##### Rendering TODO
- Simulation should start more zoomed out
- Add a focus blur effect using shaders so cells on the edges of the screen are blurred, and cells in the center are clear
- Add a subtle bloom effect to the simulation, so that the cells and food glow slightly
- Let cells add their nutrients to a floor grid but the cells shouldnt be able to use it yet, solely for visual effect
- background should be a blurred gradient rather than a solid color
- There should be a second SFML grid which is dimmer and moves a lot slower than the main grid, to give the illusion of depth
- Add a layered background like you saw in that video

-----------------------------------------------------------------------

#### Multithreading todo

- BenchMark the performance with my old laptop
Multithreadding GUI:
- Tickbox to activate the debugger 
- Slider to adjust the number of worker threads on Updating
- 
- Histogram or bar graph showing the load and memory usage of each thread


#### optimization
all_protozoa_ - has a constant size, so when there is 100 protozoa it can hold the information for 400'000, have dynamic resizing
std::vector<int> reproduce_indexes{};
		reproduce_indexes.reserve(max_protozoa);
is called every frame
pass SpringResult by reference


 - make a food tab to change all of the food settings
- Add better rendering for the world border


- Massive Lag spikes occour in the simulation
- less than 25% of the CPU is being used
- Massive Lines are drawn accross the simulation
- A massive amount of memory is used at the start of the simulation
- o_vector needs an imgui Visuliser attached to it
- Birth and Death Requests need an imgui visuliser attached to it


#### Today
[done] Food Count shows as zero 
[done] Hide panels button doesnt work
- Force reproduce and Force Die do not work, and you cant choose between single cell and whole organism
- Add cell, add spring, remove cell, remove spring do not work
- add the ability to pinch, pin, and throw organisms around 
- Feed energy / feed nutrients doesnt work
- No way to select between feed to one cell or whole organism
- When you select a Cell it should change your tab to The Organism Tracker
- When you deselect or the cell dies it should change your view back to the Simulation Tab
- render food grid doesnt work


- There are no evolutionary parameters for Reproduction
- There is no way for 2 celld organisms to add a cell or connection
- No way to see how many cells or food are in the circle
- Background Grid doesnt Fade out
- Peak Ever is ambiguous
- Average lifetime isnt calculated
- Longest Lifetime isnt calculated
- births per 100 frames isnt calculated
- deaths per 100 frames isnt calculated
- Infant mortality isnt calculated
- Avg cells and Avg springs isnt calculated
- Energy ratio is not calculated
- Highlighted cells and Highlighted food isnt live
- World Resize doesnt work
- The whole Tagged system doesnt work
- The Tools Tab needs to be side by side
- In display, enable rendering needs to go in the sim tab
- Show cell grid and food grid needs to go in the grid tabs, and cell grid needs to be renamed to collision grid
- Debug mode toggle needs to go in organism
- Simple mode needs to be removed we now have Level-Of-Detail Rendering
- Track statistics needs to go to the sim tab
- All the debug overlays need to go to the organism tracker tab
- Move UI controls to the simulation Tab
- Its hard to click on a cell, there should be an invisible click hitbox to make it easier
- Remove the redundant energy bar in the organism debugger
- give the energy subtab better names
- make SetMouseMode an Enum
- you cant change the spring constant or damping of springs which are selected
- you cant change any of the spring values when selected
- Cells can gain more than their max energy, nutrients, integrity
- Add cell, add spring, remove cell, remove spring do not work
- Apply mutation does not work and you cannot choose between a cell and a whole organism
- Clone Nearby doesnt work
- Save to file doesnt work
- you should be able to deselect the mouse right click effects, and when you select it should show the radius by defualt


#### Issues with the current evolution system
- Springs dont work off energy, they should take a cut of the energy transfer process for themselves to maintain their integrity
- Springs should have a decaying state too

808 iterations
1025 iterations: removing body pointer in spring
1056 iterations: adding references 
1045 iterations: variation
1070 iterations: took Body pointer out of cell
1082 iterations: variation
1142 iterations: managing body pointer in food update

Turning off collisions because Fuck collisions
- Recalculate food resolve thread jobs 16%
- CellManager update 25%
- Fill Snapshot 12%

1649


- Do we need to check to eat food every frame? what about every 30 frames, relative to the cells clock
- We dont need to have o_vector debug information running every single frame, or even when its not on screen
- What if each particle kept track of its 8 nearby entities for N frames, resolved collisions with them, then updated after the Nth frame


Simulation has discovered it is more evolutionarialy efficient to stay still and let food run into them
[Done] Food should slowly move.
- Food should be repelled from Cells

Cells Can blow up to rediculous populations:
- increase the max particles per grid cell count
- cells can take integrity damage from collisions
- No nutrient transfer should be allowed to take place between connector cells