/* HTTP Client
 * Simple HTTP/1.1 client
 */

#include <net/http.h>
#include <net/net.h>
#include <net/dns.h>
#include <net/tcp.h>
#include <net/socket.h>
#include <drivers/serial.h>
#include <string.h>

void http_init(void)
{
    serial_puts("[HTTP] Initialized\n");
}

static int http_parse_response(const char *data, int len, http_response_t *response)
{
    if (len < 12) return -1;
    
    if (strncmp(data, "HTTP/1.", 7) != 0) return -1;
    
    const char *p = data + 9;  
    response->status_code = 0;
    while (*p >= '0' && *p <= '9') {
        response->status_code = response->status_code * 10 + (*p - '0');
        p++;
    }
    
    if (*p == ' ') p++;
    int i = 0;
    while (*p && *p != '\r' && *p != '\n' && i < 31) {
        response->status_text[i++] = *p++;
    }
    response->status_text[i] = '\0';
    
    const char *headers = strstr(data, "\r\n");
    if (!headers) return -1;
    headers += 2;
    
    response->content_length = 0;
    const char *cl = strstr(headers, "Content-Length:");
    if (!cl) cl = strstr(headers, "content-length:");
    if (cl) {
        cl += 15;
        while (*cl == ' ') cl++;
        while (*cl >= '0' && *cl <= '9') {
            response->content_length = response->content_length * 10 + (*cl - '0');
            cl++;
        }
    }
    
    response->content_type[0] = '\0';
    const char *ct = strstr(headers, "Content-Type:");
    if (!ct) ct = strstr(headers, "content-type:");
    if (ct) {
        ct += 13;
        while (*ct == ' ') ct++;
        i = 0;
        while (*ct && *ct != '\r' && *ct != '\n' && *ct != ';' && i < 63) {
            response->content_type[i++] = *ct++;
        }
        response->content_type[i] = '\0';
    }
    
    const char *body = strstr(data, "\r\n\r\n");
    if (body) {
        body += 4;
        int body_len = len - (body - data);
        if (body_len > HTTP_MAX_BODY - 1) body_len = HTTP_MAX_BODY - 1;
        memcpy(response->body, body, body_len);
        response->body[body_len] = '\0';
        response->body_len = body_len;
    }
    
    return 0;
}

int http_get(const char *host, const char *path, http_response_t *response)
{
    if (!host || !path || !response) return -1;
    
    memset(response, 0, sizeof(http_response_t));
    
    uint32_t ip;
    if (dns_resolve(host, &ip) < 0) {
        serial_puts("[HTTP] DNS resolution failed\n");
        return -1;
    }
    
    int sock = socket_create(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        serial_puts("[HTTP] Socket creation failed\n");
        return -1;
    }
    
    sockaddr_in_t addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HTTP_PORT);
    addr.sin_addr = ip;
    
    if (socket_connect(sock, (sockaddr_t *)&addr, sizeof(addr)) < 0) {
        serial_puts("[HTTP] Connect failed\n");
        socket_close(sock);
        return -1;
    }
    
    char request[512];
    int req_len = 0;
    
    const char *method = "GET ";
    while (*method) request[req_len++] = *method++;
    while (*path) request[req_len++] = *path++;
    const char *ver = " HTTP/1.1\r\n";
    while (*ver) request[req_len++] = *ver++;
    
    const char *hdr = "Host: ";
    while (*hdr) request[req_len++] = *hdr++;
    const char *h = host;
    while (*h) request[req_len++] = *h++;
    request[req_len++] = '\r';
    request[req_len++] = '\n';
    
    hdr = "Connection: close\r\n";
    while (*hdr) request[req_len++] = *hdr++;
    
    request[req_len++] = '\r';
    request[req_len++] = '\n';
    request[req_len] = '\0';
    
    if (socket_send(sock, request, req_len, 0) < 0) {
        serial_puts("[HTTP] Send failed\n");
        socket_close(sock);
        return -1;
    }
    
    serial_puts("[HTTP] Request sent\n");
    
    char recv_buf[HTTP_MAX_BODY];
    int total = 0;
    int received;
    
    while (total < HTTP_MAX_BODY - 1) {
        received = socket_recv(sock, recv_buf + total, HTTP_MAX_BODY - 1 - total, 0);
        if (received <= 0) break;
        total += received;
    }
    recv_buf[total] = '\0';
    
    socket_close(sock);
    
    if (total > 0) {
        http_parse_response(recv_buf, total, response);
        serial_puts("[HTTP] Response received\n");
        return 0;
    }
    
    return -1;
}

int http_post(const char *host, const char *path, const char *body, http_response_t *response)
{
    if (!host || !path || !response) return -1;
    
    memset(response, 0, sizeof(http_response_t));
    
    uint32_t ip;
    if (dns_resolve(host, &ip) < 0) {
        return -1;
    }
    
    int sock = socket_create(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    sockaddr_in_t addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HTTP_PORT);
    addr.sin_addr = ip;
    
    if (socket_connect(sock, (sockaddr_t *)&addr, sizeof(addr)) < 0) {
        socket_close(sock);
        return -1;
    }
    
    char request[1024];
    int req_len = 0;
    int body_len = body ? strlen(body) : 0;
    
    const char *method = "POST ";
    while (*method) request[req_len++] = *method++;
    while (*path) request[req_len++] = *path++;
    const char *ver = " HTTP/1.1\r\n";
    while (*ver) request[req_len++] = *ver++;
    
    const char *hdr = "Host: ";
    while (*hdr) request[req_len++] = *hdr++;
    const char *h = host;
    while (*h) request[req_len++] = *h++;
    request[req_len++] = '\r';
    request[req_len++] = '\n';
    
    hdr = "Content-Type: application/x-www-form-urlencoded\r\n";
    while (*hdr) request[req_len++] = *hdr++;
    
    hdr = "Content-Length: ";
    while (*hdr) request[req_len++] = *hdr++;
    
    char len_str[12];
    int li = 0;
    int tmp = body_len;
    if (tmp == 0) len_str[li++] = '0';
    else {
        char rev[12];
        int ri = 0;
        while (tmp > 0) { rev[ri++] = '0' + (tmp % 10); tmp /= 10; }
        while (ri > 0) len_str[li++] = rev[--ri];
    }
    len_str[li] = '\0';
    for (int i = 0; len_str[i]; i++) request[req_len++] = len_str[i];
    
    request[req_len++] = '\r';
    request[req_len++] = '\n';
    
    hdr = "Connection: close\r\n\r\n";
    while (*hdr) request[req_len++] = *hdr++;
    
    if (body) {
        while (*body && req_len < 1023) request[req_len++] = *body++;
    }
    request[req_len] = '\0';
    
    socket_send(sock, request, req_len, 0);
    
    char recv_buf[HTTP_MAX_BODY];
    int total = 0;
    int received;
    
    while (total < HTTP_MAX_BODY - 1) {
        received = socket_recv(sock, recv_buf + total, HTTP_MAX_BODY - 1 - total, 0);
        if (received <= 0) break;
        total += received;
    }
    recv_buf[total] = '\0';
    
    socket_close(sock);
    
    if (total > 0) {
        http_parse_response(recv_buf, total, response);
        return 0;
    }
    
    return -1;
}
