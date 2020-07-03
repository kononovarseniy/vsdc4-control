#ifndef DEVICE_ACCESS_H_INCLUDED
#define DEVICE_ACCESS_H_INCLUDED

// This module controls concurrent access to the CAEN VME board.
// 
// Functions cv_init and cv_end are not thread-safe,
// but I think there is no need to call them concurrently.
// All other functions are thread-safe.
// 
// Most functions in this file return an error code
// which is either a system error (such as ENOMEM) or a CAEN VME error.
// A zero return code indicates success.

#include <stdint.h>


typedef struct device device;

// Print error string to stderr
void cv_perror(const char *msg, int error_code);

int cv_init(device **pdev, int link, int board, uint8_t irq);
void cv_end(device *dev);

// To execute sequence of CAENVME_* operations you must call cv_lock before any CAENVME_* call.
// The device will be locked for other threads until cv_unlock is called.
// These functions use a non-recursive mutex, so they cannot be nested.
int cv_lock(device *dev, int *handle);
int cv_unlock(device *dev);

// cv_read, cv_write and cv_get_irq_vector functions use cv_lock/cv_unlock,

// this means that you cannot use them if the current thread previously locked the device using cv_lock.
int cv_read(device *dev, uint32_t address, uint32_t *data);
int cv_write(device *dev, uint32_t address, uint32_t data);

// Get current interrupt vector.
// Interrupt vector returned via vec argument.
// If there is no active IRQ then interrupt vector is set to 0.
int cv_get_irq_vector(device *dev, uint8_t *vec);


#endif
