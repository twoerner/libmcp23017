/*
 * Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
 * SPDX-License-Identifier: OSL-3.0
 */

#ifndef LIB_MCP23017__H
#define LIB_MCP23017__H

#include <stdbool.h>
#include <stdint.h>

bool mcp23017__init (const char *devFile_p, uint8_t *i2cAddr_p, bool atlRegAddr);
void mcp23017__cleanup (void);
bool mcp23017__set_output_pins (uint8_t portAmask, uint8_t portBmask);
bool mcp23017__set_input_pins (uint8_t portAmask, uint8_t portBmask);
bool mcp23017__write_portA (uint8_t val);
bool mcp23017__write_portB (uint8_t val);
bool mcp23017__get_reg (uint8_t reg, uint8_t *val_p);
bool mcp23017__get_portA (uint8_t *val_p);
bool mcp23017__get_portB (uint8_t *val_p);

#endif
