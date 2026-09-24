#include "memory.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/regs/addressmap.h"
#include "hardware/gpio.h"
#include "command.h"
#include "device.h"
#include "led.h"

extern char __flash_binary_start;
extern char __flash_binary_end;
extern char __boot2_start__;
extern char __boot2_end__;
extern char __etext;
extern char __data_start__;
extern char __data_end__;
extern char __bss_start__;
extern char __bss_end__;
extern char __HeapLimit;
extern char __StackBottom;
extern char __StackTop;

static void row(const char *name, uintptr_t start, uintptr_t end)
{
    printf("%-10s 0x%08x 0x%08x %8u\n", name, (unsigned)start, (unsigned)end, (unsigned)(end - start));
}

void mem_info(void)
{
    uintptr_t flash_end = XIP_BASE + PICO_FLASH_SIZE_BYTES;
    uintptr_t image_start = (uintptr_t)&__flash_binary_start;
    uintptr_t image_end = (uintptr_t)&__flash_binary_end;
    uintptr_t boot_start = (uintptr_t)&__boot2_start__;
    uintptr_t boot_end = (uintptr_t)&__boot2_end__;
    uintptr_t text_end = (uintptr_t)&__etext;
    uintptr_t data_start = (uintptr_t)&__data_start__;
    uintptr_t data_end = (uintptr_t)&__data_end__;
    uintptr_t bss_start = (uintptr_t)&__bss_start__;
    uintptr_t bss_end = (uintptr_t)&__bss_end__;
    uintptr_t heap_end = (uintptr_t)&__HeapLimit;
    uintptr_t stack_start = (uintptr_t)&__StackBottom;
    uintptr_t stack_end = (uintptr_t)&__StackTop;
    unsigned data_size = data_end - data_start;
    unsigned bss_size = bss_end - bss_start;
    printf("area       start      end        size\n");
    row("flash", XIP_BASE, flash_end);
    row("sram", SRAM_BASE, SRAM_BASE + 264 * 1024);
    row("rom", ROM_BASE, ROM_BASE + 16 * 1024);
    row("image", image_start, image_end);
    row("free", image_end, flash_end);
    row("boot2", boot_start, boot_end);
    row("text", boot_end, text_end);
    row("data flash", text_end, text_end + data_size);
    row("data ram", data_start, data_end);
    row("bss", bss_start, bss_end);
    row("heap", bss_end, heap_end);
    row("stack", stack_start, stack_end);
    printf("\ntotal\n");
    printf("  flash image %8u = boot2 %u + text %u + data %u\n",
           (unsigned)(image_end - image_start), (unsigned)(boot_end - boot_start), (unsigned)(text_end - boot_end), data_size);
    printf("  flash free  %8u of %u\n", (unsigned)(flash_end - image_end), (unsigned)PICO_FLASH_SIZE_BYTES);
    printf("  ram used    %8u = data %u + bss %u\n", data_size + bss_size, data_size, bss_size);
    printf("  ram free    %8u for heap and %u for stack\n", (unsigned)(heap_end - bss_end), (unsigned)(stack_end - stack_start));
}

int main(void);
uint32_t data_variable = 100;
uint32_t bss_variable;

void fw_info(void)
{
    const uint16_t *main_code = (const uint16_t *)((uintptr_t)main & ~1u);
    const uint16_t *fw_code = (const uint16_t *)((uintptr_t)fw_info & ~1u);
    uint32_t stack_variable = 1946;
    uint32_t *heap_variable = malloc(sizeof(uint32_t));
    data_variable++;
    bss_variable++;
    printf("object          address     value\n");
    printf("main            0x%08x 0x%04x\n", (unsigned)(uintptr_t)main, (unsigned)*main_code);
    printf("fw_info         0x%08x 0x%04x\n", (unsigned)(uintptr_t)fw_info, (unsigned)*fw_code);
    printf("commands        0x%08x\n", (unsigned)(uintptr_t)commands);
    for (uint i = 0; i < command_count; i++) {
        printf("- %-13s 0x%08x\n", commands[i].name, (unsigned)(uintptr_t)commands[i].handler);
    }
    printf("DEVICE_PROJECT  0x%08x %s\n", (unsigned)(uintptr_t)DEVICE_PROJECT, DEVICE_PROJECT);
    printf("DEVICE_BOARD    0x%08x %s\n", (unsigned)(uintptr_t)DEVICE_BOARD, DEVICE_BOARD);
    printf("data_variable   0x%08x %u\n", (unsigned)(uintptr_t)&data_variable, (unsigned)data_variable);
    printf("bss_variable    0x%08x %u\n", (unsigned)(uintptr_t)&bss_variable, (unsigned)bss_variable);
    printf("stack_variable  0x%08x %u\n", (unsigned)(uintptr_t)&stack_variable, (unsigned)stack_variable);
    if (heap_variable != NULL) {
        *heap_variable = 1951;
        printf("heap_variable   0x%08x %u\n", (unsigned)(uintptr_t)heap_variable, (unsigned)*heap_variable);
        free(heap_variable);
    } else {
        printf("malloc failed\n");
    }
}

#define VECTOR_TABLE 0x10000100
void boot_info(void)
{
    const uint32_t *vectors = (const uint32_t *)VECTOR_TABLE;
    uint32_t stack_top = vectors[0];
    uint32_t reset_handler = vectors[1];
    volatile uint32_t *gpio_in = (volatile uint32_t *)0xd0000004;
    uint32_t level = (*gpio_in >> led_pin()) & 1u;
    printf("vector table   0x%08x\n", (unsigned)VECTOR_TABLE);
    printf("  stack top    0x%08x\n", (unsigned)stack_top);
    printf("  reset        0x%08x\n", (unsigned)reset_handler);
    printf("  reset (even) 0x%08x\n", (unsigned)(reset_handler & ~1u));
    printf("gpio in        0x%08x\n", (unsigned)(uintptr_t)gpio_in);
    printf("  led bit      %u\n", (unsigned)level);
    printf("  gpio_get     %u\n", (unsigned)gpio_get(led_pin()));
}
