/* Copyright (C) 2014-2015 Ben Kurtovic <ben.kurtovic@gmail.com>
   Released under the terms of the MIT License. See LICENSE for details. */

#pragma once

/* Enums */

typedef enum {
    ET_INCLUDE
} ASMErrorType;

typedef enum {
    ED_BAD_ARG,
    ED_RECURSION,
    ED_FILE_READ_ERR
} ASMErrorDesc;

/* Strings */

static const char *asm_error_types[] = {
    "include directive"
};

static const char *asm_error_descs[] = {
    "missing or invalid argument",
    "infinite recursion detected",
    "couldn't read included file"
};
