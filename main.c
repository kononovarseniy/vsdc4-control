#include <stdio.h>
#include <CAENVMElib.h>



int main(int argc, char **argv) {
	short device, link;
	int32_t handle;
	CVErrorCodes cverr = cvSuccess;

	unsigned short addr_mod = cvA32_U_DATA; // A32 non-privileged data access

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

	
	cverr = CAENVME_ReadCycle(handle, base + offset, &data, addr_mod, cvD32);
	switch (cverr) {
		case cvSuccess:
			printf("Value: %08X\n", data);
			break;
		case cvBusError:
			printf("BusError\n");
			break;				   
		case cvCommError:
			printf(" Communication Error !!!");
			break;
		default:
			printf(" Unknown Error !!!");
			break;
	}

	CAENVME_End(handle);
	return 0;
}
