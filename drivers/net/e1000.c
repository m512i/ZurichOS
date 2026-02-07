/* Intel E1000 Gigabit Ethernet Driver
 * Ring 1 driver for QEMU's default network card
 */

#include <drivers/e1000.h>
#include <drivers/pci.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <drivers/serial.h>
#include <string.h>

static uint32_t e1000_mmio_base = 0;
static uint8_t e1000_mac[6];
static int e1000_initialized = 0;

static e1000_rx_desc_t *rx_descs;
static e1000_tx_desc_t *tx_descs;
static uint32_t rx_descs_phys;
static uint32_t tx_descs_phys;

static uint8_t *rx_buffers[E1000_NUM_RX_DESC];
static uint32_t rx_buffers_phys[E1000_NUM_RX_DESC];

static uint16_t rx_cur = 0;
static uint16_t tx_cur = 0;

static inline uint32_t e1000_read(uint32_t reg)
{
    return *(volatile uint32_t *)(e1000_mmio_base + reg);
}

static inline void e1000_write(uint32_t reg, uint32_t value)
{
    *(volatile uint32_t *)(e1000_mmio_base + reg) = value;
}

static uint16_t e1000_eeprom_read(uint8_t addr)
{
    uint32_t val;
    
    e1000_write(E1000_EERD, (1) | ((uint32_t)addr << 8));
    
    while (!((val = e1000_read(E1000_EERD)) & (1 << 4)));
    
    return (uint16_t)((val >> 16) & 0xFFFF);
}

static void e1000_read_mac(void)
{
    uint16_t mac01 = e1000_eeprom_read(0);
    uint16_t mac23 = e1000_eeprom_read(1);
    uint16_t mac45 = e1000_eeprom_read(2);
    
    e1000_mac[0] = mac01 & 0xFF;
    e1000_mac[1] = (mac01 >> 8) & 0xFF;
    e1000_mac[2] = mac23 & 0xFF;
    e1000_mac[3] = (mac23 >> 8) & 0xFF;
    e1000_mac[4] = mac45 & 0xFF;
    e1000_mac[5] = (mac45 >> 8) & 0xFF;
    
    serial_puts("[E1000] MAC: ");
    char hex[3];
    for (int i = 0; i < 6; i++) {
        hex[0] = "0123456789ABCDEF"[(e1000_mac[i] >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[e1000_mac[i] & 0xF];
        hex[2] = '\0';
        serial_puts(hex);
        if (i < 5) serial_puts(":");
    }
    serial_puts("\n");
}

static void e1000_init_rx(void)
{
    uint32_t rx_ring_size = sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC;
    rx_descs_phys = pmm_alloc_frame();
    rx_descs = (e1000_rx_desc_t *)(rx_descs_phys + 0xC0000000);  
    vmm_map_page((uint32_t)rx_descs & ~0xFFF, rx_descs_phys, PAGE_PRESENT | PAGE_WRITE);
    
    memset(rx_descs, 0, rx_ring_size);
    
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_buffers_phys[i] = pmm_alloc_frame();
        rx_buffers[i] = (uint8_t *)(rx_buffers_phys[i] + 0xC0000000);
        vmm_map_page((uint32_t)rx_buffers[i] & ~0xFFF, rx_buffers_phys[i], PAGE_PRESENT | PAGE_WRITE);
        
        rx_descs[i].addr = rx_buffers_phys[i];
        rx_descs[i].status = 0;
    }
    
    e1000_write(E1000_RDBAL, rx_descs_phys);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, rx_ring_size);
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);
    
    e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | 
                            E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC);
    
    serial_puts("[E1000] Receive initialized\n");
}

static void e1000_init_tx(void)
{
    uint32_t tx_ring_size = sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC;
    tx_descs_phys = pmm_alloc_frame();
    tx_descs = (e1000_tx_desc_t *)(tx_descs_phys + 0xC0000000);
    vmm_map_page((uint32_t)tx_descs & ~0xFFF, tx_descs_phys, PAGE_PRESENT | PAGE_WRITE);
    
    memset(tx_descs, 0, tx_ring_size);
    
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].status = E1000_TXD_STAT_DD;  
    }
    
    e1000_write(E1000_TDBAL, tx_descs_phys);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, tx_ring_size);
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    
    e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
                            (15 << E1000_TCTL_CT_SHIFT) |
                            (64 << E1000_TCTL_COLD_SHIFT));
    
    serial_puts("[E1000] Transmit initialized\n");
}

int e1000_init(void)
{
    serial_puts("[E1000] Initializing...\n");
    
    pci_device_t *dev = pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID);
    if (!dev) {
        serial_puts("[E1000] Device not found\n");
        return -1;
    }
    
    serial_puts("[E1000] Found device\n");
    
    e1000_mmio_base = dev->bar[0] & ~0xF;
    
    for (uint32_t i = 0; i < 0x20000; i += 0x1000) {
        vmm_map_page(e1000_mmio_base + i, e1000_mmio_base + i, 
                     PAGE_PRESENT | PAGE_WRITE | PAGE_PCD);
    }
    
    serial_puts("[E1000] MMIO base: 0x");
    char hex[9];
    for (int k = 7; k >= 0; k--) {
        int nibble = (e1000_mmio_base >> (k * 4)) & 0xF;
        hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex[8] = '\0';
    serial_puts(hex);
    serial_puts("\n");
    
    pci_enable_bus_mastering(dev);
    pci_enable_memory_space(dev);
    
    e1000_write(E1000_CTRL, E1000_CTRL_RST);
    for (volatile int i = 0; i < 100000; i++);
    
    e1000_read_mac();
    
    e1000_write(E1000_CTRL, E1000_CTRL_SLU | E1000_CTRL_ASDE);
    
    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_MTA + i * 4, 0);
    }
    
    e1000_init_rx();
    
    e1000_init_tx();
    
    e1000_write(E1000_IMS, E1000_ICR_RXT0 | E1000_ICR_LSC);
    
    e1000_initialized = 1;
    serial_puts("[E1000] Initialization complete\n");
    
    return 0;
}

int e1000_send(const void *data, uint16_t length)
{
    if (!e1000_initialized) return -1;
    if (length > E1000_RX_BUFFER_SIZE) return -2;
    
    while (!(tx_descs[tx_cur].status & E1000_TXD_STAT_DD));
    
    uint32_t buf_phys = pmm_alloc_frame();
    uint8_t *buf = (uint8_t *)(buf_phys + 0xC0000000);
    vmm_map_page((uint32_t)buf & ~0xFFF, buf_phys, PAGE_PRESENT | PAGE_WRITE);
    
    memcpy(buf, data, length);
    
    tx_descs[tx_cur].addr = buf_phys;
    tx_descs[tx_cur].length = length;
    tx_descs[tx_cur].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    tx_descs[tx_cur].status = 0;
    
    uint16_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    
    e1000_write(E1000_TDT, tx_cur);
    
    while (!(tx_descs[old_cur].status & E1000_TXD_STAT_DD));
    
    vmm_unmap_page((uint32_t)buf & ~0xFFF);
    pmm_free_frame(buf_phys);
    
    return length;
}

int e1000_receive(void *buffer, uint16_t max_length)
{
    if (!e1000_initialized) return -1;
    
    if (!(rx_descs[rx_cur].status & E1000_RXD_STAT_DD)) {
        return 0;  
    }
    
    uint16_t length = rx_descs[rx_cur].length;
    if (length > max_length) length = max_length;
    
    memcpy(buffer, rx_buffers[rx_cur], length);
    
    rx_descs[rx_cur].status = 0;
    
    uint16_t old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    
    e1000_write(E1000_RDT, old_cur);
    
    return length;
}

void e1000_get_mac(uint8_t *mac)
{
    memcpy(mac, e1000_mac, 6);
}

void e1000_irq_handler(void)
{
    uint32_t icr = e1000_read(E1000_ICR);
    
    if (icr & E1000_ICR_RXT0) {
        /* Packet received - handled by polling for now */
    }
    
    if (icr & E1000_ICR_LSC) {
        serial_puts("[E1000] Link status changed\n");
    }
}
