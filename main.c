#include <stdio.h>
#include <CAENVMElib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "vsdc4.h"
#include "device_access.h"

const CVAddressModifier addr_mod = cvA32_U_DATA; // A32 non-privileged data access
const CVDataWidth data_width = cvD32;


// Read value from register
CVErrorCodes read_reg(int32_t handle, uint32_t address, uint32_t *data) {
    return CAENVME_ReadCycle(handle, address, data, addr_mod, data_width);
}

// Write value to register
CVErrorCodes write_reg(int32_t handle, uint32_t address, uint32_t data) {
    return CAENVME_WriteCycle(handle, address, &data, addr_mod, data_width);
}
    
struct vsdc {
    device *dev;
    uint32_t base;
};

// VSDC device information
struct vsdc_version {
    int swid;
    int hwid;
    int devid;
};

void print_vsdc_version(struct vsdc_version *info) {
    printf("Device: 0x%04X\n", info->devid);
    printf("Hardware: 0x%02X\n", info->hwid);
    printf("Software: 0x%02X\n", info->swid);
}

void decode_vsdc_version(uint32_t device_id, struct vsdc_version *info) {
    info->swid = device_id & 0xff;
    info->hwid = (device_id >> 8) & 0xff;
    info->devid = device_id >> 16;
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
        cv_perror("Writing ADC0_SR", cverr);
        return cverr;
    }
    
    // Setup timer
    float time_quant;
    cverr = read_reg(handle, base + TIME_QUANT, (uint32_t *)&time_quant);
    if (cverr) {
        cv_perror("Reading TIME_QUANT", cverr);
        return cverr;
    }
    cverr = write_reg(handle, ch_base + ADC_TIMER, (uint32_t)(time / time_quant));
    if (cverr) {
        cv_perror("Writing ADC0_TIMER", cverr);
        return cverr;
    }
    
    // Set waveform offset to the beginning of buffer
    cverr = write_reg(handle, ch_base + ADC_WRITE, 0);
    if (cverr) {
        cv_perror("Writing ADC0_WRITE", cverr);
        return cverr;
    }
    return cvSuccess;
}

// Trigger measurement if start source is program
CVErrorCodes start_measurement(int32_t handle, uint32_t base, uint32_t ch) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    CVErrorCodes cverr = write_reg(handle, ch_base + ADC_CSR, 0x1301);
    if (cverr) {
        cv_perror("Writing ADC0_CSR", cverr);
        return cverr;
    }
    return cvSuccess;
}

CVErrorCodes read_status(int32_t handle, uint32_t base, uint32_t ch, int *success) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    uint32_t status;
    CVErrorCodes cverr = read_reg(handle, ch_base + ADC_CSR, &status);
    if (cverr) {
        cv_perror("Reading ADC0_CSR", cverr);
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
        cv_perror("Writing ADC0_CSR", cverr);
        return cverr;
    }
    
    return cvSuccess;
}

CVErrorCodes read_integral(int32_t handle, uint32_t base, uint32_t ch, float *res) {
    uint32_t ch_base = base + getChannelRegistersOffset(ch);
    
    CVErrorCodes cverr = read_reg(handle, ch_base + ADC_INT, (uint32_t*) res);
    if (cverr) {
        cv_perror("Reading ADC0_INT", cverr);
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
        cv_perror("Reading ADC0_WRITE", cverr);
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
            cv_perror("Reading WAVEFORM", cverr);
            return cverr;
        }

        fprintf(f, "%f\n", val);
    }
    fclose(f);
    
    return cvSuccess;
}

int vsdc_get_version(struct vsdc *vsdc, struct vsdc_version *vsdc_version) {
    uint32_t device_id;
    int err = cv_read(vsdc->dev, vsdc->base + DEV_ID, &device_id);
    if (err)
        return err;
    decode_vsdc_version(device_id, vsdc_version);
    return 0;
}  

volatile int stop = 0;
void *trigger_thread(void *arg);
void *waiter_thread(void *arg);
void *reader_thread(void *arg);

int main(int argc, char **argv) {
    int err;
    
    struct vsdc vsdc;
    vsdc.base = 0x40000000; // VsDC3 - 0xc0 VsDC4 - 0x40
    err = cv_init(&vsdc.dev, 0, 0, cvIRQ5);
    if (err) {
        cv_perror("cv_init", err);
        return 1;
    }
    
    printf("Connection successful\n");
    printf("Base address: 0x%08X\n", vsdc.base);
    printf("\n");
    
    // Get and print vsdc version
    struct vsdc_version version;
    err = vsdc_get_version(&vsdc, &version);
    if (err) {
        cv_perror("vsdc_get_version", err);
        cv_end(vsdc.dev);
        return 1;
    }
    print_vsdc_version(&version);
        
        
    // Set interrupt vectors
    for (int ch = 0; ch < 4; ch++) {
        uint32_t ch_base = vsdc.base + getChannelRegistersOffset(ch);
        uint32_t vec = ch + 1;
        cv_write(vsdc.dev, ch_base + ADC_IRQ_VEC, vec);
        printf("ADC%d_IRQ_VEC: 0x%02X\n", ch, vec);
    }
    
    // Read REF_H voltage
    float voltage;
    err = cv_read(vsdc.dev, vsdc.base + REF_H, (uint32_t *)&voltage);
    if (err) {
        cv_perror("Reading REF_H", err);
        cv_end(vsdc.dev);
        return 1;
    }
    printf("REF_H: %f volts\n", voltage);
    
    // Start threads
    pthread_t trigger, waiter, reader;
    
    pthread_create(&trigger, NULL, trigger_thread, &vsdc);
    pthread_create(&waiter, NULL, waiter_thread, &vsdc);
    pthread_create(&reader, NULL, reader_thread, &vsdc);
    
    pthread_join(trigger, NULL);
    pthread_join(waiter, NULL);
    pthread_join(reader, NULL);
    
    cv_end(vsdc.dev);
    return 0;
}

void *trigger_thread(void *arg) {
    struct vsdc *vsdc = (struct vsdc *)arg;
    CVErrorCodes cverr;
    
    int32_t handle;
    cv_lock(vsdc->dev, &handle);
    
    for (int ch = 0; ch < 4; ch++) {
        cverr = init_single_measurement(handle, vsdc->base, ch, 0.001);
        if (cverr) {
            cv_perror("TRIGGER: Failed to initialize measurement", cverr);
            return NULL;
        }
    }
    
    cv_unlock(vsdc->dev);
    
    cv_lock(vsdc->dev, &handle);
    cverr = start_measurement(handle, vsdc->base, 3);
    if (cverr) {
        cv_perror("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(vsdc->dev);
    printf("TRIGGER: started ch3\n");
    
    
    cv_lock(vsdc->dev, &handle);
    cverr = start_measurement(handle, vsdc->base, 2);
    if (cverr) {
        cv_perror("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(vsdc->dev);
    printf("TRIGGER: started ch2\n");
    
    usleep(500*1000); // slep 0.5s
    
    cv_lock(vsdc->dev, &handle);
    cverr = start_measurement(handle, vsdc->base, 1);
    if (cverr) {
        cv_perror("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(vsdc->dev);
    printf("TRIGGER: started ch1\n");
    
    usleep(2000*1000); // slep 2s
    
    cv_lock(vsdc->dev, &handle);
    cverr = start_measurement(handle, vsdc->base, 0);
    if (cverr) {
        cv_perror("TRIGGER: Failed to trigger measurement", cverr);
            return NULL;
    }
    cv_unlock(vsdc->dev);
    printf("TRIGGER: started ch0\n");
    
    return NULL;
}

void *waiter_thread(void *arg) {
    struct vsdc *vsdc = (struct vsdc *)arg;
    
    int err;
    CVErrorCodes cverr;
    uint8_t ready_mask = 0x00;
    while (ready_mask != 0x0F) {
        usleep(50 * 1000);
        uint8_t vec;
        err = cv_get_irq_vector(vsdc->dev, &vec);
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
            cv_lock(vsdc->dev, &handle);
            
            int success;
            cverr = read_status(handle, vsdc->base, ch, &success);
            if (cverr) {
                cv_perror("WAITER: Failed to read channel status", cverr);
                continue;
            }
            
            cverr = clear_status(handle, vsdc->base, ch); // Not required
            if (cverr) {
                cv_perror("WAITER: Failed to clear status bits", cverr);
                continue;
            }
            // Return if no integral
            if (!success) {
                printf("WAITER: Integral is not ready\n");
                continue;
            }
            
            float int_res;
            cverr = read_integral(handle, vsdc->base, ch, &int_res);
            if (cverr) {
                cv_perror("WAITER: Failed to read integral", cverr);
                continue;
            }
            printf("WAITER: ch%d: %.4e\n\n", ch, int_res);
            cv_unlock(vsdc->dev);
        }
    }
    
    // STOP other threads
    stop = 1;
        
    return NULL;
}

// Does nothing useful. Just creates extra load
void *reader_thread(void *arg) {
    struct vsdc *vsdc = (struct vsdc *)arg;
    
    while (!stop) {
        //usleep(100);
        uint32_t val;
        int err = cv_read(vsdc->dev, vsdc->base + INT_LINE, &val);
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

