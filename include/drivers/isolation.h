#ifndef _DRIVERS_ISOLATION_H
#define _DRIVERS_ISOLATION_H

#include <stdint.h>
#include <arch/x86/gdt.h>

/* I/O Permission Bitmap - 8KB covers all 65536 ports (1 bit per port) */
#define IOPB_SIZE           8192
#define IOPB_ALL_PORTS      65536

/* Driver isolation levels */
#define DRIVER_ISOLATION_NONE       0   /* Ring 0 - full kernel access */
#define DRIVER_ISOLATION_RING1      1   /* Ring 1 - restricted I/O */

/* Driver domain - tracks isolation state for a driver */
#define MAX_DRIVER_DOMAINS  16
#define DRIVER_STACK_SIZE   8192

typedef struct driver_domain {
    int id;
    const char *name;
    int isolation_level;
    int active;

    uint8_t iopb[IOPB_SIZE];

    uint32_t stack_base;
    uint32_t stack_top;

    uint32_t kernel_calls;
    uint32_t io_violations;
    uint32_t total_io_ops;
} driver_domain_t;

#define DRIVER_SVC_ALLOC_MEM        0x01
#define DRIVER_SVC_FREE_MEM         0x02
#define DRIVER_SVC_MAP_MMIO         0x03
#define DRIVER_SVC_REGISTER_IRQ     0x04
#define DRIVER_SVC_UNREGISTER_IRQ   0x05
#define DRIVER_SVC_DMA_ALLOC        0x06
#define DRIVER_SVC_DMA_FREE         0x07
#define DRIVER_SVC_LOG              0x08
#define DRIVER_SVC_PORT_IN          0x09
#define DRIVER_SVC_PORT_OUT         0x0A
#define DRIVER_SVC_PCI_READ         0x0B
#define DRIVER_SVC_PCI_WRITE        0x0C

#define DRIVER_INT_SERVICE  0x81    
#define DRIVER_INT_RETURN   0x82    

typedef struct ring1_context {
    uint32_t esp;       
    uint32_t ebp;       
    uint32_t eip;       
    int return_value;   
    int valid;          
} ring1_context_t;

void driver_isolation_init(void);
driver_domain_t *driver_domain_create(const char *name, int isolation_level);
void driver_domain_destroy(driver_domain_t *domain);

void driver_domain_allow_port(driver_domain_t *domain, uint16_t port_start, uint16_t port_count);
void driver_domain_deny_port(driver_domain_t *domain, uint16_t port_start, uint16_t port_count);
void driver_domain_allow_single_port(driver_domain_t *domain, uint16_t port);

void driver_domain_activate(driver_domain_t *domain);
void driver_domain_deactivate(void);

int driver_domain_exec(driver_domain_t *domain, int (*func)(void *arg), void *arg);

uint32_t driver_kernel_service(uint32_t service_id, uint32_t arg1, uint32_t arg2, uint32_t arg3);

driver_domain_t *driver_domain_get(int id);
driver_domain_t *driver_domain_find(const char *name);

uint32_t driver_domain_count(void);
driver_domain_t *driver_domain_get_by_index(uint32_t index);

void tss_set_ring1_stack(uint32_t esp1, uint16_t ss1);
driver_domain_t *driver_domain_current(void);

extern void ring1_enter(uint32_t ring1_cs, uint32_t ring1_ds,
                        uint32_t ring1_esp, uint32_t ring1_eip);

void driver_isolation_install_handlers(void);

#endif
