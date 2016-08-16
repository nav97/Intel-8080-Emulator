# Overview
This is an Intel 8080 CPU emulator created using C with a general use machine template created using C++.
The MachineTemplate is currently setup to read Space Invaders roms into memory and print out the CPU's execution trace to the terminal.
It can be compiled with `gcc -o a MachineTemplate.cpp i8080.c` and then executed with `./a`.

# Rom Files
All rom files that will be used should be placed in the "rom" directory. 
To ensure that the files are loaded correctly into the machine change the name of the file and the desired memory offset in the constructor of the MachineTemplate. 
The rom directory in this repository is empty so you will have to obtain your own roms and place them in the rom folder.
