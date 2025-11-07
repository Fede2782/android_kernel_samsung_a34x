/*
 * leds-sm5714b-fled.c - SM5714B Flash-LEDs device driver
 *
 * Copyright (C) 2024 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
int set_sm5714b_rear_flash(const char *buf, size_t count);
int get_sm5714b_rear_flash(char *buf);
void sm5714b_fled_set_afc_voltage_mode(bool mode);
