#include <stdio.h>
#include <CAENVMElib.h>

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

CVErrorCodes read(int32_t handle, uint32_t address, uint32_t *data) {
	return CAENVME_ReadCycle(handle, address, data, addr_mod, data_width);
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
	CVErrorCodes cverr = read(handle, base + DEV_ID, &device_id);
	if (cverr)
		return cverr;
	info->swid = device_id & 0xff;
	info->hwid = (device_id >> 8) & 0xff;
	info->devid = device_id >> 16;
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
	if (read(handle, base + TIME_QUANT, (uint32_t *)&quant) == 0)
		printf("TIME_QUANT: %.9f\n", quant);
	

	CAENVME_End(handle);
	return 0;
}
