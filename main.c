#include <stdio.h>
#include <assert.h>

#define Byte unsigned char
#define byte char
#define Short unsigned short

struct CPU {
    Short PC;
    Short SP;
    Short Mem[0XFFFF];
    Byte A, X, Y;
    Byte F_N;
    Byte F_V;
    Byte F_B;
    Byte F_D;
    Byte F_I;
    Byte F_Z;
    Byte F_C;
    Byte INS_Cycles;
} CPU;

void CPU_Flag_N(Byte a);

void CPU_Reset() {
    CPU.PC = 0x0600;
    CPU.SP = 0x100;
    CPU.A = CPU.X = CPU.Y = 0;
    CPU.F_N = CPU.F_V = CPU.F_B = CPU.F_D = CPU.F_I = CPU.F_Z = CPU.F_C = 0;
}

Byte CPU_Read_Addr(Short addr) {
    CPU.INS_Cycles += 1;
    return CPU.Mem[addr];
}

Byte CPU_Get_Byte() {
    return CPU_Read_Addr(CPU.PC++);
}

Short concat_byte(Byte low, Byte high) {
    return low | (high << 8);
}

Byte CPU_Write_Addr(Short addr, Byte value) {
    CPU.INS_Cycles += 1;
    return CPU.Mem[addr] = value;
}

//-------------寻址方式开始-----------------
// AM: Addressing Mode

/**
 * 直接寻址
 * @param reg
 * @return
 */
Short AM_IMM() {

    return CPU_Get_Byte();
}

/**
 * 绝对寻址
 * Absolute: a
 * @param low
 * @param high
 * @return
 */
Short AM_Abs() {
    Byte low = CPU_Get_Byte();
    Byte high = CPU_Get_Byte();
    return concat_byte(low, high);
}

/**
 * 绝对索引寻址
 * Absolute Indexed with REG: a,REG
 * REG = CPU.X/CPU.Y
 * @param low
 * @param high
 * @param reg
 * @return
 */
Short AM_Abs_XY(Byte reg) {
    Short abs = AM_Abs();
    Short absRegAddr = abs + reg;
    //page boundary is crossed
    CPU.INS_Cycles += (abs ^ absRegAddr) >> 8;
    return absRegAddr;
}

/**
 * 零页寻址
 * @return
 */
Short AM_ZP() {

    return CPU_Get_Byte();
}

/**
 * 零页索引寻址
 * @param reg
 * @return
 */
Short AM_ZP_XY(Byte reg) {
    CPU.INS_Cycles++;
    return AM_ZP() + reg;
}

/**
 * 零页索引直接寻址 X
 * @return
 */
Short AM_ZP_IND_X() {
    Short low = AM_ZP_XY(CPU.X);
    return concat_byte(CPU_Read_Addr(low), CPU_Read_Addr(low + 1));
}

/**
 * 零页直接索引寻址 Y
 * @return
 */
Short AM_ZP_IND_Y() {
    Short low = AM_ZP();
    Short address_1 = concat_byte(CPU_Read_Addr(low), CPU_Read_Addr(low + 1));
    Short address_2 = address_1 + CPU.Y;
    //page boundary is crossed
    CPU.INS_Cycles += (address_1 ^ address_2) >> 8;
    return address_2;
}

//-------------寻址方式结束-----------------

//-------------FLAG设置开始-----------------

void CPU_F_NZ(Byte data) {
    CPU.F_N = (data >> 8) & 1;
    CPU.F_Z = data == 0;
}

//-------------FLAG设置结束-----------------

//-------------指令开始-----------------

/**
 * AXY寄存器设置值
 * @param value 值
 * @param reg AXY寄存器
 */
void INS_Set_REG(Byte value, Byte *reg) {
    *reg = value;
    CPU_F_NZ(*reg);
    CPU.INS_Cycles += 1;
}

/**
 * 保存寄存器的值到内存地址
 * @param value 值
 * @param reg AXY寄存器
 */
void INS_REG_To_MEM(Short address, Byte REG_Value) {
    CPU.INS_Cycles += 1;
    CPU_Write_Addr(address, REG_Value);
}

/**
 * 相加保存寄存器A
 * A + M + C -> A
 * Flags: N, V, Z, C
 * @param value M值
 */
void INS_ADC(Byte value) {
    CPU.INS_Cycles += 1;
    //结果为0x80代表是相同符号 返回0代表不同符号 同符号相加减才会溢出/进位
    Byte same_sign = (CPU.A ^ value) >> 7;
    Short sum_value = CPU.A + value + CPU.F_C;
    CPU.A = sum_value & 0xFF;
    CPU_F_NZ(CPU.A);
    //无符号数越界->进位 0-256
    CPU.F_C = sum_value > 0xFF;
    //有符号数越界->溢出 -128-127
    CPU.F_V = same_sign && ((CPU.A ^ value)>>7);
}

/**
 * 相减保存寄存器A
 * A - M - ~C -> A
 * Flags: N, V, Z, C
 * @param value M值
 */
void INS_SBC(Byte value) {
    INS_ADC(~value);
}

/**
 * 内存值加减
 * M + 1 -> M
 * Flags: N, Z
 * @param value M值
 * @param value2 1/-1
 */
void INS_INC_DEC(Short address,byte value) {
    CPU.INS_Cycles += 2;
    Byte data = CPU_Read_Addr(address);
    data = data + value;
    CPU_Write_Addr(address, data);
    CPU_F_NZ(data);
}

/**
 * 内存值加减
 * M + 1 -> M
 * Flags: N, Z
 * @param value M值
 * @param value2 1/-1
 */
void INS_INC_DEC_XY(Byte *REG,byte value) {
    *REG = *REG + value;
    CPU_F_NZ(*REG);
}

/**
 * 左移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_ASL(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = (value & 0x80) > 0;
    value<<=1;
    CPU_F_NZ(value);
    return value;
}

/**
 * 右移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_LSR(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = (value & 1) > 0;
    value>>=1;
    CPU_F_NZ(value);
    return value;
}


/**
 * 循环左移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_ROL(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = (value>>7) & 1;
    value<<=1;
    value|=CPU.F_C;
    CPU_F_NZ(value);
    return value;
}

/**
 * 循环右移一位
 * Flags: N, Z, C
 * @param value 值
 */
Byte INS_ROR(Byte value) {
    CPU.INS_Cycles += 1;
    CPU.F_C = value & 1;
    value>>=1;
    value|=(CPU.F_C<<7);
    CPU_F_NZ(value);
    return value;
}

//-------------指令结束-----------------

void CPU_Exec() {
    Byte opcode = CPU_Get_Byte();
    CPU.INS_Cycles = 0;
    Short addr;
    switch (opcode) {
        // ------------Load(加载到寄存器)------------
            //LDA #
        case 0xA9:
            INS_Set_REG(AM_IMM(), &CPU.A);
            break;
            //LDA a
        case 0xAD:
            INS_Set_REG(CPU_Read_Addr(AM_Abs()), &CPU.A);
            break;
            //LDA a,x
        case 0xBD:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.X)), &CPU.A);
            break;
            //LDA a,y
        case 0xB9:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.Y)), &CPU.A);
            break;
            //LDA zp
        case 0xA5:
            INS_Set_REG(CPU_Read_Addr(AM_ZP()), &CPU.A);
            break;
            //LDA zp,x
        case 0xB5:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_XY(CPU.X)), &CPU.A);
            break;
            //LDA (Indirect,X)
        case 0xA1:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_IND_X()), &CPU.A);
            break;
            //LDA (Indirect),Y
        case 0xB1:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_IND_Y()), &CPU.A);
            break;

            //LDX #
        case 0xA2:
            INS_Set_REG(AM_IMM(), &CPU.X);
            break;
            //LDX a
        case 0xAE:
            INS_Set_REG(CPU_Read_Addr(AM_Abs()), &CPU.X);
            break;
            //LDX a,y
        case 0xBE:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.Y)), &CPU.X);
            break;
            //LDX zp
        case 0xA6:
            INS_Set_REG(CPU_Read_Addr(AM_ZP()), &CPU.X);
            break;
            //LDX zp,y
        case 0xB6:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_XY(CPU.Y)), &CPU.X);
            break;

            //LDY #
        case 0xA0:
            INS_Set_REG(AM_IMM(), &CPU.Y);
            break;
            //LDY a
        case 0xAC:
            INS_Set_REG(CPU_Read_Addr(AM_Abs()), &CPU.Y);
            break;
            //LDY a,x
        case 0xBC:
            INS_Set_REG(CPU_Read_Addr(AM_Abs_XY(CPU.X)), &CPU.Y);
            break;
            //LDY zp
        case 0xA4:
            INS_Set_REG(CPU_Read_Addr(AM_ZP()), &CPU.Y);
            break;
            //LDY zp,x
        case 0xB4:
            INS_Set_REG(CPU_Read_Addr(AM_ZP_XY(CPU.X)), &CPU.Y);
            break;
        // ------------Store(寄存器存储到内存)------------
            //STA a
        case 0x8D:
            INS_REG_To_MEM(AM_Abs(), CPU.A);
            break;
            //STA a,x
        case 0x9D:
            INS_REG_To_MEM(AM_Abs_XY(CPU.X), CPU.A);
            break;
            //STA a,y
        case 0x99:
            INS_REG_To_MEM(AM_Abs_XY(CPU.Y), CPU.A);
            break;
            //STA zp
        case 0x85:
            INS_REG_To_MEM(AM_ZP(), CPU.A);
            break;
            //STA zp,x
        case 0x95:
            INS_REG_To_MEM(AM_ZP_XY(CPU.X), CPU.A);
            break;
            //STA (zp,x)
        case 0x81:
            INS_REG_To_MEM(AM_ZP_IND_X(), CPU.A);
            break;
            //STA (zp),y
        case 0x91:
            INS_REG_To_MEM(AM_ZP_IND_Y(), CPU.A);
            break;

            //STX a
        case 0x8E:
            INS_REG_To_MEM(AM_Abs(), CPU.X);
            break;
            //STX zp
        case 0x86:
            INS_REG_To_MEM(AM_ZP(), CPU.X);
            break;
            //STX zp,y
        case 0x96:
            INS_REG_To_MEM(AM_ZP_XY(CPU.Y), CPU.X);
            break;

            //STY a
        case 0x8C:
            INS_REG_To_MEM(AM_Abs(), CPU.Y);
            break;
            //STY zp
        case 0x84:
            INS_REG_To_MEM(AM_ZP(), CPU.Y);
            break;
            //STY zp,x
        case 0x94:
            INS_REG_To_MEM(AM_ZP_XY(CPU.X), CPU.Y);
            break;
        // ------------Arithmetic(算数)------------
            //ADC #   (Add Memory to Accumulator with Carry)
        case 0x69:
            INS_ADC(AM_IMM());
            break;
            //ADC a
        case 0x6D:
            INS_ADC(CPU_Read_Addr(AM_Abs()));
            break;
            //ADC a,x
        case 0x7D:
            INS_ADC(CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //ADC a,y
        case 0x79:
            INS_ADC(CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //ADC zp
        case 0x65:
            INS_ADC(CPU_Read_Addr(AM_ZP()));
            break;
            //ADC zp,x
        case 0x75:
            INS_ADC(CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //ADC (Indirect,X)
        case 0x61:
            INS_ADC(CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //ADC (Indirect),Y
        case 0x71:
            INS_ADC(CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

            //SBC #   (Subtract Memory from Accumulator with Borrow)
        case 0xE9:
            INS_SBC(AM_IMM());
            break;
            //SBC a
        case 0xED:
            INS_SBC(CPU_Read_Addr(AM_Abs()));
            break;
            //SBC a,x
        case 0xFD:
            INS_SBC(CPU_Read_Addr(AM_Abs_XY(CPU.X)));
            break;
            //SBC a,y
        case 0xF9:
            INS_SBC(CPU_Read_Addr(AM_Abs_XY(CPU.Y)));
            break;
            //SBC zp
        case 0xE5:
            INS_SBC(CPU_Read_Addr(AM_ZP()));
            break;
            //SBC zp,x
        case 0xF5:
            INS_SBC(CPU_Read_Addr(AM_ZP_XY(CPU.X)));
            break;
            //SBC (Indirect,X)
        case 0xE1:
            INS_SBC(CPU_Read_Addr(AM_ZP_IND_X()));
            break;
            //SBC (Indirect),Y
        case 0xF1:
            INS_SBC(CPU_Read_Addr(AM_ZP_IND_Y()));
            break;

        // ------------Increment and Decrement(加减)------------
            //INC a
        case 0xEE:
            INS_INC_DEC(AM_Abs(),1);
            break;
            //INC a,x
        case 0xFE:
            INS_INC_DEC(AM_Abs_XY(CPU.X),1);
            break;
            //INC zp
        case 0xE6:
            INS_INC_DEC(AM_ZP(),1);
            break;
            //INC zp,x
        case 0xF6:
            INS_INC_DEC(AM_ZP_XY(CPU.X),1);
            break;
            //INX #
        case 0xE8:
            INS_INC_DEC_XY(&CPU.X,1);
            break;
            //INY #
        case 0xC8:
            INS_INC_DEC_XY(&CPU.Y,1);
            break;

            //DEC a
        case 0xCE:
            INS_INC_DEC(AM_Abs(),-1);
            break;
            //DEC a,x
        case 0xDE:
            INS_INC_DEC(AM_Abs_XY(CPU.X),-1);
            break;
            //DEC zp
        case 0xC6:
            INS_INC_DEC(AM_ZP(),-1);
            break;
            //DEC zp,x
        case 0xD6:
            INS_INC_DEC(AM_ZP_XY(CPU.X),-1);
            break;
            //DEX #
        case 0xCA:
            INS_INC_DEC_XY(&CPU.X,-1);
            break;
            //DEY #
        case 0x88:
            INS_INC_DEC_XY(&CPU.Y,-1);
            break;

        // ------------Shift and Rotate(位运算与位翻转)------------
            //ASL a
        case 0x0E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;
            //ASL a,x
        case 0x1E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;
            //ASL A
        case 0x0A:
            CPU.A = INS_ASL(CPU.A);
            break;
            //ASL zp
        case 0x06:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;
            //ASL zp,x
        case 0x16:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ASL(CPU_Read_Addr(addr)));
            break;

            //LSR a
        case 0x4E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;
            //LSR a,x
        case 0x5E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;
            //LSR A 寄存器
        case 0x4A:
            CPU.A = INS_LSR(CPU.A);
            break;
            //LSR zp
        case 0x46:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;
            //LSR zp,x
        case 0x56:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_LSR(CPU_Read_Addr(addr)));
            break;

            //ROL a
        case 0x2E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;
            //ROL a,x
        case 0x3E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;
            //ROL A 寄存器
        case 0x2A:
            CPU.A = INS_ROL(CPU.A);
            break;
            //ROL zp
        case 0x26:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;
            //ROL zp,x
        case 0x36:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROL(CPU_Read_Addr(addr)));
            break;

            //ROR a
        case 0x6E:
            addr = AM_Abs();
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;
            //ROR a,x
        case 0x7E:
            addr = AM_Abs_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;
            //ROR A 寄存器
        case 0x6A:
            CPU.A = INS_ROR(CPU.A);
            break;
            //ROR zp
        case 0x66:
            addr = AM_ZP();
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;
            //ROR zp,x
        case 0x76:
            addr = AM_ZP_XY(CPU.X);
            CPU_Write_Addr(addr,INS_ROR(CPU_Read_Addr(addr)));
            break;


    }
}

int main() {
    CPU_Reset();

    CPU.Mem[CPU.PC]   = 0xA9;
    CPU.Mem[CPU.PC+1] = 0x01;

    CPU.Mem[CPU.PC+2] = 0x8D;
    CPU.Mem[CPU.PC+3] = 0x00;
    CPU.Mem[CPU.PC+4] = 0x20;

    CPU.Mem[CPU.PC+5] = 0xA9;
    CPU.Mem[CPU.PC+6] = 0xFF;

    CPU.Mem[CPU.PC+7] = 0xED;
    CPU.Mem[CPU.PC+8] = 0x00;
    CPU.Mem[CPU.PC+9] = 0x20;

    CPU_Exec();
    CPU_Exec();
    CPU_Exec();
    CPU_Exec();
    assert(CPU.F_C == 1);
    assert(CPU.F_V == 0);
    printf("SUCCESS!\n");
    return 0;
}