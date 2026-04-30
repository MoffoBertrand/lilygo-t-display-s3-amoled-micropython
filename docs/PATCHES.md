# Patches requis pour la compilation

## Patch 1 — machine_hw_spi.c (OBLIGATOIRE)

**Fichier :** `lib/micropython/ports/esp32/machine_hw_spi.c`

Ce patch corrige le crash lors de l'initialisation du bus QSPI (host=2) quand les pins quad sont `None` dans la vérification.

### Application

```python
python3 << 'PYEOF'
f = 'lib/micropython/ports/esp32/machine_hw_spi.c'
content = open(f).read()

old = """    } else {
        if ((int)mp_obj_get_int(self->data0) != data0) reconfigure = true;
        if ((int)mp_obj_get_int(self->data1) != data1) reconfigure = true;
        if ((int)mp_obj_get_int(self->sck) != sck) reconfigure = true;
        if ((int)mp_obj_get_int(self->data2) != data2) reconfigure = true;
        if ((int)mp_obj_get_int(self->data3) != data3) reconfigure = true;"""

new = """    } else {
        #define SAFE_GET_INT(obj) ((obj) == mp_const_none ? -1 : (int)mp_obj_get_int(obj))
        if (SAFE_GET_INT(self->data0) != data0) reconfigure = true;
        if (SAFE_GET_INT(self->data1) != data1) reconfigure = true;
        if (SAFE_GET_INT(self->sck)   != sck)   reconfigure = true;
        if (SAFE_GET_INT(self->data2) != data2) reconfigure = true;
        if (SAFE_GET_INT(self->data3) != data3) reconfigure = true;"""

old2 = """        case ESP_ERR_INVALID_STATE:
            mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("SPI(%d) sanity check"), bus->host);
            return;"""

new2 = """        case ESP_ERR_INVALID_STATE:
            break;  // Bus already initialized — treat as success"""

if old in content:  content = content.replace(old, new);  print("✓ SAFE_GET_INT")
if old2 in content: content = content.replace(old2, new2); print("✓ INVALID_STATE")
open(f, 'w').write(content)
PYEOF
```

### Vérification

```bash
grep "SAFE_GET_INT" lib/micropython/ports/esp32/machine_hw_spi.c
# Doit afficher la définition du macro
```

---

## Compilation complète

```bash
cd lvgl_micropython

# 1. Appliquer le patch ci-dessus

# 2. Copier le module amoled_qspi
mkdir -p ext_mod/amoled_qspi
cp <ce_repo>/ext_mod/amoled_qspi/* ext_mod/amoled_qspi/

# 3. Compiler
python3 make.py esp32 \
    BOARD=ESP32_GENERIC_S3 \
    BOARD_VARIANT=SPIRAM_OCT \
    --flash-size=16 \
    USER_C_MODULES=$(pwd)/ext_mod/amoled_qspi/micropython.cmake

# 4. Flasher (adapter le port)
python3 -m esptool --chip esp32s3 \
    -p /dev/cu.usbmodem1101 -b 460800 \
    --before default_reset --after hard_reset \
    write_flash --flash_mode dio --flash_size 16MB \
    --flash_freq 80m --erase-all 0x0 \
    build/lvgl_micropy_ESP32_GENERIC_S3-SPIRAM_OCT-16.bin
```
