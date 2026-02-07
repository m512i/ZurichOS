/* PCI Driver Framework
 * Driver registration, matching, and probe system
 */

#include <drivers/driver.h>
#include <drivers/isolation.h>
#include <drivers/pci.h>
#include <drivers/serial.h>
#include <string.h>

static pci_driver_t *registered_drivers[MAX_DRIVERS];
static uint32_t driver_count = 0;
static pci_binding_t device_bindings[64];
static uint32_t binding_count = 0;

static int match_id(const pci_device_id_t *id, pci_device_t *dev)
{
    if (id->vendor_id != PCI_ANY_ID && id->vendor_id != dev->vendor_id) {
        return 0;
    }
    
    if (id->device_id != PCI_ANY_ID && id->device_id != dev->device_id) {
        return 0;
    }
    
    if (id->class_mask != 0) {
        uint32_t dev_class = ((uint32_t)dev->class_code << 16) |
                             ((uint32_t)dev->subclass << 8) |
                             dev->prog_if;
        if ((dev_class & id->class_mask) != (id->class_code & id->class_mask)) {
            return 0;
        }
    }
    
    return 1;
}

const pci_device_id_t *pci_match_device(pci_driver_t *driver, pci_device_t *dev)
{
    if (!driver || !driver->id_table || !dev) {
        return NULL;
    }
    
    const pci_device_id_t *id = driver->id_table;
    
    while (id->vendor_id != 0 || id->device_id != 0 || 
           id->class_code != 0 || id->class_mask != 0) {
        if (match_id(id, dev)) {
            return id;
        }
        id++;
    }
    
    return NULL;
}

int pci_register_driver(pci_driver_t *driver)
{
    if (!driver || !driver->name) {
        return -1;
    }
    
    if (driver_count >= MAX_DRIVERS) {
        serial_puts("[DRIVER] Max drivers reached\n");
        return -2;
    }
    
    for (uint32_t i = 0; i < driver_count; i++) {
        if (registered_drivers[i] == driver) {
            return -3;
        }
    }
    
    registered_drivers[driver_count++] = driver;
    driver->status = DRIVER_STATUS_LOADED;
    driver->devices_bound = 0;
    
    if (driver->isolation_level > 0 && !driver->domain) {
        driver_domain_t *dom = driver_domain_create(driver->name, driver->isolation_level);
        if (dom) {
            driver->domain = (struct driver_domain *)dom;
            if (driver->io_port_count > 0) {
                driver_domain_allow_port(dom, driver->io_port_base, driver->io_port_count);
            }
            serial_puts("[DRIVER] Isolation domain created for: ");
            serial_puts(driver->name);
            serial_puts("\n");
        }
    }
    
    serial_puts("[DRIVER] Registered: ");
    serial_puts(driver->name);
    serial_puts("\n");
    
    pci_driver_probe_devices(driver);
    
    return 0;
}

void pci_unregister_driver(pci_driver_t *driver)
{
    if (!driver) return;
    
    for (uint32_t i = 0; i < binding_count; ) {
        if (device_bindings[i].driver == driver) {
            if (driver->remove) {
                driver->remove(device_bindings[i].device);
            }
            
            for (uint32_t j = i; j < binding_count - 1; j++) {
                device_bindings[j] = device_bindings[j + 1];
            }
            binding_count--;
        } else {
            i++;
        }
    }
    
    for (uint32_t i = 0; i < driver_count; i++) {
        if (registered_drivers[i] == driver) {
            for (uint32_t j = i; j < driver_count - 1; j++) {
                registered_drivers[j] = registered_drivers[j + 1];
            }
            driver_count--;
            break;
        }
    }
    
    if (driver->domain) {
        driver_domain_destroy((driver_domain_t *)driver->domain);
        driver->domain = NULL;
    }
    
    driver->status = DRIVER_STATUS_UNLOADED;
    driver->devices_bound = 0;
    
    serial_puts("[DRIVER] Unregistered: ");
    serial_puts(driver->name);
    serial_puts("\n");
}

pci_driver_t *pci_get_driver(const char *name)
{
    if (!name) return NULL;
    
    for (uint32_t i = 0; i < driver_count; i++) {
        if (strcmp(registered_drivers[i]->name, name) == 0) {
            return registered_drivers[i];
        }
    }
    
    return NULL;
}

uint32_t pci_get_driver_count(void)
{
    return driver_count;
}

pci_driver_t *pci_get_driver_by_index(uint32_t index)
{
    if (index >= driver_count) return NULL;
    return registered_drivers[index];
}

int pci_driver_probe_devices(pci_driver_t *driver)
{
    if (!driver || !driver->probe) {
        return -1;
    }
    
    int probed = 0;
    uint32_t dev_count = pci_get_device_count();
    
    for (uint32_t i = 0; i < dev_count; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev) continue;
        
        if (pci_get_device_driver(dev) != NULL) {
            continue;
        }
        
        const pci_device_id_t *id = pci_match_device(driver, dev);
        if (id) {
            serial_puts("[DRIVER] ");
            serial_puts(driver->name);
            serial_puts(" probing ");
            /* bus:dev.func */
            serial_puts("device\n");
            
            int result = driver->probe(dev, id);
            
            if (result == 0) {
                if (binding_count < 64) {
                    device_bindings[binding_count].device = dev;
                    device_bindings[binding_count].driver = driver;
                    binding_count++;
                    driver->devices_bound++;
                    driver->status = DRIVER_STATUS_ACTIVE;
                    probed++;
                    
                    serial_puts("[DRIVER] ");
                    serial_puts(driver->name);
                    serial_puts(" bound to device\n");
                }
            }
        }
    }
    
    return probed;
}

void pci_probe_all_drivers(void)
{
    serial_puts("[DRIVER] Probing all drivers...\n");
    
    for (uint32_t i = 0; i < driver_count; i++) {
        pci_driver_probe_devices(registered_drivers[i]);
    }
}

pci_driver_t *pci_get_device_driver(pci_device_t *dev)
{
    if (!dev) return NULL;
    
    for (uint32_t i = 0; i < binding_count; i++) {
        if (device_bindings[i].device == dev) {
            return device_bindings[i].driver;
        }
    }
    
    return NULL;
}

uint32_t pci_get_binding_count(void)
{
    return binding_count;
}

pci_binding_t *pci_get_binding(uint32_t index)
{
    if (index >= binding_count) return NULL;
    return &device_bindings[index];
}
