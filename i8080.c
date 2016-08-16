#include "i8080.h"

//CPU cycles for each command obtained from Intel 8080 Systems User Manual.
unsigned char cycles8080[] = {
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x00..0x0f
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x10..0x1f
	4, 10, 16, 5, 5, 5, 7, 4, 4, 10, 16, 5, 5, 5, 7, 4, 
	4, 10, 13, 5, 10, 10, 10, 4, 4, 10, 13, 5, 5, 5, 7, 4,
	
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, //0x40..0x4f
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 7, 5,
	
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, //0x80..8x4f
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	
	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11, //0xc0..0xcf
	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11, 
	11, 10, 10, 18, 17, 11, 7, 11, 11, 5, 10, 5, 17, 17, 7, 11, 
	11, 10, 10, 4, 17, 11, 7, 11, 11, 5, 10, 4, 17, 17, 7, 11, 
};

int parity(int x, int size)
{
	int parity = 0; 

	int i;
	for (i=0; i<size; i++)
	{
		parity += x & 1;
		x = x >> 1;
	}
	return (parity % 2 == 0);
}

void LogicFlags(State8080* state)
{
	state->cc.cy = state->cc.ac = 0;
	state->cc.z = (state->a == 0);
	state->cc.s = (0x80 == (state->a & 0x80));
	state->cc.p = parity(state->a, 8);
}

void ArithFlags(State8080* state, uint16_t res)
{
	state->cc.z = ((res&0xff) == 0); 
	state->cc.s = (0x80 == (res & 0x80)); //res & 1000 0000
	state->cc.p = parity(res&0xff, 8);
	state->cc.ac = (res > 0x09);  //checks if result is greater than 9
}

//all flags 
void BcdArithFlags(State8080* state, uint16_t res)
{
	state->cc.cy = (res > 0xff); //checks if result is greater than 0xff
	state->cc.z = ((res&0xff) == 0); 
	state->cc.s = (0x80 == (res & 0x80)); //res & 1000 0000
	state->cc.p = parity(res&0xff, 8);
	state->cc.ac = (res > 0x09); 
}

void UnimplementedInstruction(State8080* state)
{
	//pc will have advanced one, so undo that
	printf ("Error: Unimplemented instruction\n");
	state->pc--;
	Disassemble8080(state->memory, state->pc);
	printf("\n");
	exit(1);
}

/*
Opcode Functions
*/
void ADC(State8080* state, uint8_t addend)
{
	uint16_t result = (uint16_t)state->a + (uint16_t)addend + (uint16_t)state->cc.cy;
	BcdArithFlags(state, result); 
	state->a = result & 0xff; 
}

void ADD(State8080* state, uint8_t addend)
{
	uint16_t result = (uint16_t)state->a + (uint16_t)addend;
	BcdArithFlags(state, result); 
	state->a = result & 0xff; 
}

void ANA(State8080* state, uint8_t var)
{
	uint16_t result = (uint16_t)state->a & (uint16_t)var;
	BcdArithFlags(state, result); 
	state->cc.cy = 0;
	state->a = result;
}

void CALL(State8080* state, unsigned char* opcode)
{
	uint16_t ret = state->pc+2;
	state->memory[state->sp-1] = (ret >> 8) & 0xff;
	state->memory[state->sp-2] = (ret & 0xff);
	state->sp = state->sp-2;
	state->pc = (opcode[2] << 8) | opcode[1];
}

//The contents of the accumulator are complemented 
//No flags are affected.
void CMA(State8080* state)
{
	state->a = ~(state->a);
}

//Carry flag complimented 
void CMC(State8080* state)
{
	state->cc.cy = ~state->cc.cy;
}

void CMP(State8080* state, uint8_t var)
{
	uint16_t result = state->a - var;
	state->cc.z = (result == 0);
	state->cc.s = (0x80 == (result & 0x80));
	state->cc.p = parity(result, 8);
	state->cc.cy = (state->a < var);
}

//The eight-bit number in the accumulator is adjusted
//to form two four-bit Binary-Coded-Decimal digits
//all flags are affected 
void DAA (State8080* state)
{
	uint8_t fourLSB = ((state->a << 4) >> 4);
	if ((fourLSB > 0x09) || (state->cc.ac == 1))
	{
		uint16_t resultLSB = fourLSB + 0x06;
		state->a = state->a + 0x06; 
		BcdArithFlags(state, resultLSB);
	}

	uint8_t fourMSB = (state->a >> 4);
	if((fourMSB > 0x09) || (state->cc.cy == 1))
	{
		uint16_t resultMSB = fourMSB + 0x06;
		state->a = state->a + 0x06; 
		BcdArithFlags(state, resultMSB);
	}
}

//takes sum of a register pair and HL then stores result in HL
void DAD(State8080* state, uint32_t regPair)
{
	uint32_t hl = (state->h << 8) | state->l; 
	uint32_t result = hl + regPair;

	//Carry Flag
	state->cc.cy = ((result & 0xffff0000) > 0);

	//Store Answer
	state->h = (result &  0xff00) >> 8;
	state->l = (result & 0xff);
}

//Decrements a specified register by 1
void DCR(State8080* state, uint8_t* reg)
{
	uint16_t result = *reg - 1;

	state->cc.z = (result == 0);
	state->cc.s = (0x80 == (result & 0x80));
	state->cc.p = parity(result, 8);

	*reg = (result & 0xff); 
}

//Decrements a specified register pair by 1
void DCX(uint8_t* msbReg, uint8_t* lsbReg)
{
	(*lsbReg)--;

	if(*lsbReg == 0xff)
	{
		(*msbReg)--;
	}
}

//Increments a specified register by 1
void INR(State8080* state, uint8_t* reg)
{	
	uint16_t result = (uint16_t)*reg + 1;
	*reg = (result & 0x00ff); 

	ArithFlags(state, result); 
}

//Increments a register pair by 1
void INX(uint8_t* msbReg, uint8_t* lsbReg)
{
	(*lsbReg)++; 
	//overflow
	if (*lsbReg == 0)
	{
		(*msbReg)++;
	}
}

void JMP(State8080* state, unsigned char* opcode)
{
	state->pc = (opcode[2] << 8) | (opcode[1]);
}

void LDA(State8080* state, unsigned char* opcode)
{
	uint16_t address = (opcode[2] << 8) | (opcode[1]);
	state->a = state->memory[address];
}

void LDAX(State8080* state, uint8_t* msbReg, uint8_t* lsbReg)
{
	uint16_t regPair = (*msbReg << 8) | *lsbReg;
	state->a = state->memory[regPair]; 
}

//(L) = ((byte 3)(byte 2))
//(H) = ((byte 3) (byte 2) + 1)
//The content of the memory location, whose address is specified in byte 2 and byte 3 of the instruction, is moved to register L. 
//The content of the memory location at the succeeding address is moved to register H.
void LHLD(State8080* state, unsigned char* opcode)
{
	uint16_t memLocation = (opcode[2] << 8) | opcode[1];

	state->l = state->memory[memLocation];
	state-> h = state->memory[memLocation + 1];
}

void LXI(uint8_t* msbReg, uint8_t* lsbReg, unsigned char* opcode)
{
	*lsbReg = opcode[1];
	*msbReg = opcode[2];
}

void ORA(State8080* state, uint8_t var)
{
	uint16_t result = (uint16_t)state->a | (uint16_t)var;
	BcdArithFlags(state, var); 
	state->cc.cy = 0;
	state->cc.ac = 0;
	state->a = result;
}

void POP(State8080* state, uint8_t* msbReg, uint8_t* lsbReg)
{
	*lsbReg = state->memory[state->sp];
	*msbReg = state->memory[state->sp + 1];

	state->sp += 2;
}

void POP_PSW(State8080* state)
{
	uint8_t PSW = state->memory[state->sp];

	// carry flag (CY) <- ((SP))_0 
	state->cc.cy = ((PSW & 0x1) != 0);

	// parity flag (P) <- ((SP))_2
	state->cc.p = ((PSW & 0x4) != 0);

	// auxiliary flag (AC) <- ((SP))_4
	state->cc.ac = ((PSW & 0x10) != 0);

	// zero flag (Z) <- ((SP))_6
	state->cc.z = ((PSW & 0x40) != 0);

	// sign flag (S) <- ((SP))_7
	state->cc.s = ((PSW & 0x80) != 0);

	state->a = state->memory[state->sp + 1];
	state->sp += 2;
}

void PUSH(State8080* state, uint8_t msbReg, uint8_t lsbReg)
{
	state->memory[state->sp - 1] = msbReg; 
	state->memory[state->sp - 2] = lsbReg; 

	state->sp -= 2;
}

void PUSH_PSW(State8080* state)
{	

	state->memory[state->sp - 2] = (state->cc.cy & 0x01) | //0th position
								   (0x02)                | //1st
								   (state->cc.cy << 2)   | //2nd
								   (state->cc.ac << 4)   | //4th
								   (state->cc.z << 6)    | //6th
								   (state->cc.s << 7)    | //7th
								   (0x00);				   //0 in other positions

	state->memory[state->sp - 1] = state->a;
	state->sp -= 2;

}

//Shift Register A 1 bit left and set lsb to pre-shift carry flag
//Carry set to pre-shift bit 7
void RAL(State8080* state)
{
	uint8_t temp = state->a;
	uint8_t msb = (state->a >> 7); 
	state->a = (temp << 1) | (state->cc.cy);
	state->cc.cy = msb;
}

//Shift Register A 1 bit right and set msb to pre-shift msb
//Carry set to pre-shift bit 0
void RAR(State8080* state)
{
	uint8_t temp = state->a;
	uint8_t msb = ((state->a >> 7) << 7); 
	state->a = (temp >> 1) | (msb); 
	state->cc.cy = (temp << 7) >> 7; 
}

//The content of the memory location whose address is specified in register SP is 
//moved to the low-order eight bits of register PC. The content of the memory 
//location whose address is one more than the content of register SP is moved 
//to the high-order eight bits of register PC. 
void RET(State8080* state)
{
	state->pc = (state->memory[state->sp + 1])  << 8| (state->memory[state->sp]);
	state->sp+=2;
}

//Shift Register A 1 bit left and set lsb to pre-shift msb
//Carry set to pre shift bit 7
void RLC(State8080* state)
{
	uint8_t temp = state->a;

	state->a = temp << 1 | temp >> 7; 

	//Carry Flag
	state->cc.cy = ((temp >> 7) > 0);
}

//Shift Register A 1 bit right and set msb to pre-shift lsb
//Carry set to pre shift bit 0
void RRC(State8080* state)
{
	uint8_t temp = state->a;

	state->a = temp >> 1 | temp << 7; 

	//Carry Flag
	state->cc.cy = ((state->a >> 7) > 0);
}

//((byte 3) (byte 2)) = (L)
//((byte 3)(byte 2) + 1) = (H)
//The content of register L is moved to the memory location whose address is specified in byte 2 and byte
//The content of register H is moved to the succeeding memory location.
void SHLD(State8080* state, unsigned char* opcode)
{
	uint16_t memLocation = (opcode[2] << 8) | opcode[1];

	state->memory[memLocation] = state->l;
	state->memory[memLocation + 1] = state->h;
}

void STA(State8080* state, unsigned char* opcode)
{
	uint16_t adress = (opcode[2] << 8) | opcode[1];
	state->memory[adress] = state->a; 
}

void SBB (State8080* state, uint8_t subtrahend)
{
	uint16_t result = (uint16_t)state->a - (uint16_t)subtrahend - (uint16_t)state->cc.cy;
	BcdArithFlags(state, result); 
	state->a = result & 0xff; 
}

void SUB (State8080* state, uint8_t subtrahend)
{
	uint16_t result = (uint16_t)state->a - (uint16_t)subtrahend;
	BcdArithFlags(state, result); 
	state->a = result & 0xff; 
}

//Exchange H and L with D and E
void XCHG(State8080* state)
{
	uint8_t d = state->d;
	uint8_t e = state->e;

	state->d = state->h;
	state->e = state->l;

	state->h = d;
	state->l = e;
}

void XRA(State8080* state, uint8_t var)
{
	uint16_t result = (uint16_t)state->a ^ (uint16_t)var;
	BcdArithFlags(state, result); 
	state->cc.cy = 0;
	state->cc.ac = 0;
	state->a = result;
}

//The content of the L register is exchanged with the content of the 
//memory location whose address is specified by the content of register SP. 
//The content of the H register is exchanged with the content of the 
//memory location whose address is one more than the content of register SP.
void XTHL(State8080* state)
{
	state->l = state->memory[state->sp];
	state->h = state->memory[state->sp + 1];
}



/*
Emulate Intel 8080  CPU Operation
*/
int Emulate(State8080* state, bool printTrace)
{   
    //opcode is a pointer to the current instruction in memory
    unsigned char* opcode = &(state->memory[state->pc]);
    //print out log of CPU operations
    if(printTrace){Disassemble8080(state->memory, state->pc);}
    
    state->pc+=1;
    uint16_t address = (state->h << 8) | (state->l);

    switch(*opcode)    
    {    
	case 0x00: //NOP
		break;                       
	case 0x01: //LXI B,D16
		LXI(&state->b, &state->c, opcode);
		state->pc += 2;                     
		break;
	case 0x02: //STAX B
		{
			uint16_t bc = (state->b << 8) | (state->c);
			state->memory[bc] = state->a; 
		}
		break;
	case 0x03: // INX B
		INX(&state->b, &state->c); 
		break;
	case 0x04: //INR B
		INR(state, &state->b);
		break;
	case 0x05: //DCR B
		DCR(state, &state->b);
		break;
	case 0x06: //MVI B,D8
		state->b = opcode[1];
		state->pc += 1; 
		break;
	case 0x07: //RLC
		RLC(state); 
		break;
	case 0x08: //-
		break;
	case 0x09: //DAD B
		{
			uint32_t bc = (state->b << 8) | (state->c);
			DAD(state, bc); 
		}
		break;
	case 0x0a: //LDAX B
		LDAX(state, &state->b, &state->c); 
		break;
	case 0x0b: //DCX B
		DCX(&state->b, &state->c);
		break;
	case 0x0c: //INR C
		INR(state, &state->c);
		break;
	case 0x0d: //DCR C
		DCR(state, &state->c);
		break;
	case 0x0e: //MVI C,D8
		state->c = opcode[1];
		state->pc += 1; 
		break;
	case 0x0f: //RRC
		RRC(state); 
		break;

	/*....*/
	/*....*/

	case 0x10: //-
		break;
	case 0x11: //LXI D,D16
		LXI(&state->d, &state->e, opcode);
		state->pc += 2;   
		break;
	case 0x12: //STAX D
		{
			uint16_t de = (state->d << 8) | (state->e);
			state->memory[de] = state->a; 
		}
		break;
	case 0x13: //INX D
		INX(&state->d, &state->e);
		break;
	case 0x14: //INR D
		INR(state, &state->d);
		break;
	case 0x15: //DCR D
		DCR(state, &state->d);
		break;
	case 0x16: //MVI D, D8
		state->d = opcode[1];
		state->pc += 1; 
		break;
	case 0x17: //RAL
		RAL(state);
		break;
	case 0x18: //-
		break;
	case 0x19: //DAD D
		{
			uint32_t de = (state->d << 8) | (state->e);
			DAD(state, de);
		}
		break;
	case 0x1a: //LDAX D
		LDAX(state, &state->d, &state->e);
		break;
	case 0x1b: //DCX D
		DCX(&state->d, &state->e);
		break;
	case 0x1c: //INR E
		INR(state, &state->e);
		break;
	case 0x1d: //DCR E
		DCR(state, &state->e);
		break;
	case 0x1e: //MVI E, D8
		state->e = opcode[1];
		state->pc += 1; 
		break;
	case 0x1f: //RAR
		RAR(state); 
		break;

	/*....*/
	/*....*/

	case 0x20: UnimplementedInstruction(state); break; //RIM
	case 0x21: //LXI H,D16
		LXI(&state->h, &state->l, opcode);
		state->pc += 2;
		break;
	case 0x22: //SHLD adr
		SHLD(state, opcode);
		state->pc += 2;
		break;
	case 0x23: //INX H
		INX(&state->h, &state->l);
		break;
	case 0x24: //INR H
		INR(state, &state->h);
		break;
	case 0x25: //DCR H
		DCR(state, &state->h);
		break;
	case 0x26: //MVI H,D8
		state->h = opcode[1]; 
		state->pc += 1;
		break;
	case 0x27: //DAA
		DAA(state);
		break;
	case 0x28: //-
		break;
	case 0x29: //DAD H
		{	
			uint32_t hl = (state->h << 8) | (state->l);
			DAD(state, hl);
		}
		break;
	case 0x2a: //LHLD adr
		LHLD(state, opcode);
		state->pc += 2;
		break;
	case 0x2b: //DCX H
		DCX(&state->h, &state->l);
		break;
	case 0x2c: //INR L
		INR(state, &state->l);
		break;
	case 0x2d: //DCR L
		DCR(state, &state->l);
		break;
	case 0x2e: //MVI L, D8
		state->l = opcode[1]; 
		state->pc += 1;
		break;
	case 0x2f: //CMA
		CMA(state); 
		break;

	/*....*/
	/*....*/

	case 0x30: UnimplementedInstruction(state); break; //SIM
	case 0x31: //LXI SP, D16
		state->sp = (opcode[2]<<8) | opcode[1];
		state->pc += 2;
		break;
	case 0x32: //STA adr
		STA(state, opcode);
		state->pc += 2;
		break;
	case 0x33: //INX SP
		state->sp++; 
		break;
	case 0x34: //INR M
		INR(state, &state->memory[address]);
		break;
	case 0x35: //DCR M
		DCR(state, &state->memory[address]);
		break;
	case 0x36: //MVI M, D8
		state->memory[address] = opcode[1];
		state->pc+=1;
		break;
	case 0x37: //STC
		state->cc.cy = 1;
		break;
	case 0x38: //-
		break;
	case 0x39: //DAD SP
		{	
			uint32_t sp = state->sp;
			DAD(state, sp);
		}
		break;
	case 0x3a: //LDA adr
		LDA(state, opcode); 
		state->pc += 2;
		break;
	case 0x3b: //DCX SP
		state->sp--; 
		break;
	case 0x3c: //INR A
		INR(state, &state->a);
		break;
	case 0x3d: //DCR A
		DCR(state, &state->a);
		break;
	case 0x3e: //MVI A, D8
		state->a = opcode[1];
		state->pc += 1;
		break;
	case 0x3f: //CMC
		CMC(state);
		break;

	/*....*/
	/*....*/

	case 0x40: // MOV B,B
		state->b = state->b;
		break;
	case 0x41: // MOV B,C
		state->b = state->c;
		break;
	case 0x42: // MOV B,D
		state->b = state->d;
		break;
	case 0x43: // MOV B,E
		state->b = state->e;
		break;
	case 0x44: // MOV B,H
		state->b = state->h;
		break;
	case 0x45: // MOV B,L
		state->b = state->l;
		break;
	case 0x46: //MOV B,M
		state->b = state->memory[address];
		break;
	case 0x47: // MOV B,A
		state->b = state->a;
		break;
	case 0x48: //MOV C,B
		state->c = state->b;
		break;
	case 0x49: //MOV C,C
		state->c = state->c;
		break;
	case 0x4a: //MOV C,D
		state->c = state->d;
		break;
	case 0x4b: //MOV C,E
		state->c = state->e;
		break;
	case 0x4c: //MOV C,H
		state->c = state->h;
		break;
	case 0x4d: ////MOV C,L
		state->c = state->l;
		break;
	case 0x4e: //MOV C,M
		state->c = state->memory[address];
		break;
	case 0x4f: //MOV C,A
		state->c = state->a;
		break;

	/*....*/
	/*....*/

	case 0x50: // MOV D,B
		state->d = state->b;
		break;
	case 0x51: // MOV D,C
		state->d = state->c;
		break;
	case 0x52: // MOV D,D
		state->d = state->d;
		break;
	case 0x53: // MOV D,E
		state->d = state->e;
		break;
	case 0x54: // MOV D,H
		state->d = state->h;
		break;
	case 0x55: // MOV D,L
		state->d = state->l;
		break;
	case 0x56: //MOV D,M
		state->d = state->memory[address];
		break;
	case 0x57: // MOV D,A
		state->d = state->a;
		break;
	case 0x58: // MOV E,B
		state->e = state->b;
		break;
	case 0x59: // MOV E,C
		state->e = state->c;
		break;
	case 0x5a: // MOV E,D
		state->e = state->d;
		break;
	case 0x5b: // MOV E,E
		state->e = state->e;
		break;
	case 0x5c: // MOV E,H
		state->e = state->h;
		break;
	case 0x5d: // MOV E,L
		state->e = state->l;
		break;
	case 0x5e: //MOV E,M
		state->e = state->memory[address];
		break;
	case 0x5f: // MOV E,A
		state->e = state->a;
		break;

	/*....*/
	/*....*/

	case 0x60: // MOV H,B
		state->h = state->b;
		break;
	case 0x61: // MOV H,C
		state->h = state->c;
		break;
	case 0x62: // MOV H,D
		state->h = state->d;
		break;
	case 0x63: // MOV H,E
		state->h = state->e;
		break;
	case 0x64: // MOV H,H
		state->h = state->h;
		break;
	case 0x65: // MOV H,L
		state->h = state->l;
		break;
	case 0x66: //MOV H,M
		state->h = state->memory[address];
		break;
	case 0x67: // MOV H,A
		state->h = state->a;
		break;
	case 0x68: // MOV L,B
		state->l = state->b;
		break;
	case 0x69: // MOV L,C
		state->l = state->c;
		break;
	case 0x6a: // MOV L,D
		state->l = state->d;
		break;
	case 0x6b: // MOV L,E
		state->l = state->e;
		break;
	case 0x6c: // MOV L,H
		state->l = state->h;
		break;
	case 0x6d: // MOV L,L
		state->l = state->l;
		break;
	case 0x6e: //MOV L,M
		state->l = state->memory[address];
		break;
	case 0x6f: // MOV L,A
		state->l = state->a;
		break;

	/*....*/
	/*....*/

	case 0x70: //MOV M,B
		state->memory[address] = state->b;
		break;
	case 0x71: //MOV M,C
		state->memory[address] = state->c;
		break;
	case 0x72: //MOV M,D
		state->memory[address] = state->d;
		break;
	case 0x73: // MOV M,E
		state->memory[address] = state->e;
		break;
	case 0x74: // MOV M,H
		state->memory[address] = state->h;
		break;
	case 0x75: // MOV M,L
		state->memory[address] = state->l;
		break;
	case 0x76: // HLT
		exit(0);
		break;
	case 0x77: // MOV M,A
		state->memory[address] = state->a;
		break;
	case 0x78: // MOV A,B
		state->a = state->b;
		break;
	case 0x79: // MOV A,C
		state->a = state->c;
		break;
	case 0x7a: // MOV A,D
		state->a = state->d;
		break;
	case 0x7b: // MOV A,E
		state->a = state->e;
		break;
	case 0x7c: // MOV A,H
		state->a = state->h;
		break;
	case 0x7d: // MOV A,L
		state->a = state->l;
		break;
	case 0x7e: // MOV A,M
		state->a = state->memory[address];
		break;
	case 0x7f: // MOV A,A
		state->a = state->a;
		break;


	/*....*/
	/*....*/

	case 0x80: // ADD B
		ADD(state, state->b);
		break;
	case 0x81: // ADD C
		ADD(state, state->c);
		break;
	case 0x82: // ADD D
		ADD(state, state->d);
		break;
	case 0x83: // ADD E
		ADD(state, state->e);
		break;
	case 0x84: // ADD H
		ADD(state, state->h);
		break;
	case 0x85: // ADD L
		ADD(state, state->l);
		break;
	case 0x86: // ADD M
		ADD(state, state->memory[address]);
		break;
	case 0x87: // ADD A
		ADD(state, state->a);
		break;
	case 0x88: // ADC B
		ADC(state, state->b);
		break;
	case 0x89: // ADC C 
		ADC(state, state->c);
		break;
	case 0x8a: // ADC D
		ADC(state, state->d);
		break;
	case 0x8b: // ADC E
		ADC(state, state->e);
		break;
	case 0x8c: // ADC H
		ADC(state, state->h);
		break;
	case 0x8d: // ADC L
		ADC(state, state->l);
		break;
	case 0x8e: // ADC M
		ADC(state, state->memory[address]); 
		break;
	case 0x8f: // ADC A
		ADC(state, state->a);
		break;

	/*....*/
	/*....*/

	case 0x90: // SUB B
		SUB(state, state->b); 
		break;
	case 0x91: // SUB C
		SUB(state, state->c); 
		break;
	case 0x92: // SUB D
		SUB(state, state->d); 
		break;
	case 0x93: // SUB E
		SUB(state, state->e); 
		break;
	case 0x94: // SUB H
		SUB(state, state->h); 
		break;
	case 0x95: // SUB L
		SUB(state, state->l); 
		break;
	case 0x96: // SUB M
		SUB(state, state->memory[address]); 
		break;
	case 0x97: // SUB A
		SUB(state, state->a); 
		break;
	case 0x98: // SBB B
		SBB(state, state->b);
		break;
	case 0x99: // SBB C
		SBB(state, state->c);
		break;
	case 0x9a: // SBB D
		SBB(state, state->d);
		break;
	case 0x9b: // SBB E
		SBB(state, state->e);
		break;
	case 0x9c: // SBB H
		SBB(state, state->h);
		break;
	case 0x9d: // SBB L
		SBB(state, state->l);
		break;
	case 0x9e: // SBB M
		SBB(state, state->memory[address]);
		break;
	case 0x9f: // SBB A
		SBB(state, state->a);
		break;

	/*....*/       
	/*....*/  

	case 0xa0: // ANA B
		ANA(state, state->b);
		break;
	case 0xa1: // ANA C
		ANA(state, state->c);
		break;
	case 0xa2: // ANA D
		ANA(state, state->d);
		break;
	case 0xa3: // ANA E
		ANA(state, state->e);
		break;
	case 0xa4: // ANA H
		ANA(state, state->h);
		break;
	case 0xa5: // ANA L
		ANA(state, state->l);
		break;
	case 0xa6: // ANA M
		ANA(state, state->memory[address]);
		break;
	case 0xa7: // ANA A
		ANA(state, state->a);
		break;
	case 0xa8: // XRA B
		XRA(state, state->b);
		break;
	case 0xa9: // XRA C
		XRA(state, state->c);
		break;
	case 0xaa: // XRA D
		XRA(state, state->d);
		break;
	case 0xab: // XRA E
		XRA(state, state->e);
		break;
	case 0xac: // XRA H
		XRA(state, state->h);
		break;
	case 0xad: // XRA L
		XRA(state, state->l);
		break;
	case 0xae: // XRA M
		XRA(state, state->memory[address]);
		break;
	case 0xaf: // XRA A
		XRA(state, state->a);
		break;

	/*....*/       
	/*....*/ 

	case 0xb0: // ORA B
		ORA(state, state->b);
		break;
	case 0xb1: // ORA C 
		ORA(state, state->c);
		break;
	case 0xb2: // ORA D
		ORA(state, state->d);
		break;
	case 0xb3: // ORA E
		ORA(state, state->e);
		break;
	case 0xb4: // ORA H
		ORA(state, state->h);
		break;
	case 0xb5: // ORA L
		ORA(state, state->l);
		break;
	case 0xb6: // ORA M
		ORA(state, state->memory[address]);
		break;
	case 0xb7: // ORA A
		ORA(state, state->a);
		break;
	case 0xb8: // CMP B
		CMP(state, state->b);
		break;
	case 0xb9: // CMP C
		CMP(state, state->c);
		break;
	case 0xba: // CMP D
		CMP(state, state->d);
		break;
	case 0xbb: // CMP E
		CMP(state, state->e);
		break;
	case 0xbc: // CMP H
		CMP(state, state->h);
		break;
	case 0xbd: // CMP L
		CMP(state, state->l);
		break;
	case 0xbe: // CMP M
		CMP(state, state->memory[address]);
		break;
	case 0xbf: // CMP A
		CMP(state, state->a);
		break;

	/*....*/  
	/*....*/

	case 0xc0: // RNZ
		if (state->cc.z == 0)
		{
			RET(state);
		}
		break;
	case 0xc1: // POP B
		POP(state, &state->b, &state->c);
		break;
	case 0xc2: // JNZ adr
		if(state->cc.z == 0)
		{
			JMP(state, opcode); 
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xc3: // JMP adr
		JMP(state, opcode);
		break;
	case 0xc4: // CNZ adr
		if(state->cc.z == 0)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xc5: // PUSH B
		PUSH(state, state->b, state->c); 
		break;
	case 0xc6: // ADI D8
		ADD(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xc7: // RST 0
		{	  //  CALL $0
			unsigned char tmp[3];
			tmp[1] = 0;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;
	case 0xc8: // RZ
		if (state->cc.z == 1)
		{
			RET(state);
		}
		break;
	case 0xc9: // RET
		RET(state);
		break;
	case 0xca: // JZ adr
		if (state->cc.z == 1)
		{
			JMP(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xcb: // -
		break;
	case 0xcc: //CZ adr
		if (state->cc.z == 1)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xcd: // CALL adr
		CALL(state, opcode);
		break;
	case 0xce: // ACI D8
		ADC(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xcf: // RST 1
		{	  //  CALL $8
			unsigned char tmp[3];
			tmp[1] = 8;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;

	/*....*/  
	/*....*/

	case 0xd0: // RNC
		if (state->cc.cy == 0)
		{
			RET(state);
		}
		break;
	case 0xd1: // POP D
		POP(state, &state->d, &state->e);
		break;
	case 0xd2: // JNC adr
		if(state->cc.cy == 0)
		{
			JMP(state, opcode); 
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xd3: // OUT D8
		break;
	case 0xd4: // CNC adr
		if(state->cc.cy == 0)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xd5: // PUSH D
		PUSH(state, state->d, state->e);
		break;
	case 0xd6: // SUI D8
		SUB(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xd7: // RST 2
		{	  //  CALL $10
			unsigned char tmp[3];
			tmp[1] = 16;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;
	case 0xd8: // RC
		if (state->cc.cy == 1)
		{
			RET(state);
		}
		break;
	case 0xd9: // -
		break;
	case 0xda: // JC adr
		if(state->cc.cy == 1)
		{
			JMP(state, opcode); 
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xdb: // IN D8
		break;
	case 0xdc: // CC adr
		if(state->cc.cy == 1)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xdd: // -
		break;
	case 0xde: // SBI D8
		SBB(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xdf: // RST 3
		{	  //  CALL $18
			unsigned char tmp[3];
			tmp[1] = 24;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;


	/*....*/  
	/*....*/ 

	case 0xe0: // RPO
		if (state->cc.p == 0)
		{
			RET(state);
		}
		break;
	case 0xe1: // POP H
		POP(state, &state->h, &state->l);
		break;
	case 0xe2: // JPO adr
		if(state->cc.p == 0)
		{
			JMP(state, opcode); 
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xe3: // XTHL
		XTHL(state);
		break;
	case 0xe4: // CPO adr
		if(state->cc.p == 0)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xe5: // PUSH H
		PUSH(state, state->h, state->l);
		break;
	case 0xe6: // ANI D8
		ANA(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xe7: // RST 4
		{	  //  CALL $20
			unsigned char tmp[3];
			tmp[1] = 32;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;
	case 0xe8: // RPE
		if (state->cc.p == 1)
		{
			RET(state);
		}
		break;
	case 0xe9: // PCHL
		state->pc = address;
		break;
	case 0xea: // JPE adr
		if(state->cc.p == 1)
		{
			JMP(state, opcode); 
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xeb: // XCHG
		XCHG(state);
		break;
	case 0xec: // CPE adr 
		if(state->cc.p == 1)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xed: // -
		break;
	case 0xee: // XRI D8
		XRA(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xef: // RST 5
		{	  //  CALL $28
			unsigned char tmp[3];
			tmp[1] = 40;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;

	/*....*/  
	/*....*/  

	case 0xf0: // RP
		if (state->cc.s == 0)
		{
			RET(state);
		}
		break;
	case 0xf1: // POP PSW
		POP_PSW(state);
		break;
	case 0xf2: // JP adr
		if(state->cc.s == 0)
		{
			JMP(state, opcode); 
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xf3: // DI 
		state->int_enable = 0;
		break;
	case 0xf4: // CP adr
		if(state->cc.s == 0)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xf5: // PUSH PSW
		PUSH_PSW(state);
		break;
	case 0xf6: // ORI D8
		ORA(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xf7: // RST 6
		{     // CALL $30
			unsigned char tmp[3];
			tmp[1] = 48;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;
	case 0xf8: // RM
		if (state->cc.s == 1)
		{
			RET(state);
		}
		break;
	case 0xf9: // SPHL
			  //  Move HL to SP
		state->sp = address;
		break;
	case 0xfa: // JM adr
		if(state->cc.s == 1)
		{
			JMP(state, opcode); 
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xfb: // EI
		state->int_enable = 1;
		break; 
	case 0xfc: // CM adr
		if(state->cc.s == 1)
		{
			CALL(state, opcode);
		}
		else
		{
			state->pc+=2;
		}
		break;
	case 0xfd: // -
		break;
	case 0xfe: // CPI D8
		//compares value in accumulator with next value
		CMP(state, opcode[1]);
		state->pc+=1;
		break;
	case 0xff: // RST 7
		{     // CALL $38
			unsigned char tmp[3];
			tmp[1] = 56;
			tmp[2] = 0;
			CALL(state, tmp);
		}
		break;
	/*....*/  
	default: UnimplementedInstruction(state); break;
    }    
    
    return cycles8080[*opcode];
}

int Disassemble8080(unsigned char *codebuffer, int pc)  
{    
    //code is a pointer to the current opcode which is determined by the offset from the program counter (pc)
    unsigned char *code = &codebuffer[pc];    
    //represents the the size of the instruction in bytes. Min of 1, if the opcode has data afterward each byte of data afterward is counted as well
    //size can range from 1 to 3 bytes
    int opbytes = 1;    

    printf( "%04x ",pc);    
    switch (*code)    
    {    
	case 0x00: printf("NOP"); break;
	case 0x01: printf("LXI    B,#$%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x02: printf("STAX   B"); break;
	case 0x03: printf("INX    B"); break;
	case 0x04: printf("INR    B"); break;
	case 0x05: printf("DCR    B"); break;
	case 0x06: printf("MVI    B,#$%02x", code[1]); opbytes=2; break;
	case 0x07: printf("RLC"); break;
	case 0x08: printf("NOP"); break;
	case 0x09: printf("DAD    B"); break;
	case 0x0a: printf("LDAX   B"); break;
	case 0x0b: printf("DCX    B"); break;
	case 0x0c: printf("INR    C"); break;
	case 0x0d: printf("DCR    C"); break;
	case 0x0e: printf("MVI    C,#$%02x", code[1]); opbytes = 2;	break;
	case 0x0f: printf("RRC"); break;
			
	case 0x10: printf("NOP"); break;
	case 0x11: printf("LXI    D,#$%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x12: printf("STAX   D"); break;
	case 0x13: printf("INX    D"); break;
	case 0x14: printf("INR    D"); break;
	case 0x15: printf("DCR    D"); break;
	case 0x16: printf("MVI    D,#$%02x", code[1]); opbytes=2; break;
	case 0x17: printf("RAL"); break;
	case 0x18: printf("NOP"); break;
	case 0x19: printf("DAD    D"); break;
	case 0x1a: printf("LDAX   D"); break;
	case 0x1b: printf("DCX    D"); break;
	case 0x1c: printf("INR    E"); break;
	case 0x1d: printf("DCR    E"); break;
	case 0x1e: printf("MVI    E,#$%02x", code[1]); opbytes = 2; break;
	case 0x1f: printf("RAR"); break;
			
	case 0x20: printf("NOP"); break;
	case 0x21: printf("LXI    H,#$%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x22: printf("SHLD   $%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x23: printf("INX    H"); break;
	case 0x24: printf("INR    H"); break;
	case 0x25: printf("DCR    H"); break;
	case 0x26: printf("MVI    H,#$%02x", code[1]); opbytes=2; break;
	case 0x27: printf("DAA"); break;
	case 0x28: printf("NOP"); break;
	case 0x29: printf("DAD    H"); break;
	case 0x2a: printf("LHLD   $%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x2b: printf("DCX    H"); break;
	case 0x2c: printf("INR    L"); break;
	case 0x2d: printf("DCR    L"); break;
	case 0x2e: printf("MVI    L,#$%02x", code[1]); opbytes = 2; break;
	case 0x2f: printf("CMA"); break;
			
	case 0x30: printf("NOP"); break;
	case 0x31: printf("LXI    SP,#$%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x32: printf("STA    $%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x33: printf("INX    SP"); break;
	case 0x34: printf("INR    M"); break;
	case 0x35: printf("DCR    M"); break;
	case 0x36: printf("MVI    M,#$%02x", code[1]); opbytes=2; break;
	case 0x37: printf("STC"); break;
	case 0x38: printf("NOP"); break;
	case 0x39: printf("DAD    SP"); break;
	case 0x3a: printf("LDA    $%02x%02x", code[2], code[1]); opbytes=3; break;
	case 0x3b: printf("DCX    SP"); break;
	case 0x3c: printf("INR    A"); break;
	case 0x3d: printf("DCR    A"); break;
	case 0x3e: printf("MVI    A,#$%02x", code[1]); opbytes = 2; break;
	case 0x3f: printf("CMC"); break;
			
	case 0x40: printf("MOV    B,B"); break;
	case 0x41: printf("MOV    B,C"); break;
	case 0x42: printf("MOV    B,D"); break;
	case 0x43: printf("MOV    B,E"); break;
	case 0x44: printf("MOV    B,H"); break;
	case 0x45: printf("MOV    B,L"); break;
	case 0x46: printf("MOV    B,M"); break;
	case 0x47: printf("MOV    B,A"); break;
	case 0x48: printf("MOV    C,B"); break;
	case 0x49: printf("MOV    C,C"); break;
	case 0x4a: printf("MOV    C,D"); break;
	case 0x4b: printf("MOV    C,E"); break;
	case 0x4c: printf("MOV    C,H"); break;
	case 0x4d: printf("MOV    C,L"); break;
	case 0x4e: printf("MOV    C,M"); break;
	case 0x4f: printf("MOV    C,A"); break;
			
	case 0x50: printf("MOV    D,B"); break;
	case 0x51: printf("MOV    D,C"); break;
	case 0x52: printf("MOV    D,D"); break;
	case 0x53: printf("MOV    D.E"); break;
	case 0x54: printf("MOV    D,H"); break;
	case 0x55: printf("MOV    D,L"); break;
	case 0x56: printf("MOV    D,M"); break;
	case 0x57: printf("MOV    D,A"); break;
	case 0x58: printf("MOV    E,B"); break;
	case 0x59: printf("MOV    E,C"); break;
	case 0x5a: printf("MOV    E,D"); break;
	case 0x5b: printf("MOV    E,E"); break;
	case 0x5c: printf("MOV    E,H"); break;
	case 0x5d: printf("MOV    E,L"); break;
	case 0x5e: printf("MOV    E,M"); break;
	case 0x5f: printf("MOV    E,A"); break;

	case 0x60: printf("MOV    H,B"); break;
	case 0x61: printf("MOV    H,C"); break;
	case 0x62: printf("MOV    H,D"); break;
	case 0x63: printf("MOV    H.E"); break;
	case 0x64: printf("MOV    H,H"); break;
	case 0x65: printf("MOV    H,L"); break;
	case 0x66: printf("MOV    H,M"); break;
	case 0x67: printf("MOV    H,A"); break;
	case 0x68: printf("MOV    L,B"); break;
	case 0x69: printf("MOV    L,C"); break;
	case 0x6a: printf("MOV    L,D"); break;
	case 0x6b: printf("MOV    L,E"); break;
	case 0x6c: printf("MOV    L,H"); break;
	case 0x6d: printf("MOV    L,L"); break;
	case 0x6e: printf("MOV    L,M"); break;
	case 0x6f: printf("MOV    L,A"); break;

	case 0x70: printf("MOV    M,B"); break;
	case 0x71: printf("MOV    M,C"); break;
	case 0x72: printf("MOV    M,D"); break;
	case 0x73: printf("MOV    M.E"); break;
	case 0x74: printf("MOV    M,H"); break;
	case 0x75: printf("MOV    M,L"); break;
	case 0x76: printf("HLT");        break;
	case 0x77: printf("MOV    M,A"); break;
	case 0x78: printf("MOV    A,B"); break;
	case 0x79: printf("MOV    A,C"); break;
	case 0x7a: printf("MOV    A,D"); break;
	case 0x7b: printf("MOV    A,E"); break;
	case 0x7c: printf("MOV    A,H"); break;
	case 0x7d: printf("MOV    A,L"); break;
	case 0x7e: printf("MOV    A,M"); break;
	case 0x7f: printf("MOV    A,A"); break;

	case 0x80: printf("ADD    B"); break;
	case 0x81: printf("ADD    C"); break;
	case 0x82: printf("ADD    D"); break;
	case 0x83: printf("ADD    E"); break;
	case 0x84: printf("ADD    H"); break;
	case 0x85: printf("ADD    L"); break;
	case 0x86: printf("ADD    M"); break;
	case 0x87: printf("ADD    A"); break;
	case 0x88: printf("ADC    B"); break;
	case 0x89: printf("ADC    C"); break;
	case 0x8a: printf("ADC    D"); break;
	case 0x8b: printf("ADC    E"); break;
	case 0x8c: printf("ADC    H"); break;
	case 0x8d: printf("ADC    L"); break;
	case 0x8e: printf("ADC    M"); break;
	case 0x8f: printf("ADC    A"); break;

	case 0x90: printf("SUB    B"); break;
	case 0x91: printf("SUB    C"); break;
	case 0x92: printf("SUB    D"); break;
	case 0x93: printf("SUB    E"); break;
	case 0x94: printf("SUB    H"); break;
	case 0x95: printf("SUB    L"); break;
	case 0x96: printf("SUB    M"); break;
	case 0x97: printf("SUB    A"); break;
	case 0x98: printf("SBB    B"); break;
	case 0x99: printf("SBB    C"); break;
	case 0x9a: printf("SBB    D"); break;
	case 0x9b: printf("SBB    E"); break;
	case 0x9c: printf("SBB    H"); break;
	case 0x9d: printf("SBB    L"); break;
	case 0x9e: printf("SBB    M"); break;
	case 0x9f: printf("SBB    A"); break;

	case 0xa0: printf("ANA    B"); break;
	case 0xa1: printf("ANA    C"); break;
	case 0xa2: printf("ANA    D"); break;
	case 0xa3: printf("ANA    E"); break;
	case 0xa4: printf("ANA    H"); break;
	case 0xa5: printf("ANA    L"); break;
	case 0xa6: printf("ANA    M"); break;
	case 0xa7: printf("ANA    A"); break;
	case 0xa8: printf("XRA    B"); break;
	case 0xa9: printf("XRA    C"); break;
	case 0xaa: printf("XRA    D"); break;
	case 0xab: printf("XRA    E"); break;
	case 0xac: printf("XRA    H"); break;
	case 0xad: printf("XRA    L"); break;
	case 0xae: printf("XRA    M"); break;
	case 0xaf: printf("XRA    A"); break;

	case 0xb0: printf("ORA    B"); break;
	case 0xb1: printf("ORA    C"); break;
	case 0xb2: printf("ORA    D"); break;
	case 0xb3: printf("ORA    E"); break;
	case 0xb4: printf("ORA    H"); break;
	case 0xb5: printf("ORA    L"); break;
	case 0xb6: printf("ORA    M"); break;
	case 0xb7: printf("ORA    A"); break;
	case 0xb8: printf("CMP    B"); break;
	case 0xb9: printf("CMP    C"); break;
	case 0xba: printf("CMP    D"); break;
	case 0xbb: printf("CMP    E"); break;
	case 0xbc: printf("CMP    H"); break;
	case 0xbd: printf("CMP    L"); break;
	case 0xbe: printf("CMP    M"); break;
	case 0xbf: printf("CMP    A"); break;

	case 0xc0: printf("RNZ"); break;
	case 0xc1: printf("POP    B"); break;
	case 0xc2: printf("JNZ    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xc3: printf("JMP    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xc4: printf("CNZ    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xc5: printf("PUSH   B"); break;
	case 0xc6: printf("ADI    #$%02x",code[1]); opbytes = 2; break;
	case 0xc7: printf("RST    0"); break;
	case 0xc8: printf("RZ"); break;
	case 0xc9: printf("RET"); break;
	case 0xca: printf("JZ     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xcb: printf("JMP    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xcc: printf("CZ     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xcd: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xce: printf("ACI    #$%02x",code[1]); opbytes = 2; break;
	case 0xcf: printf("RST    1"); break;

	case 0xd0: printf("RNC"); break;
	case 0xd1: printf("POP    D"); break;
	case 0xd2: printf("JNC    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xd3: printf("OUT    #$%02x",code[1]); opbytes = 2; break;
	case 0xd4: printf("CNC    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xd5: printf("PUSH   D"); break;
	case 0xd6: printf("SUI    #$%02x",code[1]); opbytes = 2; break;
	case 0xd7: printf("RST    2"); break;
	case 0xd8: printf("RC");  break;
	case 0xd9: printf("RET"); break;
	case 0xda: printf("JC     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xdb: printf("IN     #$%02x",code[1]); opbytes = 2; break;
	case 0xdc: printf("CC     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xdd: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xde: printf("SBI    #$%02x",code[1]); opbytes = 2; break;
	case 0xdf: printf("RST    3"); break;

	case 0xe0: printf("RPO"); break;
	case 0xe1: printf("POP    H"); break;
	case 0xe2: printf("JPO    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xe3: printf("XTHL");break;
	case 0xe4: printf("CPO    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xe5: printf("PUSH   H"); break;
	case 0xe6: printf("ANI    #$%02x",code[1]); opbytes = 2; break;
	case 0xe7: printf("RST    4"); break;
	case 0xe8: printf("RPE"); break;
	case 0xe9: printf("PCHL");break;
	case 0xea: printf("JPE    $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xeb: printf("XCHG"); break;
	case 0xec: printf("CPE     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xed: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xee: printf("XRI    #$%02x",code[1]); opbytes = 2; break;
	case 0xef: printf("RST    5"); break;

	case 0xf0: printf("RP");  break;
	case 0xf1: printf("POP    PSW"); break;
	case 0xf2: printf("JP     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xf3: printf("DI");  break;
	case 0xf4: printf("CP     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xf5: printf("PUSH   PSW"); break;
	case 0xf6: printf("ORI    #$%02x",code[1]); opbytes = 2; break;
	case 0xf7: printf("RST    6"); break;
	case 0xf8: printf("RM");  break;
	case 0xf9: printf("SPHL");break;
	case 0xfa: printf("JM     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xfb: printf("EI");  break;
	case 0xfc: printf("CM     $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xfd: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
	case 0xfe: printf("CPI    #$%02x",code[1]); opbytes = 2; break;
	case 0xff: printf("RST    7"); break;
    }    
    printf("\r\n");    
    return opbytes;    
}

void GenInterrupt(State8080* state, int interrupt_num)
{
	//Push PC
	PUSH(state, ((state->pc & 0xff00) >> 8), (state->pc & 0xff));

	//Set PC to low memory vector
	//RST interrupt_num
	state->pc = 8 * interrupt_num;
	state->int_enable = 0;
}

void ReadFileIntoMemory(State8080* state, char* filename, uint32_t offset)
{
    FILE *f= fopen(filename, "rb");

    if (f == NULL)
    {
        printf("error opening file: %s\n", filename);
        exit(1);
    }

    fseek(f, 0L, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0L, SEEK_SET);
    
    //fread reads data from the given stream into the array pointed to, by ptr (buffer).
    uint8_t *buffer = &state->memory[offset];
    fread(buffer, fsize, 1, f);
    fclose(f);
}

State8080* Init8080(void)
{
	State8080* state = calloc(1,sizeof(State8080)); 
	state->memory = malloc(0x10000); //allocate 16K
	return state;
}