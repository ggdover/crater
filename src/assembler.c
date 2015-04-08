/* Copyright (C) 2014-2015 Ben Kurtovic <ben.kurtovic@gmail.com>
   Released under the terms of the MIT License. See LICENSE for details. */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "assembler.h"
#include "logging.h"
#include "util.h"

#define DEFAULT_HEADER_OFFSET 0x7FF0
#define DEFAULT_REGION "GG Export"

#define SYMBOL_TABLE_BUCKETS 128

/* Internal structs */

struct ASMLine {
    char *data;
    size_t length;
    const Line *original;
    const char *filename;
    struct ASMLine *next;
};
typedef struct ASMLine ASMLine;

struct ASMInclude {
    LineBuffer *lines;
    struct ASMInclude *next;
};
typedef struct ASMInclude ASMInclude;

struct ASMInstruction {
    size_t offset;
    uint8_t length;
    uint8_t b1, b2, b3, b4;
    uint8_t virtual_byte;
    char *symbol;
    struct ASMInstruction *next;
};
typedef struct ASMInstruction ASMInstruction;

struct ASMSymbol {
    size_t offset;
    char *symbol;
    struct ASMSymbol *next;
};
typedef struct ASMSymbol ASMSymbol;

typedef struct {
    ASMSymbol *buckets[SYMBOL_TABLE_BUCKETS];
} ASMSymbolTable;

typedef struct {
    size_t offset;
    bool checksum;
    uint32_t product_code;
    uint8_t version;
    uint8_t region;
    uint8_t rom_size;
} ASMHeaderInfo;

typedef struct {
    ASMHeaderInfo header;
    bool optimizer;
    size_t rom_size;
    ASMLine *lines;
    ASMInclude *includes;
    ASMInstruction *instructions;
    ASMSymbolTable *symtable;
} AssemblerState;

/*
    Deallocate a LineBuffer previously created with read_source_file().
*/
static void free_line_buffer(LineBuffer *buffer)
{
    Line *line = buffer->lines, *temp;
    while (line) {
        temp = line->next;
        free(line->data);
        free(line);
        line = temp;
    }

    free(buffer->filename);
    free(buffer);
}

/*
    Read the contents of the source file at the given path into a line buffer.

    Return the buffer if reading was successful; it must be freed with
    free_line_buffer() when done. Return NULL if an error occurred while
    reading. A message will be printed to stderr in this case.
*/
static LineBuffer* read_source_file(const char *path)
{
    FILE *fp;
    struct stat st;

    if (!(fp = fopen(path, "r"))) {
        ERROR_ERRNO("couldn't open source file")
        return NULL;
    }

    if (fstat(fileno(fp), &st)) {
        fclose(fp);
        ERROR_ERRNO("couldn't open source file")
        return NULL;
    }
    if (!(st.st_mode & S_IFREG)) {
        fclose(fp);
        ERROR("couldn't open source file: %s", st.st_mode & S_IFDIR ?
              "Is a directory" : "Is not a regular file")
        return NULL;
    }

    LineBuffer *source = malloc(sizeof(LineBuffer));
    if (!source)
        OUT_OF_MEMORY()

    source->lines = NULL;
    source->filename = malloc(sizeof(char) * (strlen(path) + 1));
    if (!source->filename)
        OUT_OF_MEMORY()
    strcpy(source->filename, path);

    Line dummy = {.next = NULL};
    Line *line, *prev = &dummy;
    size_t lineno = 1;

    while (1) {
        char *data = NULL;
        size_t cap = 0;
        ssize_t len;

        if ((len = getline(&data, &cap, fp)) < 0) {
            if (feof(fp))
                break;
            if (errno == ENOMEM)
                OUT_OF_MEMORY()
            ERROR_ERRNO("couldn't read source file")
            fclose(fp);
            source->lines = dummy.next;
            free_line_buffer(source);
            return NULL;
        }

        line = malloc(sizeof(Line));
        if (!line)
            OUT_OF_MEMORY()

        line->data = data;
        line->length = feof(fp) ? len : (len - 1);
        line->lineno = lineno++;
        line->next = NULL;

        prev->next = line;
        prev = line;
    }

    fclose(fp);
    source->lines = dummy.next;
    return source;
}

/*
    Write an assembled binary file to the given path.

    Return whether the file was written successfully. On error, a message is
    printed to stderr.
*/
static bool write_binary_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp;
    if (!(fp = fopen(path, "wb"))) {
        ERROR_ERRNO("couldn't open destination file")
        return false;
    }

    if (!fwrite(data, size, 1, fp)) {
        fclose(fp);
        ERROR_ERRNO("couldn't write to destination file")
        return false;
    }

    fclose(fp);
    return true;
}

/*
    Print an ErrorInfo object returned by assemble() to the given stream.
*/
void error_info_print(const ErrorInfo *error_info, FILE *file)
{
    // TODO
    fprintf(file, "Error: Unknown error\n");
}

/*
    Destroy an ErrorInfo object created by assemble().
*/
void error_info_destroy(ErrorInfo *error_info)
{
    if (!error_info)
        return;

    // TODO
    free(error_info);
}

/*
    Initialize default values in an AssemblerState object.
*/
static void init_state(AssemblerState *state)
{
    state->header.offset = DEFAULT_HEADER_OFFSET;
    state->header.checksum = true;
    state->header.product_code = 0;
    state->header.version = 0;
    state->header.region = region_string_to_code(DEFAULT_REGION);
    state->header.rom_size = 0;
    state->optimizer = false;
    state->rom_size = 0;

    state->lines = NULL;
    state->includes = NULL;
    state->instructions = NULL;
    state->symtable = NULL;
}

/*
    Deallocate an ASMLine list.
*/
static void free_asm_lines(ASMLine *line)
{
    while (line) {
        ASMLine *temp = line->next;
        free(line->data);
        free(line);
        line = temp;
    }
}

/*
    Deallocate an ASMInclude list.
*/
static void free_asm_includes(ASMInclude *include)
{
    while (include) {
        ASMInclude *temp = include->next;
        free_line_buffer(include->lines);
        free(include);
        include = temp;
    }
}

/*
    Deallocate an ASMInstruction list.
*/
static void free_asm_instructions(ASMInstruction *inst)
{
    while (inst) {
        ASMInstruction *temp = inst->next;
        if (inst->symbol)
            free(inst->symbol);
        free(inst);
        inst = temp;
    }
}

/*
    Deallocate an ASMSymbolTable.
*/
static void free_asm_symtable(ASMSymbolTable *symtable)
{
    if (!symtable)
        return;

    for (size_t bucket = 0; bucket < SYMBOL_TABLE_BUCKETS; bucket++) {
        ASMSymbol *sym = symtable->buckets[bucket], *temp;
        while (sym) {
            temp = sym->next;
            free(sym->symbol);
            free(sym);
            sym = temp;
        }
    }
    free(symtable);
}

/*
    Preprocess a single source line (source, length) into a normalized ASMLine.

    *Only* the data and length fields in the ASMLine object are populated. The
    normalization process converts tabs to spaces, removes runs of multiple
    spaces (outside of string literals), strips comments, and other things.

    Return NULL if an ASM line was not generated from the source, i.e. if it is
    blank after being stripped.
*/
static ASMLine* normalize_line(const char *source, size_t length)
{
    char *data = malloc(sizeof(char) * length);
    if (!data)
        OUT_OF_MEMORY()

    size_t si, di, slashes = 0;
    bool has_content = false, space_pending = false, in_string = false;
    for (si = di = 0; si < length; si++) {
        char c = source[si];

        if (c == '\\')
            slashes++;
        else
            slashes = 0;

        if (in_string) {
            if (c == '"' && (slashes % 2) == 0)
                in_string = false;

            data[di++] = c;
        } else {
            if (c == ';')
                break;
            if (c == '"' && (slashes % 2) == 0)
                in_string = true;

            if (c == '\t' || c == ' ')
                space_pending = true;
            else {
                if (space_pending) {
                    if (has_content)
                        data[di++] = ' ';
                    space_pending = false;
                }
                has_content = true;
                data[di++] = c;
            }
        }
    }

    if (!has_content) {
        free(data);
        return NULL;
    }

    ASMLine *line = malloc(sizeof(ASMLine));
    if (!line)
        OUT_OF_MEMORY()

    data = realloc(data, sizeof(char) * di);
    if (!data)
        OUT_OF_MEMORY()

    line->data = data;
    line->length = di;
    return line;
}

/*
    Preprocess the LineBuffer into ASMLines. Change some state along the way.

    This function processes include directives, so read_source_file() may be
    called multiple times (along with the implications that has), and
    state->includes may be modified.

    On success, state->lines is modified and NULL is returned. On error, an
    ErrorInfo object is returned, and state->lines and state->includes are not
    modified.
*/
static ErrorInfo* preprocess(AssemblerState *state, const LineBuffer *source)
{
    // TODO

    // state->header.offset             <-- check in list of acceptable values
    // state->header.checksum           <-- boolean check
    // state->header.product_code       <-- range check
    // state->header.version            <-- range check
    // state->header.region             <-- string conversion, check
    // state->header.rom_size           <-- value/range check
    // state->optimizer                 <-- boolean check
    // state->rom_size                  <-- value check

    // if giving rom size, check header offset is in rom size range
    // if giving reported and actual rom size, check reported is <= actual
    // ensure no duplicate explicit assignments

    ASMLine dummy = {.next = NULL};
    ASMLine *line, *prev = &dummy;
    const Line *orig = source->lines;

    while (orig) {
        if ((line = normalize_line(orig->data, orig->length))) {
            line->original = orig;
            line->filename = source->filename;
            line->next = NULL;

            prev->next = line;
            prev = line;
        }
        orig = orig->next;
    }

    state->lines = dummy.next;

#ifdef DEBUG_MODE
    DEBUG("Dumping ASMLines:")
    const ASMLine *temp = state->lines;
    while (temp) {
        DEBUG("- %-40.*s [%s:%02zu]", (int) temp->length, temp->data,
              temp->filename, temp->original->lineno)
        temp = temp->next;
    }
#endif

    return NULL;
}

/*
    Tokenize ASMLines into ASMInstructions.

    On success, state->instructions is modified and NULL is returned. On error,
    an ErrorInfo object is returned and state->instructions is not modified.
    state->symtable may or may not be modified regardless of success.
*/
static ErrorInfo* tokenize(AssemblerState *state)
{
    // TODO

    // verify no instructions clash with header offset
    // if rom size is set, verify nothing overflows

    return NULL;
}

/*
    Resolve default placeholder values in assembler state, such as ROM size.

    On success, no new heap objects are allocated. On error, an ErrorInfo
    object is returned.
*/
static ErrorInfo* resolve_defaults(AssemblerState *state)
{
    // TODO

    // if (!state.rom_size)
            // set to max possible >= 32 KB, or error if too many instructions
            // if (state.header.rom_size)
                    // check reported rom size is <= actual rom size

    // if (!state.header.rom_size)
            // set to actual rom size

    return NULL;
}

/*
    Resolve symbol placeholders in instructions such as jumps and branches.

    On success, no new heap objects are allocated. On error, an ErrorInfo
    object is returned.
*/
static ErrorInfo* resolve_symbols(AssemblerState *state)
{
    // TODO

    return NULL;
}

/*
    Convert finalized ASMInstructions into a binary data block.

    This function should never fail.
*/
static void serialize_binary(AssemblerState *state, uint8_t *binary)
{
    // TODO

    for (size_t i = 0; i < state->rom_size; i++)
        binary[i] = 'X';
}

/*
    Assemble the z80 source code in the source code buffer into binary data.

    If successful, return the size of the assembled binary data and change
    *binary_ptr to point to the assembled ROM data buffer. *binary_ptr must be
    free()'d when finished.

    If an error occurred, return 0 and update *ei_ptr to point to an ErrorInfo
    object which can be shown to the user with error_info_print(). The
    ErrorInfo object must be destroyed with error_info_destroy() when finished.

    In either case, only one of *binary_ptr and *ei_ptr is modified.
*/
size_t assemble(const LineBuffer *source, uint8_t **binary_ptr, ErrorInfo **ei_ptr)
{
    AssemblerState state;
    ErrorInfo *error_info;
    size_t retval = 0;

    init_state(&state);

    if ((error_info = preprocess(&state, source)))
        goto error;

    if (!(state.symtable = malloc(sizeof(ASMSymbolTable))))
        OUT_OF_MEMORY()
    for (size_t bucket = 0; bucket < SYMBOL_TABLE_BUCKETS; bucket++)
        state.symtable->buckets[bucket] = NULL;

    if ((error_info = tokenize(&state)))
        goto error;

    if ((error_info = resolve_defaults(&state)))
        goto error;

    if ((error_info = resolve_symbols(&state)))
        goto error;

    uint8_t *binary = malloc(sizeof(uint8_t) * state.rom_size);
    if (!binary)
        OUT_OF_MEMORY()

    serialize_binary(&state, binary);
    *binary_ptr = binary;
    retval = state.rom_size;
    goto cleanup;

    error:
    *ei_ptr = error_info;

    cleanup:
    free_asm_lines(state.lines);
    free_asm_includes(state.includes);
    free_asm_instructions(state.instructions);
    free_asm_symtable(state.symtable);
    return retval;
}

/*
    Assemble the z80 source code at the input path into a binary file.

    Return true if the operation was a success and false if it was a failure.
    Errors are printed to STDOUT; if the operation was successful then nothing
    is printed.
*/
bool assemble_file(const char *src_path, const char *dst_path)
{
    LineBuffer *source = read_source_file(src_path);
    if (!source)
        return false;

    uint8_t *binary;
    ErrorInfo *error_info;
    size_t size = assemble(source, &binary, &error_info);
    free_line_buffer(source);

    if (!size) {
        error_info_print(error_info, stderr);
        error_info_destroy(error_info);
        return false;
    }

    bool success = write_binary_file(dst_path, binary, size);
    free(binary);
    return success;
}
