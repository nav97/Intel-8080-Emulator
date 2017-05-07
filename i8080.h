#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
/*opcode flags
z   - zero
	  set to 1 when result equals 0

s   - sign
	  set to 1 when bit 7 (MSB) is set

p   - parity
	  set whenever answer has even parity

cy  - carry
	  set to 1 when instruction resulted in carry out

ac  - auxiliary carry
	  indicates when a carry has been generated out of the 
	  least significant four bits of the accumulator register 
	  following an instruction. It is primarily used in decimal (BCD) 
	  arithmetic instructions, it is adding 6 to adjust BCD arithmetic.
*/
typedef struct ConditionCodes
{
    uint8_t z : 1;
    uint8_t s : 1;
    uint8_t p : 1;
    uint8_t cy : 1;
    uint8_t ac : 1;
    uint8_t pad : 3;
} ConditionCodes;

/*Registers
a   - Accumulator
      the primary 8-bit accumulator

bc  - Register Pair
     
de  - Register Pair

hl  - Register Pair
      register pair that holds an indirect address 
      used any time data is read or written

sp  - Stack Pointer
      register that stores the last program request in a stack

pc  - Program Counter
      register that holds the address of the next instruction 


memory - Pointer
         pointer to address in memory of the Intel 8080. 
*/
typedef struct State8080
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;
    uint8_t *memory;
    struct ConditionCodes cc;
    uint8_t int_enable;
} State8080;

//Emulate Intel 8080 CPU
//Returns number of cycles for each executed instruction
int Emulate(State8080 *state, bool printTrace);

//Prints out current instruction being pointed to by program counter (PC)
int Disassemble8080(unsigned char *codebuffer, int pc);

//Facilitates Interrupts
void GenInterrupt(State8080 *state, int interrupt_num);

//Read desired ROM file into Memory at specified offset
void ReadFileIntoMemory(State8080 *state, char *filename, uint32_t offset);

//Allocate CPU struct
//Returns pointer to allocated Intel 8080 CPU object
State8080 *Init8080(void);