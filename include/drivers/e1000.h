/* Intel E1000 Gigabit Ethernet Driver Header
 * For QEMU's default network card
 */

#ifndef _DRIVERS_E1000_H
#define _DRIVERS_E1000_H

#include <stdint.h>

#define E1000_VENDOR_ID     0x8086
#define E1000_DEVICE_ID     0x100E  /* 82540EM - QEMU default */

#define E1000_CTRL          0x0000  /* Device Control */
#define E1000_STATUS        0x0008  /* Device Status */
#define E1000_EECD          0x0010  /* EEPROM Control */
#define E1000_EERD          0x0014  /* EEPROM Read */
#define E1000_ICR           0x00C0  /* Interrupt Cause Read */
#define E1000_IMS           0x00D0  /* Interrupt Mask Set */
#define E1000_IMC           0x00D8  /* Interrupt Mask Clear */
#define E1000_RCTL          0x0100  /* Receive Control */
#define E1000_TCTL          0x0400  /* Transmit Control */
#define E1000_RDBAL         0x2800  /* Rx Descriptor Base Low */
#define E1000_RDBAH         0x2804  /* Rx Descriptor Base High */
#define E1000_RDLEN         0x2808  /* Rx Descriptor Length */
#define E1000_RDH           0x2810  /* Rx Descriptor Head */
#define E1000_RDT           0x2818  /* Rx Descriptor Tail */
#define E1000_TDBAL         0x3800  /* Tx Descriptor Base Low */
#define E1000_TDBAH         0x3804  /* Tx Descriptor Base High */
#define E1000_TDLEN         0x3808  /* Tx Descriptor Length */
#define E1000_TDH           0x3810  /* Tx Descriptor Head */
#define E1000_TDT           0x3818  /* Tx Descriptor Tail */
#define E1000_RAL           0x5400  /* Receive Address Low */
#define E1000_RAH           0x5404  /* Receive Address High */
#define E1000_MTA           0x5200  /* Multicast Table Array */

#define E1000_CTRL_RST      (1 << 26)   /* Device Reset */
#define E1000_CTRL_SLU      (1 << 6)    /* Set Link Up */
#define E1000_CTRL_ASDE     (1 << 5)    /* Auto-Speed Detection Enable */

#define E1000_RCTL_EN       (1 << 1)    /* Receiver Enable */
#define E1000_RCTL_SBP      (1 << 2)    /* Store Bad Packets */
#define E1000_RCTL_UPE      (1 << 3)    /* Unicast Promiscuous Enable */
#define E1000_RCTL_MPE      (1 << 4)    /* Multicast Promiscuous Enable */
#define E1000_RCTL_LPE      (1 << 5)    /* Long Packet Enable */
#define E1000_RCTL_BAM      (1 << 15)   /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2048 (0 << 16) /* Buffer Size 2048 */
#define E1000_RCTL_SECRC    (1 << 26)   /* Strip Ethernet CRC */

#define E1000_TCTL_EN       (1 << 1)    /* Transmitter Enable */
#define E1000_TCTL_PSP      (1 << 3)    /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT 4           /* Collision Threshold */
#define E1000_TCTL_COLD_SHIFT 12        /* Collision Distance */

#define E1000_ICR_TXDW      (1 << 0)    /* Tx Descriptor Written Back */
#define E1000_ICR_TXQE      (1 << 1)    /* Tx Queue Empty */
#define E1000_ICR_LSC       (1 << 2)    /* Link Status Change */
#define E1000_ICR_RXSEQ     (1 << 3)    /* Rx Sequence Error */
#define E1000_ICR_RXDMT0    (1 << 4)    /* Rx Descriptor Minimum Threshold */
#define E1000_ICR_RXO       (1 << 6)    /* Rx Overrun */
#define E1000_ICR_RXT0      (1 << 7)    /* Rx Timer Interrupt */

#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   32
#define E1000_RX_BUFFER_SIZE 2048

typedef struct e1000_rx_desc {
    uint64_t addr;          
    uint16_t length;        
    uint16_t checksum;      
    uint8_t status;         
    uint8_t errors;         
    uint16_t special;       
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct e1000_tx_desc {
    uint64_t addr;          
    uint16_t length;        
    uint8_t cso;            
    uint8_t cmd;            
    uint8_t status;
    uint8_t css;            
    uint16_t special;       
} __attribute__((packed)) e1000_tx_desc_t;

#define E1000_TXD_CMD_EOP   (1 << 0)
#define E1000_TXD_CMD_IFCS  (1 << 1)
#define E1000_TXD_CMD_RS    (1 << 3)

#define E1000_RXD_STAT_DD   (1 << 0)
#define E1000_RXD_STAT_EOP  (1 << 1)

#define E1000_TXD_STAT_DD   (1 << 0)

int e1000_init(void);
int e1000_send(const void *data, uint16_t length);
int e1000_receive(void *buffer, uint16_t max_length);
void e1000_get_mac(uint8_t *mac);
void e1000_irq_handler(void);

#endif  
