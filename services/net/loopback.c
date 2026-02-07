/* Loopback Interface
 * 127.0.0.1 support
 */

#include <net/loopback.h>
#include <net/net.h>
#include <net/ip.h>
#include <drivers/serial.h>
#include <string.h>

#define LOOPBACK_BUFFER_SIZE 4096

static netif_t loopback_if;
static uint8_t loopback_buffer[LOOPBACK_BUFFER_SIZE];
static uint16_t loopback_head = 0;
static uint16_t loopback_tail = 0;
static uint16_t loopback_count = 0;

static int loopback_if_send(const void *data, uint16_t len);
static int loopback_if_receive(void *data, uint16_t max_len);

void loopback_init(void)
{
    memset(&loopback_if, 0, sizeof(loopback_if));
    strcpy(loopback_if.name, "lo");
    loopback_if.ip = LOOPBACK_IP;
    loopback_if.netmask = LOOPBACK_MASK;
    loopback_if.gateway = 0;
    loopback_if.send = loopback_if_send;
    loopback_if.receive = loopback_if_receive;
    
    memset(loopback_if.mac, 0, 6);
    
    loopback_head = 0;
    loopback_tail = 0;
    loopback_count = 0;
    
    serial_puts("[LOOPBACK] Initialized (127.0.0.1)\n");
}

static int loopback_if_send(const void *data, uint16_t len)
{
    if (len > LOOPBACK_BUFFER_SIZE - loopback_count) {
        return -1; 
    }
    
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++) {
        loopback_buffer[loopback_head] = src[i];
        loopback_head = (loopback_head + 1) % LOOPBACK_BUFFER_SIZE;
    }
    loopback_count += len;
    
    return len;
}

static int loopback_if_receive(void *data, uint16_t max_len)
{
    if (loopback_count == 0) {
        return 0;
    }
    
    uint16_t to_read = loopback_count;
    if (to_read > max_len) to_read = max_len;
    
    uint8_t *dst = (uint8_t *)data;
    for (uint16_t i = 0; i < to_read; i++) {
        dst[i] = loopback_buffer[loopback_tail];
        loopback_tail = (loopback_tail + 1) % LOOPBACK_BUFFER_SIZE;
    }
    loopback_count -= to_read;
    
    return to_read;
}

int loopback_send(const void *data, uint16_t len)
{
    return loopback_if_send(data, len);
}

int loopback_receive(void *data, uint16_t max_len)
{
    return loopback_if_receive(data, max_len);
}

int is_loopback_address(uint32_t ip)
{
    return (ip & 0xFF000000) == 0x7F000000;
}

netif_t *loopback_get_interface(void)
{
    return &loopback_if;
}
