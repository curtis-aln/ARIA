----------------------------------------------------------------------
##### ImGUI TODO
- add infant mortality stat
- Clone: duplicate an organism exactly, spawn the copy nearby
- Worlds should be able to be saved and loaded
- add the ability to change the worldsize in real time, regardless of if the spatial hash grid can change with it yet
- add the ability to pinch, pin, and throw organisms around 
- add a "Navigate to most sucessfull organism" button which locks on to the organism which has reporoduced the most
- add the ability to save organisms to file
- add the ability to spawn a saved organism
- add the ability to fill the world with a saved organism (with mutations optional)
or the protozoa, food line graph
- Reset Simulation Button with controls for world size, initial protozoa count, food spawn rate, and mutation rate/range
- add all settings to a json file and have the program read from it on starrtup, then add a "save settings" button to write the current settings back to the file. 
- Track collision resolutions per frame
- ability to change autoreset on extinction
- if off, create a popup that says "All organisms have died, reset simulation?" with yes and no options, if yes is selected reset the simulation, if no is selected pause the simulation and display a message that says "Simulation paused, press play to continue"
- Ability to change the food reproductive settings
- Ability to change the food update settings

-----------------------------------------------------------------------

##### world TODO
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
- a foods saturation or brightness should be preportional to how close it is to being able to reproduce
- When designing the cells, The outer and inner layers should be roughly the same color, the inside should be very transparent and the outside a lot less
- Optimize rendering thread to 144fps rather than 30fps
- Add Glow effect Using my RTX 5060
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

### Subtodo
[Done] remove the add energy from the Tuning and controls tab
[Done] Remove the save to file / load from file
[Done] move force reproduce and force die to the man organism tab to the left
[Done] make the "make immortal" button to the same place
[Done] Move the clone button to the same place
- Move the mutation and structure to the left of the graph
- remove the tuning and controls panel



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

After i have all these bug fixes, we can work on
The Pheromones Update




---------------------------------------------------------
New evolutionary Parameters
- Disconnect chance:
	- A value between 0 and 1 which is the chance a temparary spring disconnects every frame
- with a new cell, there is an add_cell_chance chance that it creates a cell connected to it.
  This new connection has little to no friction, and the spring influence is very small. This is to reduce the chance of the addition ruining the organisms function
- with a new cell (age < 30), there is a chance that it will create a new spring connection to a nearby cell under the condition that both cells dont already have a connection,
  and they are both freshly born


###### Stomach System
A neural Network that takes in inputs
Nutrient count (out of max)
Energy Levels (out of max)
Integriry Levels (out of max)
and outputs 
- a (0, 1) ratio of where the energy should go
- a (0, 1) ratio of how much energy should be given based on its maximum

The faster you transfer nutrients, the more energy it costs.
- This is negatively preportional to the size of the cell


###### Movement System
Cells Control their friction Through the following single layer NN
Inputs:
- Energy
- Current Friction
- Speed Sq

Output(s):
- delta friction


Springs Take in
- Cell A friction
- Cell B friction
- Current Length

And Churn out:
- delta length

i can begin working on the movement system when i solve 15 bugs
XXOOOOOOOOOOOOO