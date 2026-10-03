##### ImGUI TODO
- add a "Navigate to most sucessfull organism" button which locks on to the organism which has reporoduced the most
- Track collision resolutions per frame

#### Saving And Loading TODO
- Worlds should be able to be saved and loaded
- add the ability to save organisms to file
- add the ability to spawn a saved organism
- add the ability to fill the world with a saved organism (with mutations optional) or the protozoa, food line graph

#### Food To Do
- Ability to change the food reproductive settings in IMGUI
- Ability to change the food update settings in IMGUI
- A food tab in IMGUI which shows all the food settings and statistics

#### Simulation AutoReset
- if off, create a popup that says "All organisms have died, reset simulation?" with yes and no options, if yes is selected reset the simulation, 
  if no is selected pause the simulation and display a message that says "Simulation paused, press play to continue"
- ability to change autoreset on extinction
- add all settings to a json file and have the program read from it on starrtup, then add a "save settings" button to write the current settings back to the file. 
- Reset Simulation Button with controls for world size, initial protozoa count, food spawn rate, and mutation rate/range

##### world TODO
- add the ability to change the worldsize in real time, regardless of if the spatial hash grid can change with it yet, as well as an auto world size expansion
- Add radiation zones which mutates an organism based on their proximity to the center of the zone, the closer they are the higher the mutation rate and range, but also the higher the energy cost for existing in that zone
- Add black holes which can pull in protozoa and food, but not kill them, just make them orbit until they get pulled out by another protozoa or food item
  can be modified by the user. if the gravitational pull is negative it becomes a white hole which pushes things away instead of pulling them in
- Add wind Currents Using a grid of vectors which can be queried by the cells and food.
- Add Obsticals which are a collection of circles and a grid to allow cells and food to query it.
+-------- Create a forcefield around the world border which pushes protozoa and food back into the world instead of just clamping them

##### Simulation TODO
- Add ambient music with mild bubble sound effects
- std::cout debug prints in production code - Add a constexpr bool DEBUG_LOGGING = false flag in settings and gate all std::cout behind it, or use a proper logger.
- orginize settings files
- Ability to have the simulation window open on the same display that visual studio is on

##### Rendering TODO
- Add a focus blur effect using shaders so cells on the edges of the screen are blurred, and cells in the center are clear
- Add a subtle bloom effect to the simulation, so that the cells and food glow slightly
- Let cells add their nutrients to a floor grid but the cells shouldnt be able to use it yet, solely for visual effect
- background should be a blurred gradient rather than a solid color
- There should be a second SFML grid which is dimmer and moves a lot slower than the main grid, to give the illusion of depth
- Add a layered background like you saw in that video

#### Multithreading todo
- BenchMark the performance with my old laptop
Multithreadding GUI:
- Tickbox to activate the debugger 
- Slider to adjust the number of worker threads on Updating
- Histogram or bar graph showing the load and memory usage of each thread

#### optimization
all_protozoa_ - has a constant size, so when there is 100 protozoa it can hold the information for 400'000, have dynamic resizing
- Birth and Death Requests need an imgui visuliser attached to it

#### Today
- Add cell, add spring, remove cell, remove spring do not work
- add the ability to pinch, pin, and throw organisms around 
- Feed energy / feed nutrients doesnt work
- No way to select between feed to one cell or whole organism

- No way to see how many cells or food are in the circle
- Background Grid doesnt Fade out- Avg cells and Avg springs isnt calculated
- Energy ratio is not calculated
- Highlighted cells and Highlighted food isnt live
- World Resize doesnt work
- The whole Tagged system doesnt work
- you cant change the spring constant or damping of springs which are selected
- you cant change any of the spring values when selected
- Apply mutation does not work and you cannot choose between a cell and a whole organism
- you should be able to deselect the mouse right click effects, and when you select it should show the radius by defualt

- We dont need to have o_vector debug information running every single frame, or even when its not on screen
- Food should be repelled from Cells


------------------------------------------------------------
The next goal i should work on is the "primitive reproduction"
- This is hardcoded reproduction where the child organism will be copied and pasted next to its parent
- The reason for this is that i need to see if the reproduction is or isnt the bottleneck in certain situations.

implementation
when a cell is ready to reproduce, it will trigger a search function called:
bool can_protozoa_reproduce(FixedSpan<cell_id> cell_ids)
- searches through the chain of cell->spring->cell connections to make sure all the cells connected are ready to reproduce
- if the search size excedes a certain limit, it will return false and the organism will not be able to reproduce
- The function writes to a smart array all the ID's of the cells that are ready to reproduce
- The function returns a bool indicating if the organism is ready to reproduce or not

After the search is complete, if the protozoa can reproduce, it will trigger a function called:
void protozoa_reproduce(FixedSpan<cell_id> cell_ids)
- This function will take the array of cell ID's and copy the cells and springs to a new location next to the parent organism
- It will also mutate the new cells and springs based on the mutation settings in the simulation

##### TODO
pre-requisites
- Cells need to have ID's (array) attaching them to the springs they are connected to.
- When a spring is removed, it needs to remove the ID from the cells array it was connected to
- When a cell is removed, it needs to remove the ID from the springs array it was connected to
- When a cell is created, it needs its spring ID's to be zero
- In the Cell ImGui, show the array of spring ID's that are connected to the cell, as well as the size

- Create an ImGui toggle to switch between natual reproduction and primitive reproduction
- Create the can_protozoa_reproduce function
- Create the protozoa_reproduce function