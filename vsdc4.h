#ifndef VSDC4_H_INCLUDED
#define VSDC4_H_INCLUDED

#include <stdint.h>

#define DEV_ID 0x01FFFFC0
#define GCR 0x01FFFFC4
#define GSR 0x01FFFFC8
#define REF_H 0x01FFFFCC
#define REF_L 0x01FFFFD0
#define TIME_QUANT 0x01FFFFD4
#define INT_LINE 0x01FFFFD8
#define AUZ_GNDMX_DLY 0x01FFFFE0
#define AUZ_PAUSE_NUM 0x01FFFFE4
#define AUZ_SW_NUM 0x01FFFFE8
#define AUZ_FULL_NUM 0x01FFFFEC
#define CAL_PAUSE 0x01FFFFF0
#define SW_GND_NUM 0x01FFFFF4
#define GND_NUM 0x01FFFFF8
#define GAIN_NUM 0x01FFFFFC

#define TG_CSR 0x00FFFFC0
#define TG_SETTINGS 0x00FFFFC4
#define TG_CH0_PHASE 0x00FFFFC8
#define TG_CH1_PHASE 0x00FFFFCC
#define TG_CH2_PHASE 0x00FFFFD0
#define TG_CH3_PHASE 0x00FFFFD4
#define TG_TMR_PERIOD 0x00FFFFD8
#define TG_IRQ_CSR 0x00FFFFDC
#define INT_BUFF_STATUS 0x00FFFFE0
#define INT_BUFF_SW_TIME 0x00FFFFE4
#define INT_BUFF_SW_ZERO 0x00FFFFE8
#define INT_BUFF_FULL_ZERO 0x00FFFFEC
#define INT_BUFF_INTEGRAL 0x00FFFFF0
#define INT_BUF_CTRL 0x00FFFFF4
#define INT_BUFF_READ_POS 0x00FFFFF8
#define INT_BUFF_WRITE_POS 0x00FFFFFC

// Constants for ADCx_SR
#define ADC_START_SRC_PROG 0x0
#define ADC_START_SRC_A 0x1
#define ADC_START_SRC_B 0x2
#define ADC_START_SRC_C 0x3
#define ADC_START_SRC_D 0x4
#define ADC_START_SRC_BP 0x5

#define ADC_STOP_SRC_TIMER (0x0<<3)
#define ADC_STOP_SRC_PROG (0x1<<3)
#define ADC_STOP_SRC_A (0x2<<3)
#define ADC_STOP_SRC_B (0x3<<3)
#define ADC_STOP_SRC_C (0x4<<3)
#define ADC_STOP_SRC_D (0x5<<3)
#define ADC_STOP_SRC_BP (0x6<<3)

#define ADC_INPUT_SIGNAL (0x0<<6)
#define ADC_INPUT_GND (0x1<<6)
#define ADC_INPUT_REF_H (0x2<<6)
#define ADC_INPUT_REF_L (0x3<<6)

#define ADC_IRQ_ENABLED (1 << 11)

// Constants for ADCx_CSR
#define ADC_CSR_PSTART (1 << 0)
#define ADC_CSR_PSTOP (1 << 1)
#define ADC_CSR_CALIB (1 << 2)
#define ADC_CSR_PCLR (1 << 3)
#define ADC_CSR_GAIN_ERR (1 << 4)
#define ADC_CSR_OVRNG (1 << 5)
#define ADC_CSR_MEM_OVF (1 << 6)
#define ADC_CSR_MISS_INT (1 << 7)
#define ADC_CSR_MISS_START (1 << 8)
#define ADC_CSR_INTEGRAL_RDY (1 << 12)
#define ADC_CSR_RESULT_MASK 0x11F0
#define ADC_CSR_RANGE_MASK (0x7 << 24)

#define ADC_CSR 0x0
#define ADC_SR 0x4
#define ADC_TIMER 0x8
#define ADC_AVGN 0xC
#define ADC_WRITE 0x14
#define ADC_INT 0x1C
#define ADC_TIMER_PR 0x24
#define BP0_SYNC_MUX 0x2C
#define ADC_IRQ_VEC 0x30
#define ADC_OFFS 0x34
#define ADC_SW_OFFS 0x38
#define ADC_QUANT 0x3C

#define WAVEFORM0 0x00000000
#define WAVEFORM1 0x003F0000
#define WAVEFORM2 0x007E0000
#define WAVEFORM3 0x00BD0000

#define CH0 0x00FFFF80
#define CH1 0x01FFFF80
#define CH2 0x00FFFF00
#define CH3 0x01FFFF00

#define INTEGRAL_BUF 0x01000000

uint32_t getChannelRegistersOffset(int ch);
uint32_t getChannelWaveformOffset(int ch);


#endif
