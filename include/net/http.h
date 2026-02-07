/* HTTP Client Header
 * Simple HTTP/1.1 client
 */

#ifndef _NET_HTTP_H
#define _NET_HTTP_H

#include <stdint.h>

#define HTTP_PORT           80
#define HTTP_MAX_HEADERS    16
#define HTTP_MAX_BODY       4096

#define HTTP_GET    0
#define HTTP_POST   1
#define HTTP_PUT    2
#define HTTP_DELETE 3

#define HTTP_OK             200
#define HTTP_CREATED        201
#define HTTP_NO_CONTENT     204
#define HTTP_MOVED          301
#define HTTP_FOUND          302
#define HTTP_BAD_REQUEST    400
#define HTTP_UNAUTHORIZED   401
#define HTTP_FORBIDDEN      403
#define HTTP_NOT_FOUND      404
#define HTTP_SERVER_ERROR   500

typedef struct http_response {
    int status_code;
    char status_text[32];
    char content_type[64];
    uint32_t content_length;
    char body[HTTP_MAX_BODY];
    uint32_t body_len;
} http_response_t;

void http_init(void);
int http_get(const char *host, const char *path, http_response_t *response);
int http_post(const char *host, const char *path, const char *body, http_response_t *response);

#endif
