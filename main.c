#include <stdio.h>
#include <CAENVMElib.h>

const CVAddressModifier addr_mod = cvA32_U_DATA; // A32 non-privileged data access
const CVDataWidth data_width = cvD32;

int read(int32_t handle, uint32_t address, uint32_t *data) {
	CVErrorCodes cverr = CAENVME_ReadCycle(handle, address, data, addr_mod, data_width);
	switch (cverr) {
		case cvSuccess:
			return 0;
		case cvBusError:
			printf("BusError\n");
			break;				   
		case cvCommError:
			printf("Communication Error !!!");
			break;
		default:
			printf("Unknown Error !!!");
			break;
	}
	return 1;
}

#define DEVICE_ID 0x01FFFFC0
#define REF_H 0x01FFFFCC
#define REF_L 0x01FFFFD0
#define TIME_QUANT 0x01FFFFD4

int main(int argc, char **argv) {
	short device, link;
	int32_t handle;

	if (argc != 3) {
		printf("Usage: test VMEDevice VMELink");
		return 1;
	}
	device = atoi(argv[1]);
	link = atoi(argv[2]);
	
	if (CAENVME_Init(cvV2718, link, device, &handle)) {
		printf("Error opening device\n");
		return 1;
	}

	printf("Connected!!!\n");

	uint32_t base = 0xc0000000; // 0x44

	uint32_t data;
	float quant;

	
	if (read(handle, base + DEVICE_ID, &data) == 0)
		printf("DEV_ID: %08X\n", data);
	if (read(handle, base + REF_H, &data) == 0)
		printf("REF_H: %08X\n", data);
	if (read(handle, base + REF_L, &data) == 0)
		printf("REF_L: %08X\n", data);
	if (read(handle, base + TIME_QUANT, (uint32_t *)&quant) == 0)
		printf("TIME_QUANT: %f\n", quant);
	

	CAENVME_End(handle);
	return 0;
}
