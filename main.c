#include <stdio.h>
#include <CAENVMElib.h>
#include <unistd.h>

#include "vsdc3.h"

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

int measure(int32_t handle, uint32_t base, float time, float *res) {
	uint32_t settings = ADC_START_SRC_PROG | ADC_STOP_SRC_TIMER | ADC_INPUT_REF_H;
	CVErrorCodes cverr = write_reg(handle, base + ADC0_SR, settings);
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
	cverr = write_reg(handle, base + ADC0_TIMER, (uint32_t)(time / quant));
	if (cverr) {
		printError("Writing ADC0_TIMER", cverr);
		return cverr;
	}
	cverr = write_reg(handle, base + ADC0_WRITE, 0);
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
	cverr = write_reg(handle, base + ADC0_CSR, 0x1301);
	if (cverr) {
		printError("Writing ADC0_CSR", cverr);
		return cverr;
	}
	//WAIT
	uint32_t status = 0;
	while (!(status & 2)) {
		cverr = read_reg(handle, base + ADC0_CSR, &status);
		if (cverr) {
			printError("Reading ADC0_CSR", cverr);
			return cverr;
		}
	}

	printf("status: %08X\n", status);
	if (status & (1 << 5))
		printf("GAIN_ERR\n");
	if (status & (1 << 6))
		printf("OVRNG\n");
	if (status & (1 << 7))
		printf("MEM_OVF\n");
	if (status & (1 << 8))
		printf("MISS_INT\n");
	if (status & (1 << 9))
		printf("MISS_START\n");
	int ready = status & (1 << 12);
	if (ready)
		printf("INTEGRAL_READY\n");
	//else
		//return 1;
	//READ
	cverr = read_reg(handle, base + ADC0_INT, (uint32_t*) res);
	if (cverr) {
		printError("Reading ADC0_INT", cverr);
		return cverr;
	}
	uint32_t samples;
	cverr = read_reg(handle, base + ADC0_WRITE, &samples);
	if (cverr) {
		printError("Reading ADC0_WRITE", cverr);
		return cverr;
	}
	printf("samples: %d (%d)\n", samples, samples - 128);
	FILE *f = fopen("wave.csv", "w");
	int i = 0;
	int prev_high = 0;
	for (i = 0; i < samples; i++) {
		float val;
		while (1) {
			cverr = read_reg(handle, base + i * 4, (uint32_t *)&val);
			if (cverr) {
				printError("Reading WAVEFORM", cverr);
				return cverr;
			}
			if (prev_high && i < 30 - 5 + time / quant && val < 1) {
				printf("INVALID READ sample #%d: %.10f (0x%08x)\n",
					i, val, *((uint32_t*)&val));
				continue;
			}
			break;
		}

		prev_high = val > 1;

		fprintf(f, "%f\n", val);
	}
	fclose(f);
	// Reset settings
	cverr = write_reg(handle, base + ADC0_SR, 0);
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
	uint32_t base = 0xc0000000; // 0x44
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

	printf("Start calibration...\n");
	cverr = write_reg(handle, base + ADC0_CSR, 1 << 2);
	if (cverr) {
		printError("Write ADC0_CSR", cverr);
		return 1;
	}
	
	uint32_t status = 0;
	int cnt = 0;
	while (!(status & 2)) {
		cverr = read_reg(handle, base + ADC0_CSR, &status);
		cnt++;
		if (cverr) {
			printError("Read ADC0_CSR", cverr);	
		}
	}
	printf("Calibrated %d\n", cnt);
	usleep(500);

	//printError("RESET", write_reg(handle, base + GCR, 1 << 3));
	//return 1;

	int i = 0;
	for (; i < 1; i++) {
	float res;
	float time = 0.000320;
	if (measure(handle, base, time, &res)) {
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
