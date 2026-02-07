/* ACPI Table Parsing
 * Finds and parses ACPI tables for hardware discovery
 */

#include <kernel/kernel.h>
#include <acpi/acpi.h>
#include <mm/vmm.h>

static acpi_rsdp_t *rsdp = NULL;
static acpi_rsdt_t *rsdt = NULL;
static acpi_madt_t *madt = NULL;
static uint32_t lapic_addr = 0;
static uint32_t ioapic_addr = 0;
static uint8_t ioapic_id = 0;

static int acpi_checksum(void *table, size_t length)
{
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t *)table;
    
    for (size_t i = 0; i < length; i++) {
        sum += ptr[i];
    }
    
    return sum == 0;
}

static acpi_rsdp_t *acpi_find_rsdp_in_region(uint32_t start, uint32_t end)
{
    for (uint32_t addr = start; addr < end; addr += 16) {
        uint8_t *ptr = (uint8_t *)(addr + KERNEL_VMA);
        
        if (ptr[0] == 'R' && ptr[1] == 'S' && ptr[2] == 'D' && ptr[3] == ' ' &&
            ptr[4] == 'P' && ptr[5] == 'T' && ptr[6] == 'R' && ptr[7] == ' ') {
            
            acpi_rsdp_t *rsdp_candidate = (acpi_rsdp_t *)ptr;

            if (acpi_checksum(rsdp_candidate, 20)) {
                return rsdp_candidate;
            }
        }
    }
    
    return NULL;
}

static acpi_rsdp_t *acpi_find_rsdp(void)
{
    acpi_rsdp_t *found = NULL;
    
    uint16_t ebda_segment = *(uint16_t *)(0x40E + KERNEL_VMA);
    uint32_t ebda_addr = (uint32_t)ebda_segment << 4;
    
    if (ebda_addr) {
        found = acpi_find_rsdp_in_region(ebda_addr, ebda_addr + 1024);
        if (found) return found;
    }
    
    found = acpi_find_rsdp_in_region(0xE0000, 0x100000);
    
    return found;
}

static void acpi_ensure_mapped(uint32_t phys_addr, uint32_t size)
{
    uint32_t start_page = phys_addr & 0xFFFFF000;
    uint32_t end_page = (phys_addr + size + 0xFFF) & 0xFFFFF000;
    
    for (uint32_t page = start_page; page < end_page; page += 0x1000) {
        uint32_t virt = page + KERNEL_VMA;
        vmm_map_page(virt, page, PAGE_PRESENT | PAGE_WRITE);
    }
}

static void *acpi_find_table(const char *signature)
{
    if (!rsdt) return NULL;
    
    uint32_t entries = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;
    
    for (uint32_t i = 0; i < entries; i++) {
        uint32_t table_phys = rsdt->tables[i];
        
        if (table_phys >= 0x40000000) {
            continue;
        }
        
        acpi_ensure_mapped(table_phys, sizeof(acpi_sdt_header_t));
        
        acpi_sdt_header_t *header = (acpi_sdt_header_t *)(table_phys + KERNEL_VMA);
        
        if (header->signature[0] == signature[0] &&
            header->signature[1] == signature[1] &&
            header->signature[2] == signature[2] &&
            header->signature[3] == signature[3]) {
            
            acpi_ensure_mapped(table_phys, header->length);
            
            if (acpi_checksum(header, header->length)) {
                return header;
            }
        }
    }
    
    return NULL;
}

static void acpi_parse_madt(acpi_madt_t *madt_table)
{
    if (!madt_table) return;
    
    madt = madt_table;
    lapic_addr = madt->lapic_addr;
    
    uint8_t *ptr = (uint8_t *)madt + sizeof(acpi_madt_t);
    uint8_t *end = (uint8_t *)madt + madt->header.length;
    
    while (ptr < end) {
        acpi_madt_entry_header_t *entry = (acpi_madt_entry_header_t *)ptr;
        
        switch (entry->type) {
            case MADT_ENTRY_LAPIC: {
                acpi_madt_lapic_t *lapic = (acpi_madt_lapic_t *)entry;
                (void)lapic; 
                break;
            }
            
            case MADT_ENTRY_IOAPIC: {
                acpi_madt_ioapic_t *ioapic = (acpi_madt_ioapic_t *)entry;
                if (ioapic_addr == 0) {
                    ioapic_addr = ioapic->ioapic_addr;
                    ioapic_id = ioapic->ioapic_id;
                }
                break;
            }
            
            case MADT_ENTRY_ISO: {
                acpi_madt_iso_t *iso = (acpi_madt_iso_t *)entry;
                (void)iso;
                break;
            }
            
            case MADT_ENTRY_NMI: {
                break;
            }
            
            case MADT_ENTRY_LAPIC_NMI: {
                break;
            }
        }
        
        ptr += entry->length;
    }
}

int acpi_init(void)
{
    rsdp = acpi_find_rsdp();
    if (!rsdp) {
        return -1;
    }
    
    uint32_t rsdt_phys = rsdp->rsdt_addr;
    if (rsdt_phys >= 0x40000000) {
        return -2; 
    }
    
    acpi_ensure_mapped(rsdt_phys, 0x1000);
    
    rsdt = (acpi_rsdt_t *)(rsdt_phys + KERNEL_VMA);
    
    if (!acpi_checksum(rsdt, rsdt->header.length)) {
        return -3; 
    }
    
    acpi_ensure_mapped(rsdt_phys, rsdt->header.length);
    
    acpi_madt_t *madt_table = (acpi_madt_t *)acpi_find_table("APIC");
    if (madt_table) {
        acpi_parse_madt(madt_table);
    }
    
    return 0;
}

uint32_t acpi_get_lapic_addr(void)
{
    return lapic_addr;
}

uint32_t acpi_get_ioapic_addr(void)
{
    return ioapic_addr;
}

uint8_t acpi_get_ioapic_id(void)
{
    return ioapic_id;
}

int acpi_is_available(void)
{
    return rsdp != NULL;
}
