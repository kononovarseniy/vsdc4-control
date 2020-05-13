#include "vsdc4.h"

static uint32_t reg_offset[4] = { CH0, CH1, CH2, CH3 };
uint32_t getChannelRegistersOffset(int ch) {
	return reg_offset[ch];
}

static uint32_t wf_offset[4] = { WAVEFORM0, WAVEFORM1, WAVEFORM2, WAVEFORM3 };
uint32_t getChannelWaveformOffset(int ch) {
	return wf_offset[ch];
}
