/* ATA/IDE Disk Driver
 * PIO mode disk access
 */

#include <drivers/ata.h>
#include <kernel/kernel.h>
#include <drivers/serial.h>
#include <string.h>

static ata_drive_t drives[ATA_MAX_DRIVES];
static int drive_count = 0;

static const struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    const char *name;
} channels[2] = {
    { ATA_PRIMARY_IO,   ATA_PRIMARY_CTRL,   "Primary" },
    { ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, "Secondary" }
};

static int ata_wait(uint16_t io_base, int timeout_ms)
{
    for (int i = 0; i < timeout_ms * 100; i++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return 0;
        }
        for (volatile int j = 0; j < 100; j++);
    }
    return -1;
}

static int ata_wait_drq(uint16_t io_base, int timeout_ms)
{
    for (int i = 0; i < timeout_ms * 100; i++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) {
            return -1;
        }
        if (status & ATA_SR_DF) {
            return -2;
        }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return 0;
        }
        for (volatile int j = 0; j < 100; j++);
    }
    return -3;
}

static void ata_select_drive(uint16_t io_base, int drive, int lba_mode)
{
    uint8_t val = 0xA0;
    if (drive == ATA_DRIVE_SLAVE) {
        val |= 0x10;
    }
    if (lba_mode) {
        val |= 0x40;
    }
    outb(io_base + ATA_REG_DRIVE, val);
    
    for (int i = 0; i < 4; i++) {
        inb(io_base + ATA_REG_STATUS);
    }
}

static void ata_copy_string(char *dest, uint16_t *src, int words)
{
    for (int i = 0; i < words; i++) {
        dest[i * 2] = (src[i] >> 8) & 0xFF;
        dest[i * 2 + 1] = src[i] & 0xFF;
    }
    dest[words * 2] = '\0';
    
    for (int i = words * 2 - 1; i >= 0 && dest[i] == ' '; i--) {
        dest[i] = '\0';
    }
}

static int ata_identify(int channel, int drive_num, ata_drive_t *drive)
{
    uint16_t io_base = channels[channel].io_base;
    uint16_t ctrl_base = channels[channel].ctrl_base;
    uint16_t identify_data[256];
    
    ata_select_drive(io_base, drive_num, 0);
    
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LO, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HI, 0);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    uint8_t status = inb(io_base + ATA_REG_STATUS);
    if (status == 0) {
        return -1;  
    }
    
    if (ata_wait(io_base, 1000) < 0) {
        return -2;
    }
    
    uint8_t lba_mid = inb(io_base + ATA_REG_LBA_MID);
    uint8_t lba_hi = inb(io_base + ATA_REG_LBA_HI);
    
    if (lba_mid == 0x14 && lba_hi == 0xEB) {
        outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
        if (ata_wait(io_base, 1000) < 0) {
            return -3;
        }
        drive->type = ATA_TYPE_ATAPI;
    } else if (lba_mid == 0 && lba_hi == 0) {
        drive->type = ATA_TYPE_ATA;
    } else {
        return -4;  
    }
    
    if (ata_wait_drq(io_base, 1000) < 0) {
        return -5;
    }
    
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(io_base + ATA_REG_DATA);
    }
    
    drive->present = 1;
    drive->channel = channel;
    drive->drive = drive_num;
    drive->io_base = io_base;
    drive->ctrl_base = ctrl_base;
    
    ata_copy_string(drive->serial, &identify_data[10], 10);
    ata_copy_string(drive->firmware, &identify_data[23], 4);
    ata_copy_string(drive->model, &identify_data[27], 20);
    
    /* Check for LBA48 support */
    if (identify_data[83] & (1 << 10)) {
        drive->lba48 = true;
        drive->sectors48 = ((uint64_t)identify_data[103] << 48) |
                          ((uint64_t)identify_data[102] << 32) |
                          ((uint64_t)identify_data[101] << 16) |
                          identify_data[100];
        drive->sectors = (drive->sectors48 > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)drive->sectors48;
    } else {
        drive->lba48 = false;
        drive->sectors = ((uint32_t)identify_data[61] << 16) | identify_data[60];
        drive->sectors48 = drive->sectors;
    }
    
    drive->size_mb = (uint32_t)(drive->sectors48 * ATA_SECTOR_SIZE / (1024 * 1024));
    
    return 0;
}

void ata_init(void)
{
    memset(drives, 0, sizeof(drives));
    drive_count = 0;
    
    serial_puts("[ATA] Scanning for drives...\n");
    
    for (int channel = 0; channel < 2; channel++) {
        for (int drive_num = 0; drive_num < 2; drive_num++) {
            int idx = channel * 2 + drive_num;
            
            if (ata_identify(channel, drive_num, &drives[idx]) == 0) {
                drive_count++;
                serial_puts("[ATA] Found: ");
                serial_puts(drives[idx].model);
                serial_puts("\n");
            }
        }
    }
    
    serial_puts("[ATA] Total drives: ");
    char buf[2] = { '0' + drive_count, '\0' };
    serial_puts(buf);
    serial_puts("\n");
}

ata_drive_t *ata_get_drive(int index)
{
    if (index < 0 || index >= ATA_MAX_DRIVES) {
        return NULL;
    }
    if (!drives[index].present) {
        return NULL;
    }
    return &drives[index];
}

int ata_get_drive_count(void)
{
    return drive_count;
}

int ata_read_sectors(int drive_idx, uint32_t lba, uint8_t count, void *buffer)
{
    ata_drive_t *drive = ata_get_drive(drive_idx);
    if (!drive) {
        return -1;
    }
    
    if (drive->type != ATA_TYPE_ATA) {
        return -2;
    }
    
    uint16_t io_base = drive->io_base;
    uint16_t *buf = (uint16_t *)buffer;
    
    if (ata_wait(io_base, 1000) < 0) {
        return -3;
    }
    
    uint8_t drive_sel = 0xE0 | (drive->drive << 4) | ((lba >> 24) & 0x0F);
    outb(io_base + ATA_REG_DRIVE, drive_sel);
    
    for (int i = 0; i < 4; i++) {
        inb(io_base + ATA_REG_STATUS);
    }
    
    outb(io_base + ATA_REG_SECCOUNT, count);
    outb(io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq(io_base, 1000) < 0) {
            return -4;
        }
        
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(io_base + ATA_REG_DATA);
        }
    }
    
    return count;
}

int ata_write_sectors(int drive_idx, uint32_t lba, uint8_t count, const void *buffer)
{
    ata_drive_t *drive = ata_get_drive(drive_idx);
    if (!drive) {
        return -1;
    }
    
    if (drive->type != ATA_TYPE_ATA) {
        return -2;
    }
    
    uint16_t io_base = drive->io_base;
    const uint16_t *buf = (const uint16_t *)buffer;
    
    if (ata_wait(io_base, 1000) < 0) {
        return -3;
    }
    
    uint8_t drive_sel = 0xE0 | (drive->drive << 4) | ((lba >> 24) & 0x0F);
    outb(io_base + ATA_REG_DRIVE, drive_sel);
    
    for (int i = 0; i < 4; i++) {
        inb(io_base + ATA_REG_STATUS);
    }
    
    outb(io_base + ATA_REG_SECCOUNT, count);
    outb(io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq(io_base, 1000) < 0) {
            return -4;
        }
        
        for (int i = 0; i < 256; i++) {
            outw(io_base + ATA_REG_DATA, buf[s * 256 + i]);
        }
    }
    ata_flush(drive_idx);
    
    return count;
}

int ata_flush(int drive_idx)
{
    ata_drive_t *drive = ata_get_drive(drive_idx);
    if (!drive) {
        return -1;
    }
    
    uint16_t io_base = drive->io_base;
    
    uint8_t drive_sel = 0xE0 | (drive->drive << 4);
    outb(io_base + ATA_REG_DRIVE, drive_sel);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    
    return ata_wait(io_base, 5000);
}
