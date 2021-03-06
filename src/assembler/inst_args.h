/* Copyright (C) 2014-2015 Ben Kurtovic <ben.kurtovic@gmail.com>
   Released under the terms of the MIT License. See LICENSE for details. */

#pragma once

#include <stdint.h>

#define MAX_SYMBOL_SIZE 256

typedef enum {
    AT_NONE      = 0x00,
    AT_OPTIONAL  = 0x01,
    AT_REGISTER  = 0x02,
    AT_IMMEDIATE = 0x04,
    AT_INDIRECT  = 0x08,
    AT_INDEXED   = 0x10,
    AT_CONDITION = 0x20,
    AT_PORT      = 0x40
} ASMArgType;

typedef enum {
    REG_A, REG_F, REG_B, REG_C, REG_D, REG_E, REG_H, REG_L, REG_I, REG_R,
    REG_AF, REG_BC, REG_DE, REG_HL, REG_IX, REG_IY,
    REG_PC, REG_SP,
    REG_AF_, REG_IXH, REG_IXL, REG_IYH, REG_IYL
} ASMArgRegister;

typedef enum {
    IMM_U16 = 0x01,  // unsigned 16-bit: [0, 65535]
    IMM_U8  = 0x02,  // unsigned 8-bit: [0, 255]
    IMM_S8  = 0x04,  // signed 8-bit: [-128, 127]
    IMM_REL = 0x08,  // relative offset: [-126, 129]
    IMM_BIT = 0x10,  // bit index: [0, 7]
    IMM_RST = 0x20,  // RST page 0 addr: {0x00, 0x08, 0x10, 0x18, ..., 0x38}
    IMM_IM  = 0x40   // interrupt mode: [0, 2]
} ASMArgImmType;

typedef struct {
    ASMArgImmType mask;
    bool is_label;
    uint16_t uval;
    int16_t sval;
    char label[MAX_SYMBOL_SIZE];
} ASMArgImmediate;

typedef struct {
    ASMArgType type;
    union {
        ASMArgRegister reg;
        ASMArgImmediate imm;
    } addr;
} ASMArgIndirect;

typedef struct {
    ASMArgRegister reg;
    int8_t offset;
} ASMArgIndexed;

typedef enum {
    COND_NZ, COND_Z, COND_NC, COND_C, COND_PO, COND_PE, COND_P, COND_M
} ASMArgCondition;

typedef struct {
    ASMArgType type;
    union {
        ASMArgRegister reg;
        ASMArgImmediate imm;
    } port;
} ASMArgPort;

typedef struct {
    ASMArgType type;
    union {
        ASMArgRegister reg;
        ASMArgImmediate imm;
        ASMArgIndirect indirect;
        ASMArgIndexed index;
        ASMArgCondition cond;
        ASMArgPort port;
    } data;
} ASMInstArg;
