// amoled_qspi_bus.c — Driver QSPI natif pour RM67162
// tx_color envoie header + données avec CS maintenu bas pendant tout l'envoi

#include "amoled_qspi_bus.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "mphalport.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>

#define SEND_BUF_SIZE  0x4000  // 16kb

static void qspi_tx_param(amoled_qspi_obj_t *self, int reg, const void *data, size_t len)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    t.cmd   = 0x02;
    t.addr  = (uint32_t)reg << 8;
    if (len > 0 && data) {
        t.tx_buffer = data;
        t.length    = 8 * len;
    }
    gpio_set_level(self->cs_pin, 0);
    spi_device_polling_transmit(self->spi_dev, &t);
    gpio_set_level(self->cs_pin, 1);
}

// Envoie header RAMWR — CS reste BAS après
static void qspi_tx_color_start(amoled_qspi_obj_t *self)
{
    spi_transaction_ext_t th;
    memset(&th, 0, sizeof(th));
    th.base.flags = SPI_TRANS_MODE_QIO;
    th.base.cmd   = 0x32;
    th.base.addr  = 0x002C00;
    gpio_set_level(self->cs_pin, 0);
    spi_device_polling_transmit(self->spi_dev, (spi_transaction_t *)&th);
    // CS reste BAS
}

// Envoie données pixel — CS reste BAS
static void qspi_tx_pixels(amoled_qspi_obj_t *self, const void *data, size_t len)
{
    spi_transaction_ext_t td;
    memset(&td, 0, sizeof(td));
    td.base.flags   = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                      SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    td.command_bits = 0;
    td.address_bits = 0;
    td.dummy_bits   = 0;

    uint8_t *p = (uint8_t *)data;
    size_t remain = len;
    while (remain > 0) {
        size_t chunk = remain > SEND_BUF_SIZE ? SEND_BUF_SIZE : remain;
        td.base.tx_buffer = p;
        td.base.length    = chunk * 8;
        spi_device_polling_transmit(self->spi_dev, (spi_transaction_t *)&td);
        p      += chunk;
        remain -= chunk;
    }
    // CS reste BAS
}

// Termine la transaction — lève CS
static void qspi_tx_color_end(amoled_qspi_obj_t *self)
{
    gpio_set_level(self->cs_pin, 1);
}

// tx_color : header + données, CS reste BAS à la fin
static void qspi_tx_color(amoled_qspi_obj_t *self, const void *data, size_t len)
{
    qspi_tx_color_start(self);
    qspi_tx_pixels(self, data, len);
    // CS reste BAS — caller doit appeler tx_color_end
}

static void qspi_init_bus(amoled_qspi_obj_t *self)
{
    gpio_config_t cs_conf = {
        .pin_bit_mask = 1ULL << self->cs_pin,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cs_conf);
    gpio_set_level(self->cs_pin, 1);

    spi_bus_config_t buscfg = {
        .data0_io_num = self->d0_pin,
        .data1_io_num = self->d1_pin,
        .sclk_io_num  = self->sck_pin,
        .data2_io_num = self->d2_pin,
        .data3_io_num = self->d3_pin,
        .max_transfer_sz = (SEND_BUF_SIZE * 16) + 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS |
                 SPICOMMON_BUSFLAG_QUAD,
    };

    esp_err_t ret = spi_bus_initialize(self->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        mp_raise_msg_varg(&mp_type_OSError,
            MP_ERROR_TEXT("%d(spi_bus_initialize)"), ret);
    }

    spi_device_interface_config_t devcfg = {
        .command_bits  = 8,
        .address_bits  = 24,
        .mode          = 0,
        .clock_speed_hz = self->freq,
        .spics_io_num  = -1,
        .flags         = SPI_DEVICE_HALFDUPLEX,
        .queue_size    = 10,
    };

    ret = spi_bus_add_device(self->spi_host, &devcfg, &self->spi_dev);
    if (ret != ESP_OK) {
        mp_raise_msg_varg(&mp_type_OSError,
            MP_ERROR_TEXT("%d(spi_bus_add_device)"), ret);
    }
}

// ── MicroPython API ───────────────────────────────────────────

static mp_obj_t amoled_qspi_make_new(const mp_obj_type_t *type,
    size_t n_args, size_t n_kw, const mp_obj_t *all_args)
{
    enum { ARG_host, ARG_sck, ARG_d0, ARG_d1, ARG_d2, ARG_d3, ARG_cs, ARG_freq };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_host, MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 2} },
        { MP_QSTR_sck,  MP_ARG_INT | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_d0,   MP_ARG_INT | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_d1,   MP_ARG_INT | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_d2,   MP_ARG_INT | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_d3,   MP_ARG_INT | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_cs,   MP_ARG_INT | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_freq, MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 80000000} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args,
        MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    amoled_qspi_obj_t *self = mp_obj_malloc(amoled_qspi_obj_t, type);
    self->spi_host = (spi_host_device_t)(args[ARG_host].u_int - 1);
    self->sck_pin  = args[ARG_sck].u_int;
    self->d0_pin   = args[ARG_d0].u_int;
    self->d1_pin   = args[ARG_d1].u_int;
    self->d2_pin   = args[ARG_d2].u_int;
    self->d3_pin   = args[ARG_d3].u_int;
    self->cs_pin   = args[ARG_cs].u_int;
    self->freq     = args[ARG_freq].u_int;

    qspi_init_bus(self);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t amoled_qspi_tx_param(size_t n_args, const mp_obj_t *args)
{
    amoled_qspi_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    int reg = mp_obj_get_int(args[1]);
    if (n_args >= 3) {
        mp_buffer_info_t buf;
        mp_get_buffer_raise(args[2], &buf, MP_BUFFER_READ);
        qspi_tx_param(self, reg, buf.buf, buf.len);
    } else {
        qspi_tx_param(self, reg, NULL, 0);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(amoled_qspi_tx_param_obj, 2, 3, amoled_qspi_tx_param);

// tx_color: header + data, laisse CS bas
static mp_obj_t amoled_qspi_tx_color(size_t n_args, const mp_obj_t *args)
{
    amoled_qspi_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (n_args >= 3) {
        mp_buffer_info_t buf;
        mp_get_buffer_raise(args[2], &buf, MP_BUFFER_READ);
        qspi_tx_color(self, buf.buf, buf.len);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(amoled_qspi_tx_color_obj, 2, 3, amoled_qspi_tx_color);

// tx_pixels: données seulement, CS reste bas (suite de tx_color)
static mp_obj_t amoled_qspi_tx_pixels(size_t n_args, const mp_obj_t *args)
{
    amoled_qspi_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (n_args >= 3) {
        mp_buffer_info_t buf;
        mp_get_buffer_raise(args[2], &buf, MP_BUFFER_READ);
        qspi_tx_pixels(self, buf.buf, buf.len);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(amoled_qspi_tx_pixels_obj, 2, 3, amoled_qspi_tx_pixels);

// tx_end: lève CS
static mp_obj_t amoled_qspi_tx_end(mp_obj_t self_in)
{
    amoled_qspi_obj_t *self = MP_OBJ_TO_PTR(self_in);
    qspi_tx_color_end(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(amoled_qspi_tx_end_obj, amoled_qspi_tx_end);

static const mp_rom_map_elem_t amoled_qspi_locals[] = {
    { MP_ROM_QSTR(MP_QSTR_tx_param),  MP_ROM_PTR(&amoled_qspi_tx_param_obj)  },
    { MP_ROM_QSTR(MP_QSTR_tx_color),  MP_ROM_PTR(&amoled_qspi_tx_color_obj)  },
    { MP_ROM_QSTR(MP_QSTR_tx_pixels), MP_ROM_PTR(&amoled_qspi_tx_pixels_obj) },
    { MP_ROM_QSTR(MP_QSTR_tx_end),    MP_ROM_PTR(&amoled_qspi_tx_end_obj)    },
};
static MP_DEFINE_CONST_DICT(amoled_qspi_locals_dict, amoled_qspi_locals);

MP_DEFINE_CONST_OBJ_TYPE(
    amoled_qspi_type,
    MP_QSTR_QSPIBus,
    MP_TYPE_FLAG_NONE,
    make_new, amoled_qspi_make_new,
    locals_dict, &amoled_qspi_locals_dict
);

static const mp_rom_map_elem_t amoled_qspi_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_amoled_qspi) },
    { MP_ROM_QSTR(MP_QSTR_QSPIBus),  MP_ROM_PTR(&amoled_qspi_type)    },
};
static MP_DEFINE_CONST_DICT(amoled_qspi_module_globals, amoled_qspi_module_globals_table);

const mp_obj_module_t amoled_qspi_module = {
    .base    = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&amoled_qspi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_amoled_qspi, amoled_qspi_module);
