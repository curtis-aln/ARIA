----------------------------------------------------------------------
##### ImGUI TODO
[Done] time elapsed in hours minutes seconds
- add infant mortality stat
- Death notifications: optional toast notification when a tagged organism dies, with cause of death (starvation, age, etc.
- Clone: duplicate an organism exactly, spawn the copy nearby
- Organism tagging, ability to tag an organism and it will be outlined and visible, will show up on the tagged organisms screen
- Time-lapse / fast-forward mode — run simulation at 10x-100x with minimal rendering for long-run experiments
[Done] Ability to make organisms immortal
[Done] Force feed: manually inject energy into a selected organism, useful for keeping a favourite alive during experiments
- Worlds should be able to be saved and loaded
- add the ability to change the worldsize in real time, regardless of if the spatial hash grid can change with it yet
- add the ability to pinch, pin, and throw organisms around 
- add a zoom slider
- add a "Navigate to most sucessfull organism" button which locks on to the organism which has reporoduced the most
- add the ability to save organisms to file
- add the ability to spawn a saved organism
- add the ability to fill the world with a saved organism (with mutations optional)
[Done] Add a "total" f
or the protozoa, food line graph
- Reset Simulation Button with controls for world size, initial protozoa count, food spawn rate, and mutation rate/range
- add all settings to a json file and have the program read from it on starrtup, then add a "save settings" button to write the current settings back to the file. 
- line graph for average lifetime
- line graph for mutation rate and range
- line graph for average offspring
- Track collision resolutions per frame
[Done] Get remove, add spring, and remove spring working
- get stomach bar working correctly
- get time since last reproduced working better, replace with reproduce cooldown
[Done] when cells are immortal their energy shouldnt drop below zero
[Done] sometimes the sin wave graph goers by too quick, slider to change the amount of cycles displayed
- ability to modify max protozoa and max cells
- ability to toggle vsync
- ability to set the speed of the updating simulation
- ability to change autoreset on extinction
- if off, create a popup that says "All organisms have died, reset simulation?" with yes and no options, if yes is selected reset the simulation, if no is selected pause the simulation and display a message that says "Simulation paused, press play to continue"
- Ability to change energy parameters of the protozoas
- Ability to change the food reproductive settings
- Ability to change the food update settings
- Ability to save these settings to the toml file

-----------------------------------------------------------------------

##### world TODO
- spatial hash grid should dynamically change density as the population changes, and should be able to change size too
- Add radiation zones which mutates an organism based on their proximity to the center of the zone, the closer they are the higher the mutation rate and range, but also the higher the energy cost for existing in that zone
- Add black holes which can pull in protozoa and food, but not kill them, just make them orbit until they get pulled out by another protozoa or food item
  can be modified by the user. if the gravitational pull is negative it becomes a white hole which pushes things away instead of pulling them in
- Add wind Currents Using a grid of vectors which can be queried by the cells and food.
- Add Obsticals which are a collection of circles and a grid to allow cells and food to query it.
- Track velocity, and previous position outside of the protozoa class
- Create a forcefield around the world border which pushes protozoa and food back into the world instead of just clamping them

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
[Done] Get the seperate thread architecture fully working
[Done] Check with previous version to see if there are any performance degradations
[Done] Get the Rendering and Updating Working on Seperate Threads
[Done] Separate the update thread into Several other worker threads
- BenchMark the performance with my old laptop
Multithreadding GUI:
- Tickbox to activate the debugger 
- Slider to adjust the number of worker threads on Updating
- Histogram or bar graph showing the load and memory usage of each thread

-----------------------------------------------------------------------

##### Protozoa TODO
[Done] when a new cell is created it should have very low friction and generally not affect the organism too much, test this
- create a cell body class
- each protozoa stores cell_positions_nearby and food positions for debugging, just have it once in the protozoa manager
- when springs are created through create cell or create spring, give them random properties

-----------------------------------------------------------------------

##### Known Bugs
- Protozoa end up developing humungous hitboxes and seem to be moving on their own
- you cant toggle the screen upper border so its impossible to move
- Food are dying suspiciously quickly
- if the cell grid and the food grid are different sizes then the cells index the wrong location in the food grid and they cant resolve
- food grid tracking in imgui has bugs
- you cant deselect a protozoa once you select it
- protozoa springs can get too powerful - kill them if under enough tension
- Protozoa's will spawn with springs which can cause them to die, change the bounding

- Friction in the panel should range from its min to max
- Nutrients doesnt work
- Reproduction cooldown doesnt work
- when a cell dies it doesnt deselect
- when paused a lot of the protozoa selection doesnt work
- pausing desyncs the slection
- period wrong 
- There is no limit to how much food they can eat, add a stomach capacity and digestion time

The way protozoa are created at the start of the sim needs to use different code to the mutation one thats where the bug is coming from

-----------------------------------------------------------------------
##### Debugging Todo
- When selecting a protozoa it should track all the cells nearby
- When selecting a protozoa it should track all the food nearby

-----------------------------------------------------------------------
##### Cells Todo
- Add the ability for cells to determine 
	- when they reproduce
	- when they die artificially
	- How fast they convert Nutrients to energy
	- How they converve energy
	- How much energy they give to their offspring


#### optimization
all_protozoa_ - has a constant size, so when there is 100 protozoa it can hold the information for 400'000, have dynamic resizing
std::vector<int> reproduce_indexes{};
		reproduce_indexes.reserve(max_protozoa);
is called every frame
pass SpringResult by reference


#### Next todo
# - make a food tab to change all of the food settings
# - make the protozoa container dynamic


- add spring rendering for all protozoa
- create a move force for the mouse to move the protozoa

- organise the imgui tab files a bit better, brak up the organism tab into its sub-tabs
- Add better rendering for the world border



Order of operations for cell updating

Velocity changes:
- Collision resolution
- Spring forces
- Friction

Position changes:
- Clamping into world


# Note for next visit
- get reproduction working better
check if there is a way you can remove birth request and connection requests, because it would make the process so much simpler.
We could perhaps pack more information into a birth or connection request to help with the process


[DONE] Get Simulation To start of 30fps
[DONE] Ability to Select Cells and see information about them
[DONE] camera should track the selected protozoa
[DONE] create a world where there are a bunch of two cells connected by a spring
[DONE] todo make the springs visible
[DONE] Let the Organism Tracker Find the spring and other cells and display them in the organism tracker
[DONE] Get a range of 2-5 Cells with 2-N springs
[DONE] Ability to select a cell and see all of their cells debug information
[DONE] Ability to deselect cells
[DONE] ability to drag individual cells around with the mouse
[DONE] springs should break when they are under too much tension
[DONE] Ability for Protozoa to eat food and gain energy
[DONE] Add all the missing debug information for a protozoa
- Ability to spawn Food particles at the mouse position
- Ability to spawn Cell particles at the mouse position
- Ability for Protozoa to transfer energy to all of their cells

# Future To-dos
- some cells dont show up for some reason
- Cells need the ability to die
- implement new spatial hash grid
- implement new collision resolution
- Get Simulation Running with 400,000 entitities before continuing with the rest of the features'


[Done] cell energy can drop below zero
[Done] you cant view cells when they are dead
- Death is not handled correctly
- Massive Lag spikes occour in the simulation
- less than 25% of the CPU is being used
- Massive Lines are drawn accross the simulation
- There is no hard limit to how many cells there can be
- A massive amount of memory is used at the start of the simulation
- o_vector needs an imgui Visuliser attached to it
- Birth and Death Requests need an imgui visuliser attached to it



Next To work on:
- Bugs in the code
- Getting all the statistics that were omitted working again, ALL of them
- Natural Selection Parameters to infuence evolution



Today todo
[Done] Use the SimSnapshot to write RenderData, then remove the copies from the fillsnapshot (1)
[Done] Make sure we are writing to the simsnapshot Before reproducing or killing protozoa (2)
- Locate and Fix the cell -> Body missmatch, look at cases where a body exists but protozoa doesnt or vise versa (3)
- Make the Spring connections simple when zoomed out far enough (4)
- Draw the cell as a single circle averaged of its two colors when zoomed out far enough (5)
- Stop updating the cell's inner body by velocity when zoomed out far enough (6)
- Spend Some Time Optimizing the simulation to run at 60fps with 100,000 cells and 300,000 food (7)
- restore resolve_modifications function (8)

Speedrun time (again), start time: 6:35pm
(1): 19:00
(2): 19:00