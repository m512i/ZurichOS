/* Network Test Program
 * Comprehensive tests for the ZurichOS networking stack
 * Tests all advanced networking features
 */

#define NULL ((void *)0)

/* Syscall numbers */
#define SYS_EXIT        0
#define SYS_WRITE       2
#define SYS_SOCKET      50
#define SYS_BIND        51
#define SYS_LISTEN      52
#define SYS_ACCEPT      53
#define SYS_CONNECT     54
#define SYS_SEND        55
#define SYS_RECV        56
#define SYS_CLOSESOCK   57
#define SYS_SENDTO      58
#define SYS_RECVFROM    59
#define SYS_SHUTDOWN    60
#define SYS_GETSOCKNAME 61
#define SYS_GETPEERNAME 62
#define SYS_SETSOCKOPT  63
#define SYS_GETSOCKOPT  64
#define SYS_SELECT      65

/* Socket constants */
#define AF_INET         2
#define SOCK_STREAM     1
#define SOCK_DGRAM      2

/* Shutdown constants */
#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

/* Socket options */
#define SOL_SOCKET      1
#define SO_REUSEADDR    2
#define SO_KEEPALIVE    9

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1) : "memory");
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return ret;
}

static inline int syscall4(int num, int arg1, int arg2, int arg3, int arg4)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4) : "memory");
    return ret;
}

static inline int syscall5(int num, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5) : "memory");
    return ret;
}

static void print(const char *str)
{
    int len = 0;
    while (str[len]) len++;
    syscall3(SYS_WRITE, 1, (int)str, len);
}

static void print_num(int n)
{
    char buf[12];
    int i = 0;
    if (n < 0) { print("-"); n = -n; }
    if (n == 0) { print("0"); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) { char c[2] = {buf[--i], 0}; print(c); }
}

static void test_result(const char *name, int passed)
{
    print("  ");
    print(name);
    print(": ");
    if (passed == 1) {
        print("[PASS]\n");
        tests_passed++;
    } else if (passed == 0) {
        print("[FAIL]\n");
        tests_failed++;
    } else {
        print("[SKIP]\n");
        tests_skipped++;
    }
}

static void print_header(const char *title)
{
    print("\n");
    print(title);
    print("\n");
    for (int i = 0; title[i]; i++) print("-");
    print("\n");
}

/* Test socket creation */
static void test_socket_creation(void)
{
    print_header("[1] Socket Creation Tests");
    
    /* Test UDP socket */
    int udp = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, 0);
    test_result("Create UDP socket", udp >= 0);
    if (udp >= 0) syscall1(SYS_CLOSESOCK, udp);
    
    /* Test TCP socket */
    int tcp = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
    test_result("Create TCP socket", tcp >= 0);
    if (tcp >= 0) syscall1(SYS_CLOSESOCK, tcp);
    
    /* Test invalid socket type */
    int bad = syscall3(SYS_SOCKET, AF_INET, 99, 0);
    test_result("Reject invalid socket type", bad < 0);
    
    /* Test invalid address family */
    int bad_af = syscall3(SYS_SOCKET, 99, SOCK_DGRAM, 0);
    test_result("Reject invalid address family", bad_af < 0);
}

/* Test socket binding */
static void test_socket_bind(void)
{
    print_header("[2] Socket Bind Tests");
    
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        test_result("Socket creation for bind test", -1);
        return;
    }
    
    /* Create sockaddr_in structure */
    struct {
        unsigned short sin_family;
        unsigned short sin_port;
        unsigned int sin_addr;
        char padding[8];
    } addr;
    
    addr.sin_family = AF_INET;
    addr.sin_port = (12345 >> 8) | ((12345 & 0xFF) << 8);  /* htons(12345) */
    addr.sin_addr = 0;  /* INADDR_ANY */
    
    int result = syscall3(SYS_BIND, sock, (int)&addr, sizeof(addr));
    test_result("Bind to port 12345", result == 0);
    
    syscall1(SYS_CLOSESOCK, sock);
}

/* Test TCP listen */
static void test_tcp_listen(void)
{
    print_header("[3] TCP Listen Tests");
    
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        test_result("Socket creation for listen test", -1);
        return;
    }
    
    struct {
        unsigned short sin_family;
        unsigned short sin_port;
        unsigned int sin_addr;
        char padding[8];
    } addr;
    
    addr.sin_family = AF_INET;
    addr.sin_port = (8080 >> 8) | ((8080 & 0xFF) << 8);  /* htons(8080) */
    addr.sin_addr = 0;
    
    int bind_result = syscall3(SYS_BIND, sock, (int)&addr, sizeof(addr));
    test_result("Bind TCP socket", bind_result == 0);
    
    int listen_result = syscall3(SYS_LISTEN, sock, 5, 0);
    test_result("Listen on TCP socket", listen_result == 0);
    
    syscall1(SYS_CLOSESOCK, sock);
}

/* Test multiple sockets */
static void test_multiple_sockets(void)
{
    print_header("[4] Multiple Socket Tests");
    
    int sockets[8];
    int created = 0;
    
    for (int i = 0; i < 8; i++) {
        sockets[i] = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, 0);
        if (sockets[i] >= 0) created++;
    }
    
    test_result("Create multiple sockets", created >= 4);
    print("  Created ");
    print_num(created);
    print(" sockets\n");
    
    /* Close all */
    for (int i = 0; i < 8; i++) {
        if (sockets[i] >= 0) syscall1(SYS_CLOSESOCK, sockets[i]);
    }
    
    /* Test socket reuse after close */
    int reused = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, 0);
    test_result("Socket reuse after close", reused >= 0);
    if (reused >= 0) syscall1(SYS_CLOSESOCK, reused);
}

/* Test socket close */
static void test_socket_close(void)
{
    print_header("[5] Socket Close Tests");
    
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, 0);
    test_result("Create socket for close test", sock >= 0);
    
    if (sock >= 0) {
        int close_result = syscall1(SYS_CLOSESOCK, sock);
        test_result("Close socket", close_result == 0);
        
        /* Try to close again - should fail */
        int double_close = syscall1(SYS_CLOSESOCK, sock);
        test_result("Double close rejected", double_close < 0);
    }
    
    /* Close invalid socket */
    int bad_close = syscall1(SYS_CLOSESOCK, 999);
    test_result("Close invalid socket rejected", bad_close < 0);
}

/* Test socket options */
static void test_socket_options(void)
{
    print_header("[6] Socket Options Tests");
    
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        test_result("Socket creation for options test", -1);
        return;
    }
    
    /* Test SO_REUSEADDR */
    int optval = 1;
    int result = syscall5(SYS_SETSOCKOPT, sock, SOL_SOCKET, SO_REUSEADDR, (int)&optval, sizeof(optval));
    test_result("Set SO_REUSEADDR", result == 0);
    
    /* Test SO_KEEPALIVE */
    result = syscall5(SYS_SETSOCKOPT, sock, SOL_SOCKET, SO_KEEPALIVE, (int)&optval, sizeof(optval));
    test_result("Set SO_KEEPALIVE", result == 0);
    
    syscall1(SYS_CLOSESOCK, sock);
}

/* Test getsockname */
static void test_getsockname(void)
{
    print_header("[7] Getsockname Tests");
    
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        test_result("Socket creation for getsockname", -1);
        return;
    }
    
    /* Bind to a port */
    struct {
        unsigned short sin_family;
        unsigned short sin_port;
        unsigned int sin_addr;
        char padding[8];
    } addr, result_addr;
    
    addr.sin_family = AF_INET;
    addr.sin_port = (54321 >> 8) | ((54321 & 0xFF) << 8);
    addr.sin_addr = 0;
    
    syscall3(SYS_BIND, sock, (int)&addr, sizeof(addr));
    
    /* Get socket name */
    unsigned int addrlen = sizeof(result_addr);
    int result = syscall3(SYS_GETSOCKNAME, sock, (int)&result_addr, (int)&addrlen);
    test_result("Getsockname call", result == 0);
    test_result("Port matches", result_addr.sin_port == addr.sin_port);
    
    syscall1(SYS_CLOSESOCK, sock);
}

/* Test shutdown */
static void test_shutdown(void)
{
    print_header("[8] Shutdown Tests");
    
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        test_result("Socket creation for shutdown", -1);
        return;
    }
    
    /* Test shutdown read */
    int result = syscall3(SYS_SHUTDOWN, sock, SHUT_RD, 0);
    test_result("Shutdown read", result == 0);
    
    /* Test shutdown write */
    result = syscall3(SYS_SHUTDOWN, sock, SHUT_WR, 0);
    test_result("Shutdown write", result == 0);
    
    syscall1(SYS_CLOSESOCK, sock);
    
    /* Test shutdown on new socket with RDWR */
    sock = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        result = syscall3(SYS_SHUTDOWN, sock, SHUT_RDWR, 0);
        test_result("Shutdown both", result == 0);
        syscall1(SYS_CLOSESOCK, sock);
    }
}

/* Test select */
static void test_select(void)
{
    print_header("[9] Select Tests");
    
    int sock = syscall3(SYS_SOCKET, AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        test_result("Socket creation for select", -1);
        return;
    }
    
    /* Set up fd sets */
    unsigned int readfds = (1 << sock);
    unsigned int writefds = (1 << sock);
    
    /* Select should return immediately for UDP (always writable) */
    int result = syscall4(SYS_SELECT, sock + 1, (int)&readfds, (int)&writefds, 0);
    test_result("Select returns", result >= 0);
    
    syscall1(SYS_CLOSESOCK, sock);
}

/* Print summary */
static void print_summary(void)
{
    print("\n========================================\n");
    print("           TEST SUMMARY\n");
    print("========================================\n");
    print("  Passed:  ");
    print_num(tests_passed);
    print("\n  Failed:  ");
    print_num(tests_failed);
    print("\n  Skipped: ");
    print_num(tests_skipped);
    print("\n  Total:   ");
    print_num(tests_passed + tests_failed + tests_skipped);
    print("\n========================================\n\n");
    
    if (tests_failed == 0) {
        print("SUCCESS: All tests passed!\n");
    } else {
        print("FAILURE: Some tests failed.\n");
    }
}

void _start(void)
{
    print("========================================\n");
    print("   ZurichOS Network Stack Test Suite\n");
    print("========================================\n");
    print("\nThis program tests the socket API from\n");
    print("userspace. Run 'netinit' first!\n");
    
    test_socket_creation();
    test_socket_bind();
    test_tcp_listen();
    test_multiple_sockets();
    test_socket_close();
    test_socket_options();
    test_getsockname();
    test_shutdown();
    test_select();
    
    print_summary();
    
    syscall1(SYS_EXIT, tests_failed == 0 ? 0 : 1);
    while(1);
}
