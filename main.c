#include <stdio.h>
#include <CAENVMElib.h>
#include <unistd.h>

#include "vsdc4.h"

const CVAddressModifier addr_mod = cvA32_U_DATA; // A32 non-privileged data access
const CVDataWidth data_width = cvD32;


void printError(const char *msg, CVErrorCodes err) {
	printf("%s: ", msg);
	switch (err) {
		case cvSuccess:
			printf("Success\n");
			return;
		case cvBusError:
			printf("VME bus error\n");
			break;				   
		case cvCommError:
			printf("Communication error\n");
			break;
		case cvGenericError:
			printf("Unspecified error\n");
			break;
		case cvInvalidParam:
			printf("Invalid parameter\n");
			break;
		case cvTimeoutError:
			printf("Timeout error\n");
			break;
		default:
			printf("Unknown Error\n");
			break;
	}
}

CVErrorCodes read_reg(int32_t handle, uint32_t address, uint32_t *data) {
	return CAENVME_ReadCycle(handle, address, data, addr_mod, data_width);
}

CVErrorCodes write_reg(int32_t handle, uint32_t address, uint32_t data) {
	return CAENVME_WriteCycle(handle, address, &data, addr_mod, data_width);
}
	

struct device_info {
	int swid;
	int hwid;
	int devid;
};

void print_device_info(struct device_info *info) {
	printf("Device: 0x%04X\n", info->devid);
	printf("Hardware: 0x%02X\n", info->hwid);
	printf("Software: 0x%02X\n", info->swid);
}

CVErrorCodes get_device_info(int32_t handle, uint32_t base, struct device_info *info) {
	uint32_t device_id;
	CVErrorCodes cverr = read_reg(handle, base + DEV_ID, &device_id);
	if (cverr)
		return cverr;
	info->swid = device_id & 0xff;
	info->hwid = (device_id >> 8) & 0xff;
	info->devid = device_id >> 16;
	return cvSuccess;
}

int measure(int32_t handle, uint32_t base, uint32_t ch, float time, float *res) {
	uint32_t ch_base = base + getChannelRegistersOffset(ch);
	uint32_t wf_base = base + getChannelWaveformOffset(ch);
	uint32_t settings = ADC_START_SRC_PROG | ADC_STOP_SRC_TIMER | ADC_INPUT_REF_H;
	CVErrorCodes cverr = write_reg(handle, ch_base + ADC_SR, settings);
	if (cverr) {
		printError("Writing ADC0_SR", cverr);
		return cverr;
	}
	float quant;
	cverr = read_reg(handle, base + TIME_QUANT, (uint32_t *)&quant);
	if (cverr) {
		printError("Reading TIME_QUANT", cverr);
		return cverr;
	}
	cverr = write_reg(handle, ch_base + ADC_TIMER, (uint32_t)(time / quant));
	if (cverr) {
		printError("Writing ADC0_TIMER", cverr);
		return cverr;
	}
	cverr = write_reg(handle, ch_base + ADC_WRITE, 0);
	if (cverr) {
		printError("Writing ADC0_WRITE", cverr);
		return cverr;
	}
	/*cverr = write_reg(handle, base + INT_BUFF_WRITE_POS, 0);
	if (cverr) {
		printError("Writing INT_BUFF_WRITE_POS", cverr);
		return cverr;
	}*/
	// START
	cverr = write_reg(handle, ch_base + ADC_CSR, 0x1301);
	if (cverr) {
		printError("Writing ADC0_CSR", cverr);
		return cverr;
	}
	//WAIT
	uint32_t status = 0;
	while (!(status & ADC_CSR_RESULT_MASK)) {
		cverr = read_reg(handle, ch_base + ADC_CSR, &status);
		if (cverr) {
			printError("Reading ADC0_CSR", cverr);
			return cverr;
		}
	}

	printf("status: %08X\n", status);
	if (status & ADC_CSR_GAIN_ERR)
		printf("GAIN_ERR\n");
	if (status & ADC_CSR_OVRNG)
		printf("OVRNG\n");
	if (status & ADC_CSR_MEM_OVF)
		printf("MEM_OVF\n");
	if (status & ADC_CSR_MISS_INT)
		printf("MISS_INT\n");
	if (status & ADC_CSR_MISS_START)
		printf("MISS_START\n");
	int ready = status & ADC_CSR_INTEGRAL_RDY;
	if (ready)
		printf("INTEGRAL_READY\n");
	else
		return 1;
	//READ
	cverr = read_reg(handle, ch_base + ADC_INT, (uint32_t*) res);
	if (cverr) {
		printError("Reading ADC0_INT", cverr);
		return cverr;
	}
	uint32_t samples;
	cverr = read_reg(handle, ch_base + ADC_WRITE, &samples);
	if (cverr) {
		printError("Reading ADC0_WRITE", cverr);
		return cverr;
	}
	printf("samples: %d (%d)\n", samples, samples - 128);
	FILE *f = fopen("wave.csv", "w");
	for (unsigned int i = 0; i < samples; i++) {
		float val;
		cverr = read_reg(handle, wf_base + i * 4, (uint32_t *)&val);
		if (cverr) {
			printError("Reading WAVEFORM", cverr);
			return cverr;
		}

		fprintf(f, "%f\n", val);
	}
	fclose(f);
	// Reset settings
	cverr = write_reg(handle, ch_base + ADC_SR, 0);
	if (cverr) {
		printError("Writing ADC0_SR", cverr);
		return cverr;
	}
	return cvSuccess;
}

int main(int argc, char **argv) {
	short device, link;
	int32_t handle;

	if (argc != 3) {
		printf("Usage: test VMEDevice VMELink\n");
		return 1;
	}
	device = atoi(argv[1]);
	link = atoi(argv[2]);
	
	CVErrorCodes cverr = CAENVME_Init(cvV2718, link, device, &handle);
	if (cverr) {
		printError("Error opening device", cverr);
		return 1;
	}

	printf("Connection successful\n");
	uint32_t base = 0x40000000; // VsDC3 - 0xc0 VsDC4 - 0x40
	printf("Base address: 0x%08X\n", base);
	printf("\n");

	struct device_info info;
	cverr = get_device_info(handle, base, &info);
	if (cverr) {
		printError("get_device_info", cverr);
		return 1;
	}
	print_device_info(&info);

	float quant;
	if (read_reg(handle, base + TIME_QUANT, (uint32_t *)&quant) == 0)
		printf("TIME_QUANT: %.9f\n", quant);


	uint32_t ch_base = base + getChannelRegistersOffset(0);
	printf("Start calibration...\n");
	cverr = write_reg(handle, ch_base + ADC_CSR, 1 << 2);
	if (cverr) {
		printError("Write ADC0_CSR", cverr);
		return 1;
	}
	
	uint32_t status = 0;
	int cnt = 0;
	while (!(status & 2)) {
		cverr = read_reg(handle, ch_base + ADC_CSR, &status);
		cnt++;
		if (cverr) {
			printError("Read ADC0_CSR", cverr);	
		}
	}
	printf("Calibrated %d\n", cnt);
	usleep(500);

	//printError("RESET", write_reg(handle, base + GCR, 1 << 3));
	//return 1;

	for (int ch = 0; ch < 4; ch++) {
		printf("\nMeasuring on channel #%d\n", ch);
		float res;
		float time = 0.001;
		if (measure(handle, base, ch, time, &res)) {
			printf("Measurement failed\n");
			return 1;
		}
		float value;
		cverr = read_reg(handle, base + REF_H, (uint32_t *)&value);
		if (cverr) {
			printError("Reading REF_H", cverr);
			return 1;
		}
		printf("Measured value: %.4e (expected %.4e)\n", res, (time) * value);
	}


	CAENVME_End(handle);
	return 0;
}
