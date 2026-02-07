#ifndef _DRIVERS_PCI_H
#define _DRIVERS_PCI_H

#include <stdint.h>

#define PCI_CLASS_UNCLASSIFIED      0x00
#define PCI_CLASS_STORAGE           0x01
#define PCI_CLASS_NETWORK           0x02
#define PCI_CLASS_DISPLAY           0x03
#define PCI_CLASS_MULTIMEDIA        0x04
#define PCI_CLASS_MEMORY            0x05
#define PCI_CLASS_BRIDGE            0x06
#define PCI_CLASS_COMMUNICATION     0x07
#define PCI_CLASS_SYSTEM            0x08
#define PCI_CLASS_INPUT             0x09
#define PCI_CLASS_DOCKING           0x0A
#define PCI_CLASS_PROCESSOR         0x0B
#define PCI_CLASS_SERIAL            0x0C
#define PCI_CLASS_WIRELESS          0x0D
#define PCI_CLASS_INTELLIGENT       0x0E
#define PCI_CLASS_SATELLITE         0x0F
#define PCI_CLASS_ENCRYPTION        0x10
#define PCI_CLASS_SIGNAL            0x11

typedef struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    
    uint16_t vendor_id;
    uint16_t device_id;
    
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t interrupt_line;
    
    uint32_t bar[6];        
} pci_device_t;

void pci_init(void);
uint32_t pci_get_device_count(void);
pci_device_t *pci_get_device(uint32_t index);
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t *pci_find_device_by_class(uint8_t class_code, uint8_t subclass);
uint16_t pci_read_vendor_id(uint8_t bus, uint8_t device, uint8_t func);
uint16_t pci_read_device_id(uint8_t bus, uint8_t device, uint8_t func);
uint8_t pci_read_class(uint8_t bus, uint8_t device, uint8_t func);
uint8_t pci_read_subclass(uint8_t bus, uint8_t device, uint8_t func);
uint8_t pci_read_prog_if(uint8_t bus, uint8_t device, uint8_t func);
uint8_t pci_read_header_type(uint8_t bus, uint8_t device, uint8_t func);
uint32_t pci_read_bar(uint8_t bus, uint8_t device, uint8_t func, uint8_t bar);
uint8_t pci_read_interrupt_line(uint8_t bus, uint8_t device, uint8_t func);

void pci_enable_bus_mastering(pci_device_t *dev);
void pci_enable_memory_space(pci_device_t *dev);
void pci_enable_io_space(pci_device_t *dev);

#endif 
