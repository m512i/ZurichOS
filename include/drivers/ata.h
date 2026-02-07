#ifndef _DRIVERS_ATA_H
#define _DRIVERS_ATA_H

#include <stdint.h>
#include <stdbool.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_REG_DATA        0x00    
#define ATA_REG_ERROR       0x01    
#define ATA_REG_FEATURES    0x01    
#define ATA_REG_SECCOUNT    0x02    
#define ATA_REG_LBA_LO      0x03    
#define ATA_REG_LBA_MID     0x04    
#define ATA_REG_LBA_HI      0x05    
#define ATA_REG_DRIVE       0x06    
#define ATA_REG_STATUS      0x07    
#define ATA_REG_COMMAND     0x07    
#define ATA_REG_ALTSTATUS   0x00    
#define ATA_REG_DEVCTRL     0x00    

#define ATA_SR_BSY          0x80    
#define ATA_SR_DRDY         0x40    
#define ATA_SR_DF           0x20    
#define ATA_SR_DSC          0x10    
#define ATA_SR_DRQ          0x08    
#define ATA_SR_CORR         0x04    
#define ATA_SR_IDX          0x02    
#define ATA_SR_ERR          0x01    

#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1

#define ATA_DRIVE_MASTER    0x00
#define ATA_DRIVE_SLAVE     0x01

#define ATA_TYPE_NONE       0
#define ATA_TYPE_ATA        1
#define ATA_TYPE_ATAPI      2

#define ATA_MAX_DRIVES      4

#define ATA_SECTOR_SIZE     512

typedef struct {
    uint8_t present;            
    uint8_t type;               
    uint8_t channel;            
    uint8_t drive;              
    uint16_t io_base;           
    uint16_t ctrl_base;         
    char model[41];             
    char serial[21];            
    char firmware[9];           
    uint32_t sectors;           
    uint64_t sectors48;         
    bool lba48;                 
    uint32_t size_mb;           
} ata_drive_t;

void ata_init(void);

ata_drive_t *ata_get_drive(int index);

int ata_get_drive_count(void);

int ata_read_sectors(int drive, uint32_t lba, uint8_t count, void *buffer);

int ata_write_sectors(int drive, uint32_t lba, uint8_t count, const void *buffer);

int ata_flush(int drive);

void ata_pci_register(void);

#endif 
