/* Copyright (C) 2014-2015 Ben Kurtovic <ben.kurtovic@gmail.com>
   Released under the terms of the MIT License. See LICENSE for details. */

#include <stdlib.h>

#include "errors.h"
#include "state.h"
#include "../assembler.h"
#include "../logging.h"

#define ERROR_TYPE(err_info) (asm_error_types[err_info->type])
#define ERROR_DESC(err_info) (asm_error_descs[err_info->desc])

/* Internal structs */

struct ASMErrorLine {
    char *data;
    size_t length;
    size_t lineno;
    char *filename;
    struct ASMErrorLine *next;
};
typedef struct ASMErrorLine ASMErrorLine;

struct ErrorInfo {
    ASMErrorType type;
    ASMErrorDesc desc;
    ASMErrorLine *line;
};

/*
    Create an ASMErrorLine object from an ASMLine.
*/
static ASMErrorLine* create_error_line(const ASMLine *line)
{
    ASMErrorLine *el = malloc(sizeof(ASMErrorLine));
    if (!el)
        OUT_OF_MEMORY()

    const char *source = line->original->data;
    size_t length = line->original->length;
    if (!(el->data = malloc(sizeof(char) * length)))
        OUT_OF_MEMORY()

    // Ignore spaces at beginning:
    while (length > 0 && (*source == ' ' || *source == '\t'))
        source++, length--;
    memcpy(el->data, source, length);

    el->length = length;
    el->lineno = line->original->lineno;

    el->filename = strdup(line->filename);
    if (!el->filename)
        OUT_OF_MEMORY()

    el->next = NULL;
    return el;
}

/*
    Create an ErrorInfo object describing a particular error.

    The ErrorInfo object can be printed with error_info_print(), and must be
    freed when done with error_info_destroy().

    This function never fails (OOM triggers an exit()); the caller can be
    confident the returned object is valid.
*/
ErrorInfo* error_info_create(
    const ASMLine *line, ASMErrorType err_type, ASMErrorDesc err_desc)
{
    ErrorInfo *einfo = malloc(sizeof(ErrorInfo));
    if (!einfo)
        OUT_OF_MEMORY()

    einfo->type = err_type;
    einfo->desc = err_desc;
    einfo->line = create_error_line(line);
    return einfo;
}

/*
    Add an ASMLine to an ErrorInfo object, as part of a file trace.
*/
void error_info_append(ErrorInfo *einfo, const ASMLine *line)
{
    ASMErrorLine* el = create_error_line(line);
    el->next = einfo->line;
    einfo->line = el;
}

/*
    Print an ErrorInfo object returned by assemble() to the given stream.
*/
void error_info_print(const ErrorInfo *einfo, FILE *file)
{
    ASMErrorLine *line = einfo->line;

    fprintf(file, "error: %s: %s\n", ERROR_TYPE(einfo), ERROR_DESC(einfo));
    while (line) {
        fprintf(file, "%s:%zu:\n", line->filename, line->lineno);
        fprintf(file, "    %.*s\n", (int) line->length, line->data);
        line = line->next;
    }
}

/*
    Destroy an ErrorInfo object created by assemble().
*/
void error_info_destroy(ErrorInfo *error_info)
{
    if (!error_info)
        return;

    ASMErrorLine *line = error_info->line, *temp;
    while (line) {
        temp = line->next;
        free(line->data);
        free(line->filename);
        free(line);
        line = temp;
    }
    free(error_info);
}