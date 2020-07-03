#include "device_access.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <CAENVMElib.h>

const CVAddressModifier addr_mod = cvA32_U_DATA; // A32 non-privileged data access
const CVDataWidth data_width = cvD32;

struct device {
    int32_t handle;
    uint8_t irq;
    pthread_mutex_t mutex;
};

void cv_perror(const char *msg, int error_code) {
    if (error_code == 0)
        fprintf(stderr, "%s: SUCCESS\n", msg);
    else if (error_code < 0)
        fprintf(stderr, "%s: CAEN VME ERROR: %s\n", msg, CAENVME_DecodeError((CVErrorCodes)error_code));
    else
        fprintf(stderr, "%s: SYSTEM ERROR: %s\n", msg, strerror(error_code));
}

int cv_init(device **pdev, int link, int board, uint8_t irq) {
    errno = 0;
    CVErrorCodes cverr;
    
    int32_t handle;
    cverr = CAENVME_Init(cvV2718, link, board, &handle);
    if (cverr) return cverr;
    cverr = CAENVME_IRQEnable(handle, irq);
    if (cverr) return cverr;

    device *dev = (device *) malloc(sizeof(device));
    if (dev == NULL) {
        int err = errno;
        CAENVME_End(handle);
        return err;
    }

    dev->handle = handle;
    dev->irq = irq;  
    if (pthread_mutex_init(&dev->mutex, NULL)) {
        int err = errno;
        CAENVME_End(handle);
        free(dev);
        return err;
    }
    
    *pdev = dev;
    
    return 0;    
}

int cv_lock(device *dev, int *handle) {
    *handle = dev->handle;
    return pthread_mutex_lock(&dev->mutex);
}

int cv_unlock(device *dev) {
    return pthread_mutex_unlock(&dev->mutex);
}

void cv_end(device *dev) {
    CAENVME_End(dev->handle);
    pthread_mutex_destroy(&dev->mutex);
    free(dev);
}

int cv_read(device *dev, uint32_t address, uint32_t *data) {
    int32_t handle;
    cv_lock(dev, &handle);
    CVErrorCodes cverr = CAENVME_ReadCycle(handle, address, data, addr_mod, data_width);
    cv_unlock(dev);
    return cverr;
}

int cv_write(device *dev, uint32_t address, uint32_t data) {
    int32_t handle;
    cv_lock(dev, &handle);
    CVErrorCodes cverr = CAENVME_WriteCycle(handle, address, &data, addr_mod, data_width);
    cv_unlock(dev);
    return cverr;
}

int cv_get_irq_vector(device *dev, uint8_t *vec) {
    int32_t handle;
    cv_lock(dev, &handle);
    
    *vec = 0;
    uint8_t irq_mask;
    CVErrorCodes cverr = CAENVME_IRQCheck(handle, &irq_mask);
    if (cverr) {
        cv_unlock(dev);
        return cverr;
    }
    if (irq_mask & dev->irq) {
        cverr = CAENVME_IACKCycle(handle, cvIRQ5, vec, cvD8);
        if (cverr) {
            cv_unlock(dev);
            return cverr;
        }
    }
    
    cv_unlock(dev);
    return 0;
}

