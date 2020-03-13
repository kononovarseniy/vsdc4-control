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

	uint32_t base = 0x44000000; // 0x44
	uint32_t offset = 0x01FFFFC0; // DEVICE_ID

	uint32_t data;

	
	if (read(handle, base + offset, &data) == 0) {
		printf("Value: %08X\n", data);
	}

	CAENVME_End(handle);
	return 0;
}
