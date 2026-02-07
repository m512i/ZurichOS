
#ifndef _DRIVERS_DRIVER_H
#define _DRIVERS_DRIVER_H

#include <stdint.h>
#include <drivers/pci.h>

#define MAX_DRIVERS 32
#define PCI_ANY_ID 0xFFFF

typedef struct pci_device_id {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subvendor_id;
    uint16_t subdevice_id;
    uint32_t class_code;        
    uint32_t class_mask;        
    unsigned long driver_data;  
} pci_device_id_t;

#define PCI_DEVICE(vend, dev) \
    { .vendor_id = (vend), .device_id = (dev), \
      .subvendor_id = PCI_ANY_ID, .subdevice_id = PCI_ANY_ID, \
      .class_code = 0, .class_mask = 0, .driver_data = 0 }

#define PCI_DEVICE_CLASS(cls, msk) \
    { .vendor_id = PCI_ANY_ID, .device_id = PCI_ANY_ID, \
      .subvendor_id = PCI_ANY_ID, .subdevice_id = PCI_ANY_ID, \
      .class_code = (cls), .class_mask = (msk), .driver_data = 0 }

#define PCI_DEVICE_END { 0, 0, 0, 0, 0, 0, 0 }

typedef enum {
    DRIVER_STATUS_UNLOADED = 0,
    DRIVER_STATUS_LOADED,
    DRIVER_STATUS_ACTIVE,
    DRIVER_STATUS_ERROR
} driver_status_t;

struct pci_driver;
struct driver_domain;

typedef struct pci_driver {

    const char *name;
    const pci_device_id_t *id_table;
    int (*probe)(pci_device_t *dev, const pci_device_id_t *id);
    void (*remove)(pci_device_t *dev);
    int (*suspend)(pci_device_t *dev);
    int (*resume)(pci_device_t *dev);
    
    driver_status_t status;
    uint32_t devices_bound;

    struct driver_domain *domain;
    int isolation_level;
    uint16_t io_port_base;
    uint16_t io_port_count;
} pci_driver_t;

int pci_register_driver(pci_driver_t *driver);
void pci_unregister_driver(pci_driver_t *driver);

pci_driver_t *pci_get_driver(const char *name);

uint32_t pci_get_driver_count(void);

pci_driver_t *pci_get_driver_by_index(uint32_t index);

int pci_driver_probe_devices(pci_driver_t *driver);

void pci_probe_all_drivers(void);

const pci_device_id_t *pci_match_device(pci_driver_t *driver, pci_device_t *dev);

pci_driver_t *pci_get_device_driver(pci_device_t *dev);

typedef struct {
    pci_device_t *device;
    pci_driver_t *driver;
} pci_binding_t;

uint32_t pci_get_binding_count(void);
pci_binding_t *pci_get_binding(uint32_t index);

#endif 
