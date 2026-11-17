##### ImGUI TODO
- add a "Navigate to most sucessfull organism" button which locks on to the organism which has reporoduced the most
- Track collision resolutions per frame

#### Saving And Loading TODO
- Worlds should be able to be saved and loaded
- add the ability to save organisms to file
- add the ability to spawn a saved organism
- add the ability to fill the world with a saved organism (with mutations optional) or the protozoa, food line graph


#### Simulation AutoReset
- if off, create a popup that says "All organisms have died, reset simulation?" with yes and no options, if yes is selected reset the simulation, 
  if no is selected pause the simulation and display a message that says "Simulation paused, press play to continue"
- ability to change autoreset on extinction
- add all settings to a json file and have the program read from it on starrtup, then add a "save settings" button to write the current settings back to the file. 

##### world TODO
- Add radiation zones which mutates an organism based on their proximity to the center of the zone, the closer they are the higher the mutation rate and range, but also the higher the energy cost for existing in that zone
- Add black holes which can pull in protozoa and food, but not kill them, just make them orbit until they get pulled out by another protozoa or food item
  can be modified by the user. if the gravitational pull is negative it becomes a white hole which pushes things away instead of pulling them in
- Add wind Currents Using a grid of vectors which can be queried by the cells and food.
- Add Obsticals which are a collection of circles and a grid to allow cells and food to query it.
+-------- Create a forcefield around the world border which pushes protozoa and food back into the world instead of just clamping them

##### Simulation TODO
- Add ambient music with mild bubble sound effects

##### Rendering TODO
- Add a subtle bloom effect to the simulation, so that the cells and food glow slightly
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

- No way to see how many cells or food are in the circle
- Highlighted cells and Highlighted food isnt live
- you cant change the spring constant or damping of springs which are selected
- you cant change any of the spring values when selected
- Apply mutation does not work and you cannot choose between a cell and a whole organism
- you should be able to deselect the mouse right click effects, and when you select it should show the radius by defualt