#include <stdio.h>
#include <CAENVMElib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "vsdc4.h"
#include "device_access.h"

const CVAddressModifier addr_mod = cvA32_U_DATA; // A32 non-privileged data access
const CVDataWidth data_width = cvD32;

const uint32_t base = 0x40000000; // VsDC3 - 0xc0 VsDC4 - 0x40


// Print CAEN VME error
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

// Read value from register
CVErrorCodes read_reg(int32_t handle, uint32_t address, uint32_t *data) {
    return CAENVME_ReadCycle(handle, address, data, addr_mod, data_width);
}

// Write value to register
CVErrorCodes write_reg(int32_t handle, uint32_t address, uint32_t data) {
    return CAENVME_WriteCycle(handle, address, &data, addr_mod, data_width);
}
    

// VSDC device information
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

// Read VSDC version
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

// Initialize single measurement with high reference voltage as input
CVErrorCodes init_single_measurement(int32_t handle, uint32_t base, uint32_t ch, float time) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    // Start source - program
    // Stop source - timer
    // Input source - high reference voltage
    // And enable interrupts for this channel
    uint32_t settings = ADC_START_SRC_PROG | ADC_STOP_SRC_TIMER | ADC_INPUT_REF_H | ADC_IRQ_ENABLED;
    CVErrorCodes cverr = write_reg(handle, ch_base + ADC_SR, settings);
    if (cverr) {
        printError("Writing ADC0_SR", cverr);
        return cverr;
    }
    
    // Setup timer
    float time_quant;
    cverr = read_reg(handle, base + TIME_QUANT, (uint32_t *)&time_quant);
    if (cverr) {
        printError("Reading TIME_QUANT", cverr);
        return cverr;
    }
    cverr = write_reg(handle, ch_base + ADC_TIMER, (uint32_t)(time / time_quant));
    if (cverr) {
        printError("Writing ADC0_TIMER", cverr);
        return cverr;
    }
    
    // Set waveform offset to the beginning of buffer
    cverr = write_reg(handle, ch_base + ADC_WRITE, 0);
    if (cverr) {
        printError("Writing ADC0_WRITE", cverr);
        return cverr;
    }
    return cvSuccess;
}

// Trigger measurement if start source is program
CVErrorCodes start_measurement(int32_t handle, uint32_t base, uint32_t ch) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    CVErrorCodes cverr = write_reg(handle, ch_base + ADC_CSR, 0x1301);
    if (cverr) {
        printError("Writing ADC0_CSR", cverr);
        return cverr;
    }
    return cvSuccess;
}

CVErrorCodes wait_for_measurement(int32_t handle, uint32_t base, uint32_t ch) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    uint32_t status = 0;
    while (!(status & ADC_CSR_RESULT_MASK)) {
        CVErrorCodes cverr = read_reg(handle, ch_base + ADC_CSR, &status);
        if (cverr) {
            printError("Reading ADC0_CSR", cverr);
            return cverr;
        }
    }
    return cvSuccess;
}

CVErrorCodes wait_for_measurement_irq(int32_t handle, uint32_t base, uint32_t ch) {
    CVErrorCodes cverr;
    
    CAENVME_IRQEnable(handle, cvIRQ5);
    //CAENVME_IRQDisable(handle, cvIRQ5);
    //puts("1");
    cverr = CAENVME_IRQWait(handle, cvIRQ5, 1000);
    //puts("2");
    if (cverr == cvTimeoutError) {
        printf("IRQ timeout\n");
        return cverr;
    }
    for (;;) {
        uint32_t vec;
        // TODO: use CAENVME_IRQCheck in loop condition
        cverr = CAENVME_IACKCycle(handle, cvIRQ5, &vec, data_width);
        if (cverr) {
            if (cverr == cvBusError) {
                printf("Wrong IRQ vector\n");
                // Ignore interrupts from other channels
                // This algorithm is not suitable for use
                return cverr;
            }
            
            printError("IACK CYCLE", cverr);
            return cverr;
        }
        vec &= 0xff;
        printf("IRQ vector: 0x%02X\n", vec);
        
        if (vec == 0x10 + ch) // Ignore interrupts from other channels
            break;
    }
    
    return cvSuccess;
}

CVErrorCodes read_status(int32_t handle, uint32_t base, uint32_t ch, int *success) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    uint32_t status;
    CVErrorCodes cverr = read_reg(handle, ch_base + ADC_CSR, &status);
    if (cverr) {
        printError("Reading ADC0_CSR", cverr);
        return cverr;
    }

    printf("status: 0x%08X\n", status);
    if (status & ADC_CSR_GAIN_ERR)
        printf("\tGAIN_ERR\n");
    if (status & ADC_CSR_OVRNG)
        printf("\tOVRNG\n");
    if (status & ADC_CSR_MEM_OVF)
        printf("\tMEM_OVF\n");
    if (status & ADC_CSR_MISS_INT)
        printf("\tMISS_INT\n");
    if (status & ADC_CSR_MISS_START)
        printf("\tMISS_START\n");
        
    *success = status & ADC_CSR_INTEGRAL_RDY;
    if (*success)
        printf("\tintegral is ready\n");
    else
        printf("\tintegral is NOT ready\n");
    
    return cvSuccess;
}

CVErrorCodes clear_status(int32_t handle, uint32_t base, uint32_t ch) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    CVErrorCodes cverr = write_reg(handle, ch_base + ADC_CSR, ADC_CSR_RESULT_MASK);
    if (cverr) {
        printError("Writing ADC0_CSR", cverr);
        return cverr;
    }
    
    return cvSuccess;
}

CVErrorCodes read_integral(int32_t handle, uint32_t base, uint32_t ch, float *res) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    CVErrorCodes cverr = read_reg(handle, ch_base + ADC_INT, (uint32_t*) res);
    if (cverr) {
        printError("Reading ADC0_INT", cverr);
        return cverr;
    }
    
    return cvSuccess;
}

CVErrorCodes read_waveform(int32_t handle, uint32_t base, uint32_t ch, const char *file) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    uint32_t wf_base = base + getChannelWaveformOffset(ch);
    
    // Read number of samples in waveform
    uint32_t samples;
    CVErrorCodes cverr = read_reg(handle, ch_base + ADC_WRITE, &samples);
    if (cverr) {
        printError("Reading ADC0_WRITE", cverr);
        return cverr;
    }
    // According to the documentation, VSDC4 records 128 additional samples after stopping
    printf("samples: %d (%d)\n", samples, samples - 128);
    
    // Read waveform
    FILE *f = fopen(file, "w");
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
    
    return cvSuccess;
}

int measure(int32_t handle, uint32_t base, uint32_t ch, float time, float *res) {
    CVErrorCodes cverr;
    cverr = init_single_measurement(handle, base, ch, time);
    if (cverr) {
        printError("Failed to initialize measurement", cverr);
        return 0;
    }
    
    cverr = start_measurement(handle, base, ch);
    if (cverr) {
        printError("Failed to trigger measurement", cverr);
        return 0;
    }
    
    cverr = wait_for_measurement_irq(handle, base, ch);
    if (cverr) {
        printError("Error while waiting for measurement", cverr);
        return 0;
    }
    //usleep(1000);
    
    int success;
    cverr = read_status(handle, base, ch, &success);
    if (cverr) {
        printError("Failed to read channel status", cverr);
        return 0;
    }
    cverr = clear_status(handle, base, ch);
    if (cverr) {
        printError("Failed to clear status bits", cverr);
        return 0;
    }
    // Return if no integral
    if (!success) {
        printf("Integral is not ready\n");
        return 0;
    }
    
    cverr = read_integral(handle, base, ch, res);
    if (cverr) {
        printError("Failed to read integral", cverr);
        return 0;
    }
    
    cverr = read_waveform(handle, base, ch, "wave.csv");
    if (cverr) {
        printError("Failed to read waveform", cverr);
        return 0;
    }
    
    return 1;
}

int full_vsdc_reset(int32_t handle, uint32_t base) {
    CVErrorCodes cverr;
    // Reset VSDC. It looks like reset does nothing. (All registers retain their values)
    /*
    cverr = write_reg(handle, base + GCR, 1 << 3);
    if (cverr) {
        printError("Unable to send reset command", cverr);
        return 0;
    }
    // No more than 1 second is required to reset device
    usleep(1000*1000);
    */
    
    // ACK pending interrupts
    for (;;) {
        uint8_t mask;
        cverr = CAENVME_IRQCheck(handle, &mask);
        if (cverr) {
            printError("IRQ check failed", cverr);
            return 0;
        }
        if (!(mask & cvIRQ5)) {
            printf("No pending IRQ detected\n");
            break;
        }
        CAENVME_IRQEnable(handle, cvIRQ5);
        cverr = CAENVME_IRQWait(handle, cvIRQ5, 100);
        if (cverr) {
            if (cverr == cvTimeoutError) {
                printf("IRQ timeout\n");
                break;
            }
        }
        
        uint32_t vec;
        cverr = CAENVME_IACKCycle(handle, cvIRQ5, &vec, data_width);
        if (cverr) {
            printError("IACK CYCLE", cverr);
            return 0;
        }
        vec &= 0xff;
        printf("IRQ vector: 0x%02X\n", vec);
    }
    CAENVME_IRQDisable(handle, cvIRQ5);
    
    return 1;
}

volatile int stop = 0;

void *trigger_thread(void *arg) {
    device *dev = (device *)arg;
    CVErrorCodes cverr;
    
    int32_t handle;
    cv_lock(dev, &handle);
    
    for (int ch = 0; ch < 4; ch++) {
        cverr = init_single_measurement(handle, base, ch, 0.001);
        if (cverr) {
            printError("TRIGGER: Failed to initialize measurement", cverr);
            return NULL;
        }
    }
    
    cv_unlock(dev);
    
    cv_lock(dev, &handle);
    cverr = start_measurement(handle, base, 3);
    if (cverr) {
        printError("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(dev);
    printf("TRIGGER: started ch3\n");
    
    
    cv_lock(dev, &handle);
    cverr = start_measurement(handle, base, 2);
    if (cverr) {
        printError("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(dev);
    printf("TRIGGER: started ch2\n");
    
    usleep(500*1000); // slep 0.5s
    
    cv_lock(dev, &handle);
    cverr = start_measurement(handle, base, 1);
    if (cverr) {
        printError("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(dev);
    printf("TRIGGER: started ch1\n");
    
    usleep(2000*1000); // slep 2s
    
    cv_lock(dev, &handle);
    cverr = start_measurement(handle, base, 0);
    if (cverr) {
        printError("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(dev);
    printf("TRIGGER: started ch0\n");
    
    return NULL;
}

void *waiter_thread(void *arg) {
    device *dev = (device *)arg;
    
    int err;
    CVErrorCodes cverr;
    uint8_t ready_mask = 0x00;
    while (ready_mask != 0x0F) {
        usleep(50 * 1000);
        uint8_t vec;
        err = cv_get_irq_vector(dev, &vec);
        if (err) {
            cv_perror("WAITER: get irq failed", cverr);
            continue;
        }
        if (vec) {
            printf("WAITER: irq received 0x%02X\n", vec);
            if (vec > 4) {
                fprintf(stderr, "WAITER: WRONG_VECTOR\n");
                continue;
            }
            
            // Handle interrupt
            int ch = vec - 1;
            ready_mask |= 1 << ch;
            int32_t handle;
            cv_lock(dev, &handle);
            
            int success;
            cverr = read_status(handle, base, ch, &success);
            if (cverr) {
                printError("WAITER: Failed to read channel status", cverr);
                continue;
            }
            cverr = clear_status(handle, base, ch);
            if (cverr) {
                printError("WAITER: Failed to clear status bits", cverr);
                continue;
            }
            // Return if no integral
            if (!success) {
                printf("WAITER: Integral is not ready\n");
                continue;
            }
            
            float int_res;
            cverr = read_integral(handle, base, ch, &int_res);
            if (cverr) {
                printError("WAITER: Failed to read integral", cverr);
                continue;
            }
            printf("WAITER: ch%d: %.4e\n\n", ch, int_res);
            cv_unlock(dev);
        }
    }
    
    // STOP other threads
    stop = 1;
        
    return NULL;
}

void *reader_thread(void *arg) {
    device *dev = (device *)arg;
    
    while (!stop) {
        //usleep(1000);
        uint32_t val;
        int err = cv_read(dev, base + INT_LINE, &val);
        if (err) {
            cv_perror("READER: read INT_LINE", err);
            continue;
        }
        if (val != 5) {
            fprintf(stderr, "READER: WRONG VALUE\n");
        }
    }
    return NULL;
}

int multithread_test() {
    device *dev;
    int err = cv_init(&dev, 0, 0, cvIRQ5);
    if (dev == NULL) {
        if (errno == 0)
            fprintf(stderr, "CAEN VME error while initialising device\n");
        else
            perror("cv_init");
        return 1;
    }
    
    // Set interrupt vectors
    for (int ch = 0; ch < 4; ch++) {
        uint32_t ch_base = base + getChannelRegistersOffset(ch);
        uint32_t vec = ch + 1;
        cv_write(dev, ch_base + ADC_IRQ_VEC, vec);
        printf("ADC%d_IRQ_VEC: 0x%02X\n", ch, vec);
    }
    
    // Read REF_H voltage
    float voltage;
    err = cv_read(dev, base + REF_H, (uint32_t *)&voltage);
    if (err) {
        cv_perror("Reading REF_H", err);
        return 1;
    }
    printf("REF_H: %f volts\n", voltage);
    
    pthread_t trigger, waiter, reader;
    
    pthread_create(&trigger, NULL, trigger_thread, dev);
    pthread_create(&waiter, NULL, waiter_thread, dev);
    pthread_create(&reader, NULL, reader_thread, dev);
    
    pthread_join(trigger, NULL);
    pthread_join(waiter, NULL);
    pthread_join(reader, NULL);
    
    cv_end(dev);
    return 0;
}

int main(int argc, char **argv) {
    return multithread_test();

    int32_t handle;
    short device, link;
    uint32_t base = 0x40000000; // VsDC3 - 0xc0 VsDC4 - 0x40

    if (argc != 3) {
        printf("Usage:\n\ttest VMEDevice VMELink\n");
        printf("example:\n\ttest 0 0\n");
        return 1;
    }
    device = atoi(argv[1]);
    link = atoi(argv[2]);
    
    CVErrorCodes cverr = CAENVME_Init(cvV2718, link, device, &handle);
    if (cverr) {
        printError("Error opening device", cverr);
        return 1;
    }
    
    /*printError("RESET", CAENVME_SystemReset(handle));
    CAENVME_End(handle);
    return 0;*/

    printf("Connection successful\n");
    printf("Base address: 0x%08X\n", base);
    printf("\n");
    
    printf("Reseting VSDC...\n");
    if (full_vsdc_reset(handle, base))
        printf("VSDC restarted\n");
    else
        printf("Failed to restart VSDC\n");
    printf("\n");

    struct device_info info;
    cverr = get_device_info(handle, base, &info);
    if (cverr) {
        printError("get_device_info", cverr);
        return 1;
    }
    print_device_info(&info);

    float quant;
    cverr = read_reg(handle, base + TIME_QUANT, (uint32_t *)&quant);
    if (cverr) {
        printError("Read TIME_QUANT", cverr);
        return 1;
    }
    printf("TIME_QUANT: %.9f\n", quant);

    uint32_t int_line;
    cverr = read_reg(handle, base + INT_LINE, &int_line);
    if (cverr != 0) {
        printError("Read INT_LINE", cverr);
        return 1;
    }
    printf("INT_LINE: %d\n", int_line);
  
    /*
    // Calibration. Not required?
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
    usleep(500);*/

    // Set interrupt vectors
    for (int ch = 0; ch < 4; ch++) {
        uint32_t ch_base = base + getChannelRegistersOffset(ch);
        uint32_t vec = 0x10 + ch;
        write_reg(handle, ch_base + ADC_IRQ_VEC, vec);
        printf("ADC%d_IRQ_VEC: 0x%02X\n", ch, vec);
    }
    
    float time = 0.001;
    // Read REF_H voltage
    float voltage;
    cverr = read_reg(handle, base + REF_H, (uint32_t *)&voltage);
    if (cverr) {
        printError("Reading REF_H", cverr);
        return 1;
    }
    
    // Sequential measurements on each channel
    for (int ch = 0; ch < 4; ch++) {
        printf("\nMeasuring on channel #%d\n", ch);
        printf("\tvoltage: %f volts\n\tduration: %f seconds\n", voltage, time);
        
        float res;
        if (!measure(handle, base, ch, time, &res)) {
            printf("Measurement failed\n");
            return 1;
        }
        printf("Measured value: %.4e\nExpected value: %.4e\n", res, voltage * time);
    }

    CAENVME_End(handle);
    return 0;
}
