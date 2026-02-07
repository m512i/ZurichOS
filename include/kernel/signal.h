/* Signal Definitions
 * POSIX-style signal handling
 */

#ifndef _KERNEL_SIGNAL_H
#define _KERNEL_SIGNAL_H

#include <stdint.h>

#define SIGHUP      1   /* Hangup */
#define SIGINT      2   /* Interrupt (Ctrl+C) */
#define SIGQUIT     3   /* Quit */
#define SIGILL      4   /* Illegal instruction */
#define SIGTRAP     5   /* Trace trap */
#define SIGABRT     6   /* Abort */
#define SIGBUS      7   /* Bus error */
#define SIGFPE      8   /* Floating point exception */
#define SIGKILL     9   /* Kill (cannot be caught) */
#define SIGUSR1     10  /* User signal 1 */
#define SIGSEGV     11  /* Segmentation fault */
#define SIGUSR2     12  /* User signal 2 */
#define SIGPIPE     13  /* Broken pipe */
#define SIGALRM     14  /* Alarm */
#define SIGTERM     15  /* Termination */
#define SIGSTKFLT   16  /* Stack fault */
#define SIGCHLD     17  /* Child stopped or terminated */
#define SIGCONT     18  /* Continue if stopped */
#define SIGSTOP     19  /* Stop (cannot be caught) */
#define SIGTSTP     20  /* Terminal stop */
#define SIGTTIN     21  /* Background read from tty */
#define SIGTTOU     22  /* Background write to tty */
#define SIGURG      23  /* Urgent condition on socket */
#define SIGXCPU     24  /* CPU time limit exceeded */
#define SIGXFSZ     25  /* File size limit exceeded */
#define SIGVTALRM   26  /* Virtual timer expired */
#define SIGPROF     27  /* Profiling timer expired */
#define SIGWINCH    28  /* Window size change */
#define SIGIO       29  /* I/O possible */
#define SIGPWR      30  /* Power failure */
#define SIGSYS      31  /* Bad system call */

#define NSIG        32  /* Number of signals */

#define SA_NOCLDSTOP    0x00000001
#define SA_NOCLDWAIT    0x00000002
#define SA_SIGINFO      0x00000004
#define SA_RESTART      0x10000000
#define SA_NODEFER      0x40000000
#define SA_RESETHAND    0x80000000

#define SIG_DFL     ((sighandler_t)0)   /* Default action */
#define SIG_IGN     ((sighandler_t)1)   /* Ignore signal */
#define SIG_ERR     ((sighandler_t)-1)  /* Error return */
typedef void (*sighandler_t)(int);
typedef uint32_t sigset_t;
typedef struct {
    sighandler_t sa_handler;
    uint32_t sa_flags;
    sigset_t sa_mask;
} sigaction_t;

#define sigemptyset(set)    (*(set) = 0)
#define sigfillset(set)     (*(set) = 0xFFFFFFFF)
#define sigaddset(set, sig) (*(set) |= (1 << ((sig) - 1)))
#define sigdelset(set, sig) (*(set) &= ~(1 << ((sig) - 1)))
#define sigismember(set, sig) ((*(set) & (1 << ((sig) - 1))) != 0)

#define SIG_ACTION_TERM     0   /* Terminate process */
#define SIG_ACTION_IGN      1   /* Ignore signal */
#define SIG_ACTION_CORE     2   /* Terminate and dump core */
#define SIG_ACTION_STOP     3   /* Stop process */
#define SIG_ACTION_CONT     4   /* Continue process */

void signal_init_process(void *proc);
int signal_send(uint32_t pid, int sig);
int signal_pending(void *proc);
void signal_handle_pending(void *proc);
int signal_set_handler(int sig, sighandler_t handler, sigaction_t *oldact);
int signal_block(sigset_t *set, sigset_t *oldset);
int signal_unblock(sigset_t *set, sigset_t *oldset);
int signal_setmask(sigset_t *set, sigset_t *oldset);

#endif
