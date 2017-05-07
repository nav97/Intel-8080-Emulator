/*
General Machine Emulation Template

Make use of the Intel 8080 emulator to emulate any specific machine 
that would make use of the Intel 8080 CPU such as the Space Invaders Arcade Machine
This Template is a general start and guiding outline to complete the Space Invaders Machine

-- gcc -o a MachineTemplate.cpp i8080.c
*/

extern "C" {
#include "i8080.h"
}

//REMINDER
//When implementing a specific machine
//execute CPU and platform window (graphics) on separate threads
//appropriately sleep the CPU thread to have it run at 2MHz
class MachineTemplate
{
    //TODO
    //Define machine specific input and output ports

  private:
    //CPU
    State8080 *state;

    //Timer & Interrupt variables
    double now;
    double lastTimer;
    double nextInterrupt;
    double sinceLastCycle;
    int whichInterrupt;

  public:
    //Set to True if you want CPU to output trace of execution
    bool printTrace;
    MachineTemplate(bool printTraceSet);

    void *FrameBuffer();
    void MachineOUT(uint8_t port, uint8_t value);
    uint8_t MachineIN(uint8_t port);
    double timeusec();
    void doCPU();
};

//Constructor
MachineTemplate::MachineTemplate(bool printTraceSet)
{
    state = Init8080();
    ReadFileIntoMemory(state, "./rom/invaders.h", 0);
    ReadFileIntoMemory(state, "./rom/invaders.g", 0x800);
    ReadFileIntoMemory(state, "./rom/invaders.f", 0x1000);
    ReadFileIntoMemory(state, "./rom/invaders.e", 0x1800);
    printTrace = printTraceSet;
}

//TO DO
//Returns the location of the video memory in the machine.
//The platform code needs it for drawing to the window.
//Machine Specific, different ROMS will store video memory in different address locations.
void *MachineTemplate::FrameBuffer()
{
    //Space Invaders stores video memory at 0x2400.
    return ((void *)&state->memory[0x2400]);
}

//TO DO
//Handle Machine Specific Output
void MachineTemplate::MachineOUT(uint8_t port, uint8_t value)
{
}

//TO DO
//Handle Machine Specific Input
uint8_t MachineTemplate::MachineIN(uint8_t port)
{
    return 0;
}

//Returns time in microseconds
double MachineTemplate::timeusec()
{
    //get time
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    //convert from seconds to microseconds
    return ((double)currentTime.tv_sec * 1E6) + ((double)currentTime.tv_usec);
}

//runs CPU at a steady 2MHz
void MachineTemplate::doCPU()
{
    while (1)
    {
        now = timeusec();

        if (lastTimer == 0.0)
        {
            lastTimer = now;
            nextInterrupt = lastTimer + 16000.0;
            whichInterrupt = 1;
        }

        if ((state->int_enable) && (now > nextInterrupt))
        {
            if (whichInterrupt == 1)
            {
                GenInterrupt(state, 1);
                whichInterrupt = 2;
            }
            else
            {
                GenInterrupt(state, 2);
                whichInterrupt = 1;
            }
            nextInterrupt = now + 8000.0;
        }

        //How much time has passed?  How many instructions will it take to keep up with
        // the current time? CPU is 2 MHz so 2M cycles/sec
        double sinceLast = now - lastTimer;
        int cycles_to_catch_up = 2 * sinceLast;
        int cycles = 0;

        while (cycles_to_catch_up > cycles)
        {
            unsigned char *opcode;
            opcode = &state->memory[state->pc];

            //Need to define machine specific functions to handle IN and OUT depending on the use of the CPU
            if (*opcode == 0xdb) //machine specific handling for IN
            {
                uint8_t port = opcode[1];
                state->a = MachineIN(port);
                state->pc += 1;
                cycles += 3;
            }
            else if (*opcode == 0xd3) //machine specific handling for OUT
            {
                uint8_t port = opcode[1];
                uint8_t value = state->a;
                MachineOUT(port, value);
                state->pc += 1;
                cycles += 3;
            }
            else
            {
                cycles += Emulate(state, printTrace);
            }
        }
        lastTimer = now;
    }
}

int main(int argc, char **argv)
{
    //set printTrace on initialization to print trace of CPU operation
    MachineTemplate machine(true);
    machine.doCPU();
    return 0;
}