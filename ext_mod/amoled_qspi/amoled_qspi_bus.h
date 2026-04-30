#pragma once
#include "py/obj.h"
#include "driver/spi_master.h"

typedef struct {
    mp_obj_base_t    base;
    spi_host_device_t spi_host;
    spi_device_handle_t spi_dev;
    int sck_pin, d0_pin, d1_pin, d2_pin, d3_pin, cs_pin;
    int freq;
} amoled_qspi_obj_t;

extern const mp_obj_type_t amoled_qspi_type;
