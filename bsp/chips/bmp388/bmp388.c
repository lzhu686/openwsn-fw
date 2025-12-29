/**
\brief BMP388 driver implementation.

This driver mirrors the structure of the BMX160 example and follows the
initialization and compensation steps from the Bosch BMP388 datasheet.
*/

#include <math.h>
#include <string.h>

#include "board.h"
#include "i2c.h"
#include "bmp388.h"

#ifndef BMP388_SEA_LEVEL_PA
#define BMP388_SEA_LEVEL_PA 101325.0f
#endif

//=========================== defines ========================================

#define BMP388_STATUS_CMD_RDY      (1u << 4)
#define BMP388_STATUS_DATA_RDY     (1u << 7)

#define BMP388_WAIT_CYCLES         10000u

#define BMP388_PRESS_BYTES         3u
#define BMP388_TEMP_BYTES          3u

//=========================== variables =======================================

typedef struct {
    bmp388_calibration_t calib;
    float pressure_pa;
    float temperature_c;
    bool calibrated;
} bmp388_vars_t;

static bmp388_vars_t bmp388_vars;

//=========================== prototypes =====================================

static void bmp388_read_calibration(void);
static double bmp388_compensate_temperature(int32_t uncomp_temp);
static double bmp388_compensate_pressure(int32_t uncomp_press);
static void bmp388_delay_cycles(uint32_t cycles);

//=========================== public =========================================

bool bmp388_init(void) {
    memset(&bmp388_vars, 0, sizeof(bmp388_vars));

    i2c_set_addr(BMP388_I2C_ADDR);

    bmp388_soft_reset();

    bmp388_read_calibration();

    bmp388_configure(/*osr_pressure=*/0x03u, /*osr_temperature=*/0x02u,
                     /*odr=*/0x00u, /*iir_coeff=*/BMP388_IIR_COEFF_7);

    bmp388_vars.calibrated = true;

    return (bmp388_who_am_i() == 0x50u);
}

void bmp388_soft_reset(void) {
    uint8_t cmd = BMP388_CMD_SOFT_RESET;
    i2c_write_bytes(BMP388_REG_CMD, &cmd, 1);
    bmp388_delay_cycles(BMP388_WAIT_CYCLES);

    // ensure command interface ready
    uint8_t status = 0;
    do {
        i2c_read_bytes(BMP388_REG_STATUS, &status, 1);
    } while ((status & BMP388_STATUS_CMD_RDY) == 0);
}

uint8_t bmp388_who_am_i(void) {
    uint8_t chip_id = 0u;
    i2c_read_bytes(BMP388_REG_CHIP_ID, &chip_id, 1);
    return chip_id;
}

void bmp388_configure(uint8_t osr_pressure, uint8_t osr_temperature, uint8_t odr, uint8_t iir_coeff) {
    uint8_t osr_value = (uint8_t)(BMP388_OSR_PRESS(osr_pressure) | BMP388_OSR_TEMP(osr_temperature));
    uint8_t config_value = (uint8_t)(iir_coeff & 0x07u);
    uint8_t pwr_ctrl = (uint8_t)(BMP388_PWR_CTRL_PRESS_EN | BMP388_PWR_CTRL_TEMP_EN | BMP388_PWR_CTRL_MODE_NORMAL);

    i2c_write_bytes(BMP388_REG_OSR, &osr_value, 1);
    i2c_write_bytes(BMP388_REG_ODR, &odr, 1);
    i2c_write_bytes(BMP388_REG_CONFIG, &config_value, 1);
    i2c_write_bytes(BMP388_REG_PWR_CTRL, &pwr_ctrl, 1);
}

void bmp388_trigger_forced_measurement(void) {
    uint8_t pwr_ctrl = (uint8_t)(BMP388_PWR_CTRL_PRESS_EN | BMP388_PWR_CTRL_TEMP_EN | BMP388_PWR_CTRL_MODE_FORCED);
    i2c_write_bytes(BMP388_REG_PWR_CTRL, &pwr_ctrl, 1);
}

void bmp388_read_uncompensated(uint32_t *pressure_raw, uint32_t *temperature_raw) {
    uint8_t buffer[BMP388_PRESS_BYTES + BMP388_TEMP_BYTES];
    memset(buffer, 0, sizeof(buffer));

    i2c_read_bytes(BMP388_REG_DATA_P_XLSB, buffer, sizeof(buffer));

    if (pressure_raw) {
        *pressure_raw = (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16);
    }
    if (temperature_raw) {
        *temperature_raw = (uint32_t)buffer[3] | ((uint32_t)buffer[4] << 8) | ((uint32_t)buffer[5] << 16);
    }
}

void bmp388_update_measurement(void) {
    if (!bmp388_vars.calibrated) {
        return;
    }

    uint32_t press_raw = 0;
    uint32_t temp_raw = 0;
    bmp388_read_uncompensated(&press_raw, &temp_raw);

    double temperature = bmp388_compensate_temperature((int32_t)temp_raw);
    double pressure = bmp388_compensate_pressure((int32_t)press_raw);

    bmp388_vars.temperature_c = (float)temperature;
    bmp388_vars.pressure_pa = (float)pressure;
}

float bmp388_get_temperature_c(void) {
    return bmp388_vars.temperature_c;
}

float bmp388_get_pressure_pa(void) {
    return bmp388_vars.pressure_pa;
}

float bmp388_get_altitude_m(float sea_level_pa) {
    float pressure = bmp388_vars.pressure_pa;

    if (pressure <= 0.0f) {
        return 0.0f;
    }

    const float ratio = pressure / sea_level_pa;
    return (float)((1.0f - powf(ratio, 0.190284f)) * 44307.69f);
}

//=========================== private ========================================

static void bmp388_read_calibration(void) {
    uint8_t calib_raw[BMP388_REG_CALIB_LENGTH] = {0};
    i2c_read_bytes(BMP388_REG_CALIB_DATA, calib_raw, sizeof(calib_raw));

    uint16_t par_t1_u16 = (uint16_t)((calib_raw[1] << 8) | calib_raw[0]);
    uint16_t par_t2_u16 = (uint16_t)((calib_raw[3] << 8) | calib_raw[2]);
    int8_t par_t3_i8 = (int8_t)calib_raw[4];

    int16_t par_p1_i16 = (int16_t)((calib_raw[6] << 8) | calib_raw[5]);
    int16_t par_p2_i16 = (int16_t)((calib_raw[8] << 8) | calib_raw[7]);
    int8_t par_p3_i8 = (int8_t)calib_raw[9];
    int8_t par_p4_i8 = (int8_t)calib_raw[10];
    uint16_t par_p5_u16 = (uint16_t)((calib_raw[12] << 8) | calib_raw[11]);
    uint16_t par_p6_u16 = (uint16_t)((calib_raw[14] << 8) | calib_raw[13]);
    int8_t par_p7_i8 = (int8_t)calib_raw[15];
    int8_t par_p8_i8 = (int8_t)calib_raw[16];
    int16_t par_p9_i16 = (int16_t)((calib_raw[18] << 8) | calib_raw[17]);
    int8_t par_p10_i8 = (int8_t)calib_raw[19];
    int8_t par_p11_i8 = (int8_t)calib_raw[20];

    bmp388_vars.calib.par_t1 = ((double)par_t1_u16) / 0.00390625;              // raw / (1/2^-8)
    bmp388_vars.calib.par_t2 = ((double)par_t2_u16) / 1073741824.0;             // 2^-30
    bmp388_vars.calib.par_t3 = ((double)par_t3_i8) / 281474976710656.0;         // 2^-48

    bmp388_vars.calib.par_p1 = (((double)par_p1_i16) - 16384.0) / 1048576.0;     // 2^-20
    bmp388_vars.calib.par_p2 = (((double)par_p2_i16) - 16384.0) / 536870912.0;   // 2^-29
    bmp388_vars.calib.par_p3 = ((double)par_p3_i8) / 4294967296.0;               // 2^-32
    bmp388_vars.calib.par_p4 = ((double)par_p4_i8) / 137438953472.0;             // 2^-37
    bmp388_vars.calib.par_p5 = ((double)par_p5_u16) / 0.125;                     // 2^-(-3)
    bmp388_vars.calib.par_p6 = ((double)par_p6_u16) / 64.0;                      // 2^-6
    bmp388_vars.calib.par_p7 = ((double)par_p7_i8) / 256.0;                      // 2^-8
    bmp388_vars.calib.par_p8 = ((double)par_p8_i8) / 32768.0;                    // 2^-15
    bmp388_vars.calib.par_p9 = ((double)par_p9_i16) / 281474976710656.0;         // 2^-48
    bmp388_vars.calib.par_p10 = ((double)par_p10_i8) / 281474976710656.0;        // 2^-48
    bmp388_vars.calib.par_p11 = ((double)par_p11_i8) / 36893488147419103232.0;   // 2^-65
}

static double bmp388_compensate_temperature(int32_t uncomp_temp) {
    double partial1 = (double)uncomp_temp - bmp388_vars.calib.par_t1;
    double partial2 = partial1 * bmp388_vars.calib.par_t2;
    double result = partial2 + (partial1 * partial1) * bmp388_vars.calib.par_t3;

    bmp388_vars.calib.t_lin = result;
    return result;
}

static double bmp388_compensate_pressure(int32_t uncomp_press) {
    double t_lin = bmp388_vars.calib.t_lin;
    double partial1 = bmp388_vars.calib.par_p6 * t_lin;
    double partial2 = bmp388_vars.calib.par_p7 * t_lin * t_lin;
    double partial3 = bmp388_vars.calib.par_p8 * t_lin * t_lin * t_lin;
    double partial_out1 = bmp388_vars.calib.par_p5 + partial1 + partial2 + partial3;

    double partial4 = bmp388_vars.calib.par_p2 * t_lin;
    double partial5 = bmp388_vars.calib.par_p3 * t_lin * t_lin;
    double partial6 = bmp388_vars.calib.par_p4 * t_lin * t_lin * t_lin;
    double partial_out2 = (double)uncomp_press * (bmp388_vars.calib.par_p1 + partial4 + partial5 + partial6);

    double partial7 = (double)uncomp_press * (double)uncomp_press;
    double partial8 = bmp388_vars.calib.par_p9 + bmp388_vars.calib.par_p10 * t_lin;
    double partial9 = partial7 * partial8;
    double partial10 = partial7 * (double)uncomp_press * bmp388_vars.calib.par_p11;

    return partial_out1 + partial_out2 + partial9 + partial10;
}

static void bmp388_delay_cycles(uint32_t cycles) {
    volatile uint32_t count = cycles;
    while (count--) {
        (void)count;
    }
}
