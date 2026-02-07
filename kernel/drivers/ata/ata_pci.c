/* ATA/IDE PCI Driver Reg
 * Registers ATA driver with PCI driver framework
 */

#include <drivers/driver.h>
#include <drivers/isolation.h>
#include <drivers/ata.h>
#include <drivers/serial.h>
#include <stddef.h>

#define PCI_CLASS_STORAGE_IDE   0x0101

static const pci_device_id_t ata_pci_ids[] = {
    PCI_DEVICE_CLASS(0x010100, 0xFFFF00),
    PCI_DEVICE_CLASS(0x010180, 0xFFFF80),
    
    /* IDE controllers */
    PCI_DEVICE(0x8086, 0x7010),  /* Intel PIIX3 IDE */
    PCI_DEVICE(0x8086, 0x7111),  /* Intel PIIX4 IDE */
    PCI_DEVICE(0x8086, 0x2820),  /* Intel ICH8 SATA */
    PCI_DEVICE(0x8086, 0x2921),  /* Intel ICH9 SATA */
    PCI_DEVICE(0x8086, 0x1C00),  /* Intel 6 Series SATA */
    PCI_DEVICE(0x1022, 0x7800),  /* AMD Hudson SATA */
    PCI_DEVICE(0x1002, 0x4390),  /* AMD SB7x0 SATA */
    
    PCI_DEVICE_END
};

static int ata_pci_probe(pci_device_t *dev, const pci_device_id_t *id)
{
    (void)id;
    
    serial_puts("[ATA-PCI] Probing IDE controller: ");
    serial_puts("vendor=0x");
    char hex[5];
    for (int i = 3; i >= 0; i--) {
        int nibble = (dev->vendor_id >> (i * 4)) & 0xF;
        hex[3-i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex[4] = '\0';
    serial_puts(hex);
    serial_puts(" device=0x");
    for (int i = 3; i >= 0; i--) {
        int nibble = (dev->device_id >> (i * 4)) & 0xF;
        hex[3-i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    serial_puts(hex);
    serial_puts("\n");
    
    pci_enable_bus_mastering(dev);
    pci_enable_io_space(dev);
    
    /* ATA is already init via legacy ports in ata_init()
     * This driver just claims the PCI device for the framework */
    
    return 0;
}

static void ata_pci_remove(pci_device_t *dev)
{
    (void)dev;
    serial_puts("[ATA-PCI] Removing IDE controller\n");
}

static pci_driver_t ata_pci_driver = {
    .name = "ata-pci",
    .id_table = ata_pci_ids,
    .probe = ata_pci_probe,
    .remove = ata_pci_remove,
    .suspend = NULL,
    .resume = NULL,
    .status = DRIVER_STATUS_UNLOADED,
    .devices_bound = 0,
    .domain = NULL,
    .isolation_level = 1,       
    .io_port_base = 0x1F0,     
    .io_port_count = 8         
};

void ata_pci_register(void)
{
    pci_register_driver(&ata_pci_driver);
    
    if (ata_pci_driver.domain) {
        driver_domain_t *dom = (driver_domain_t *)ata_pci_driver.domain;
        /* Secondary ATA channel: 0x170-0x177 */
        driver_domain_allow_port(dom, 0x170, 8);
        /* Primary control register: 0x3F6 */
        driver_domain_allow_single_port(dom, 0x3F6);
        /* Secondary control register: 0x376 */
        driver_domain_allow_single_port(dom, 0x376);
        serial_puts("[ATA-PCI] Isolation: granted ports 0x1F0-0x1F7, 0x170-0x177, 0x3F6, 0x376\n");
    }
}
