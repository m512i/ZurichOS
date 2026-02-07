#ifndef _ACPI_ACPI_H
#define _ACPI_ACPI_H

#include <stdint.h>
#include <stddef.h>

typedef struct acpi_rsdp {
    char signature[8];      
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;    
    uint32_t length;
    uint64_t xsdt_addr;     
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

typedef struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct acpi_rsdt {
    acpi_sdt_header_t header;
    uint32_t tables[];      
} __attribute__((packed)) acpi_rsdt_t;

typedef struct acpi_madt {
    acpi_sdt_header_t header;
    uint32_t lapic_addr;    
    uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

#define MADT_ENTRY_LAPIC        0
#define MADT_ENTRY_IOAPIC       1
#define MADT_ENTRY_ISO          2   
#define MADT_ENTRY_NMI          3
#define MADT_ENTRY_LAPIC_NMI    4
#define MADT_ENTRY_LAPIC_ADDR   5
#define MADT_ENTRY_X2APIC       9

typedef struct acpi_madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_header_t;

typedef struct acpi_madt_lapic {
    acpi_madt_entry_header_t header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;         
} __attribute__((packed)) acpi_madt_lapic_t;

typedef struct acpi_madt_ioapic {
    acpi_madt_entry_header_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;      
} __attribute__((packed)) acpi_madt_ioapic_t;

typedef struct acpi_madt_iso {
    acpi_madt_entry_header_t header;
    uint8_t bus;            
    uint8_t source;         
    uint32_t gsi;           
    uint16_t flags;
} __attribute__((packed)) acpi_madt_iso_t;

int acpi_init(void);
uint32_t acpi_get_lapic_addr(void);
uint32_t acpi_get_ioapic_addr(void);
uint8_t acpi_get_ioapic_id(void);
int acpi_is_available(void);

#endif 
