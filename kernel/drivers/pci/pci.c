/* PCI Bus Driver
 * Enumerates and configures PCI devices
 */

#include <kernel/kernel.h>
#include <drivers/pci.h>

#define PCI_CONFIG_ADDR     0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define MAX_PCI_DEVICES     64

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

static uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset)
{
    uint32_t address = (1 << 31)            
                     | ((uint32_t)bus << 16)
                     | ((uint32_t)device << 11)
                     | ((uint32_t)func << 8)
                     | (offset & 0xFC);
    
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value)
{
    uint32_t address = (1 << 31)
                     | ((uint32_t)bus << 16)
                     | ((uint32_t)device << 11)
                     | ((uint32_t)func << 8)
                     | (offset & 0xFC);
    
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_read_vendor_id(uint8_t bus, uint8_t device, uint8_t func)
{
    return (uint16_t)(pci_config_read(bus, device, func, 0x00) & 0xFFFF);
}

uint16_t pci_read_device_id(uint8_t bus, uint8_t device, uint8_t func)
{
    return (uint16_t)(pci_config_read(bus, device, func, 0x00) >> 16);
}

uint8_t pci_read_class(uint8_t bus, uint8_t device, uint8_t func)
{
    return (uint8_t)(pci_config_read(bus, device, func, 0x08) >> 24);
}

uint8_t pci_read_subclass(uint8_t bus, uint8_t device, uint8_t func)
{
    return (uint8_t)(pci_config_read(bus, device, func, 0x08) >> 16);
}

uint8_t pci_read_prog_if(uint8_t bus, uint8_t device, uint8_t func)
{
    return (uint8_t)(pci_config_read(bus, device, func, 0x08) >> 8);
}

uint8_t pci_read_header_type(uint8_t bus, uint8_t device, uint8_t func)
{
    return (uint8_t)(pci_config_read(bus, device, func, 0x0C) >> 16);
}

uint32_t pci_read_bar(uint8_t bus, uint8_t device, uint8_t func, uint8_t bar)
{
    if (bar > 5) return 0;
    return pci_config_read(bus, device, func, 0x10 + (bar * 4));
}

uint8_t pci_read_interrupt_line(uint8_t bus, uint8_t device, uint8_t func)
{
    return (uint8_t)(pci_config_read(bus, device, func, 0x3C) & 0xFF);
}

static void pci_scan_function(uint8_t bus, uint8_t device, uint8_t func)
{
    uint16_t vendor_id = pci_read_vendor_id(bus, device, func);
    if (vendor_id == 0xFFFF) return;
    
    if (pci_device_count >= MAX_PCI_DEVICES) return;
    
    pci_device_t *dev = &pci_devices[pci_device_count++];
    
    dev->bus = bus;
    dev->device = device;
    dev->function = func;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_read_device_id(bus, device, func);
    dev->class_code = pci_read_class(bus, device, func);
    dev->subclass = pci_read_subclass(bus, device, func);
    dev->prog_if = pci_read_prog_if(bus, device, func);
    dev->header_type = pci_read_header_type(bus, device, func);
    dev->interrupt_line = pci_read_interrupt_line(bus, device, func);
    
    /* Read BARs */
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_read_bar(bus, device, func, i);
    }
}

static void pci_scan_device(uint8_t bus, uint8_t device)
{
    uint16_t vendor_id = pci_read_vendor_id(bus, device, 0);
    if (vendor_id == 0xFFFF) return;
    
    pci_scan_function(bus, device, 0);
    
    uint8_t header_type = pci_read_header_type(bus, device, 0);
    if (header_type & 0x80) {
        for (uint8_t func = 1; func < 8; func++) {
            if (pci_read_vendor_id(bus, device, func) != 0xFFFF) {
                pci_scan_function(bus, device, func);
            }
        }
    }
}

static void pci_scan_bus(uint8_t bus)
{
    for (uint8_t device = 0; device < 32; device++) {
        pci_scan_device(bus, device);
    }
}

void pci_init(void)
{
    pci_device_count = 0;
    
    if ((pci_config_read(0, 0, 0, 0) & 0xFFFF) == 0xFFFF) {
        return; 
    }
    
    uint8_t header_type = pci_read_header_type(0, 0, 0);
    if ((header_type & 0x80) == 0) {
        pci_scan_bus(0);
    } else {
        for (uint8_t func = 0; func < 8; func++) {
            if (pci_read_vendor_id(0, 0, func) != 0xFFFF) {
                pci_scan_bus(func);
            }
        }
    }
}

uint32_t pci_get_device_count(void)
{
    return pci_device_count;
}

pci_device_t *pci_get_device(uint32_t index)
{
    if (index >= pci_device_count) return NULL;
    return &pci_devices[index];
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

pci_device_t *pci_find_device_by_class(uint8_t class_code, uint8_t subclass)
{
    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code &&
            pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

void pci_enable_bus_mastering(pci_device_t *dev)
{
    uint32_t command = pci_config_read(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 2); 
    pci_config_write(dev->bus, dev->device, dev->function, 0x04, command);
}

void pci_enable_memory_space(pci_device_t *dev)
{
    uint32_t command = pci_config_read(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 1); 
    pci_config_write(dev->bus, dev->device, dev->function, 0x04, command);
}

void pci_enable_io_space(pci_device_t *dev)
{
    uint32_t command = pci_config_read(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 0);
    pci_config_write(dev->bus, dev->device, dev->function, 0x04, command);
}
