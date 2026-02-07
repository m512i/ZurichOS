/* Signal
 * POSIX-style signal handling
 */

#include <kernel/signal.h>
#include <kernel/process.h>
#include <drivers/serial.h>
#include <string.h>

static const uint8_t default_actions[NSIG] = {
    [0]        = SIG_ACTION_IGN,    /* Signal 0 (null) */
    [SIGHUP]   = SIG_ACTION_TERM,
    [SIGINT]   = SIG_ACTION_TERM,
    [SIGQUIT]  = SIG_ACTION_CORE,
    [SIGILL]   = SIG_ACTION_CORE,
    [SIGTRAP]  = SIG_ACTION_CORE,
    [SIGABRT]  = SIG_ACTION_CORE,
    [SIGBUS]   = SIG_ACTION_CORE,
    [SIGFPE]   = SIG_ACTION_CORE,
    [SIGKILL]  = SIG_ACTION_TERM,   /* Cannot be caught */
    [SIGUSR1]  = SIG_ACTION_TERM,
    [SIGSEGV]  = SIG_ACTION_CORE,
    [SIGUSR2]  = SIG_ACTION_TERM,
    [SIGPIPE]  = SIG_ACTION_TERM,
    [SIGALRM]  = SIG_ACTION_TERM,
    [SIGTERM]  = SIG_ACTION_TERM,
    [SIGSTKFLT]= SIG_ACTION_TERM,
    [SIGCHLD]  = SIG_ACTION_IGN,
    [SIGCONT]  = SIG_ACTION_CONT,
    [SIGSTOP]  = SIG_ACTION_STOP,   /* Cannot be caught */
    [SIGTSTP]  = SIG_ACTION_STOP,
    [SIGTTIN]  = SIG_ACTION_STOP,
    [SIGTTOU]  = SIG_ACTION_STOP,
    [SIGURG]   = SIG_ACTION_IGN,
    [SIGXCPU]  = SIG_ACTION_CORE,
    [SIGXFSZ]  = SIG_ACTION_CORE,
    [SIGVTALRM]= SIG_ACTION_TERM,
    [SIGPROF]  = SIG_ACTION_TERM,
    [SIGWINCH] = SIG_ACTION_IGN,
    [SIGIO]    = SIG_ACTION_TERM,
    [SIGPWR]   = SIG_ACTION_TERM,
    [SIGSYS]   = SIG_ACTION_CORE,
};

void signal_init_process(void *p)
{
    process_t *proc = (process_t *)p;
    if (!proc) return;
    
    proc->pending_signals = 0;
    proc->blocked_signals = 0;
    
    for (int i = 0; i < NSIG; i++) {
        proc->signal_handlers[i] = SIG_DFL;
    }
}

int signal_send(uint32_t pid, int sig)
{
    if (sig < 0 || sig >= NSIG) {
        return -22;  
    }
    
    if (sig == 0) {
        return process_get(pid) ? 0 : -3; 
    }
    
    process_t *proc = process_get(pid);
    if (!proc) {
        return -3;  
    }
    
    proc->pending_signals |= (1 << (sig - 1));
    
    serial_puts("[SIGNAL] Sent signal ");
    char buf[4];
    buf[0] = '0' + (sig / 10);
    buf[1] = '0' + (sig % 10);
    buf[2] = '\0';
    serial_puts(buf);
    serial_puts(" to PID ");
    buf[0] = '0' + (pid / 10);
    buf[1] = '0' + (pid % 10);
    serial_puts(buf);
    serial_puts("\n");
    
    if (sig == SIGCONT && proc->state == PROC_STATE_STOPPED) {
        proc->state = PROC_STATE_READY;
    }
    
    return 0;
}

int signal_pending(void *p)
{
    process_t *proc = (process_t *)p;
    if (!proc) return 0;
    
    return proc->pending_signals & ~proc->blocked_signals;
}

void signal_handle_pending(void *p)
{
    process_t *proc = (process_t *)p;
    if (!proc) return;
    
    uint32_t pending = proc->pending_signals & ~proc->blocked_signals;
    if (pending == 0) return;
    
    for (int sig = 1; sig < NSIG; sig++) {
        if (!(pending & (1 << (sig - 1)))) continue;
        
        proc->pending_signals &= ~(1 << (sig - 1));
        
        sighandler_t handler = proc->signal_handlers[sig];
        
        if (sig == SIGKILL) {
            serial_puts("[SIGNAL] SIGKILL - terminating process\n");
            process_exit(-sig);
            return;
        }
        
        if (sig == SIGSTOP) {
            serial_puts("[SIGNAL] SIGSTOP - stopping process\n");
            proc->state = PROC_STATE_STOPPED;
            return;
        }
        
        if (handler == SIG_IGN) {
            continue;
        }
        
        if (handler == SIG_DFL) {
            uint8_t action = default_actions[sig];
            
            switch (action) {
                case SIG_ACTION_TERM:
                    serial_puts("[SIGNAL] Default action: terminate\n");
                    process_exit(-sig);
                    return;
                    
                case SIG_ACTION_CORE:
                    serial_puts("[SIGNAL] Default action: core dump (terminate)\n");
                    process_exit(-sig);
                    return;
                    
                case SIG_ACTION_STOP:
                    serial_puts("[SIGNAL] Default action: stop\n");
                    proc->state = PROC_STATE_STOPPED;
                    return;
                    
                case SIG_ACTION_CONT:
                    serial_puts("[SIGNAL] Default action: continue\n");
                    if (proc->state == PROC_STATE_STOPPED) {
                        proc->state = PROC_STATE_READY;
                    }
                    break;
                    
                case SIG_ACTION_IGN:
                default:
                    break;
            }
        } else {
            /* User-defined handler - would need to set up signal trampoline 
             * For now, just call the handler directly (unsafe in real OS) */
            serial_puts("[SIGNAL] Calling user handler\n");
            handler(sig);
        }
    }
}

int signal_set_handler(int sig, sighandler_t handler, sigaction_t *oldact)
{
    if (sig < 1 || sig >= NSIG) {
        return -22;  /* EINVAL */
    }
    
    if (sig == SIGKILL || sig == SIGSTOP) {
        return -22;  
    }
    
    process_t *proc = process_current();
    if (!proc) return -1;
    
    if (oldact) {
        oldact->sa_handler = proc->signal_handlers[sig];
        oldact->sa_flags = 0;
        oldact->sa_mask = 0;
    }
    
    proc->signal_handlers[sig] = handler;
    return 0;
}

int signal_block(sigset_t *set, sigset_t *oldset)
{
    process_t *proc = process_current();
    if (!proc) return -1;
    
    if (oldset) {
        *oldset = proc->blocked_signals;
    }
    
    if (set) {
        uint32_t mask = *set;
        mask &= ~((1 << (SIGKILL - 1)) | (1 << (SIGSTOP - 1)));
        proc->blocked_signals |= mask;
    }
    
    return 0;
}

int signal_unblock(sigset_t *set, sigset_t *oldset)
{
    process_t *proc = process_current();
    if (!proc) return -1;
    
    if (oldset) {
        *oldset = proc->blocked_signals;
    }
    
    if (set) {
        proc->blocked_signals &= ~(*set);
    }
    
    return 0;
}

int signal_setmask(sigset_t *set, sigset_t *oldset)
{
    process_t *proc = process_current();
    if (!proc) return -1;
    
    if (oldset) {
        *oldset = proc->blocked_signals;
    }
    
    if (set) {
        uint32_t mask = *set;
        mask &= ~((1 << (SIGKILL - 1)) | (1 << (SIGSTOP - 1)));
        proc->blocked_signals = mask;
    }
    
    return 0;
}
