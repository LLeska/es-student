#include "stdio-text-protocol/stdio-text-protocol.h"
#include <stddef.h>
#include <stdio.h>
#include "pico/stdlib.h"

static char line[STDIO_TEXT_PROTOCOL_LINE_SIZE];
static uint32_t length;
static bool overflow;
static command_t command;

void stdio_text_protocol_init(void)
{
    length = 0;
    overflow = false;
    command = (command_t){0};
}

static const command_t *split_line(void)
{
    command = (command_t){.truncated = overflow};
    char *cursor = line;
    while (*cursor != '\0') {
        while (*cursor == ' ') ++cursor;
        if (*cursor == '\0') break;
        char *word = cursor;
        while (*cursor != '\0' && *cursor != ' ') ++cursor;
        if (*cursor != '\0') *cursor++ = '\0';
        if (command.name == NULL) {
            command.name = word;
        } else if (command.argc < STDIO_TEXT_PROTOCOL_MAX_ARGS) {
            command.argv[command.argc++] = word;
        } else {
            command.truncated = true;
        }
    }
    return command.name != NULL || command.truncated ? &command : NULL;
}

const command_t *stdio_text_protocol_handle(void)
{
    int symbol = getchar_timeout_us(0);
    if (symbol == PICO_ERROR_TIMEOUT) return NULL;
    if (symbol == '\r' || symbol == '\n') {
        if (length == 0 && !overflow) return NULL;
        putchar('\n');
        line[length] = '\0';
        const command_t *result = split_line();
        length = 0;
        overflow = false;
        return result;
    }
    if (symbol == 0x08 || symbol == 0x7f) {
        if (length != 0) {
            --length;
            overflow = false;
            fputs("\b \b", stdout);
        }
        return NULL;
    }
    if (symbol >= 0x20 && symbol <= 0x7e) {
        if (length + 1 < STDIO_TEXT_PROTOCOL_LINE_SIZE) {
            line[length++] = (char)symbol;
            putchar(symbol);
        } else {
            overflow = true;
        }
    }
    return NULL;
}
