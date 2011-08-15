
#include "xen.h"

#include <xen/io/xs_wire.h>

#define NR_EVS 1024
typedef void (*evtchn_handler_t)(evtchn_port_t, struct pt_regs *, void *);
/* this represents a event handler. Chaining or sharing is not allowed */
typedef struct _ev_action_t {
	evtchn_handler_t handler;
	void *data;
	uint32_t count;
} ev_action_t;

static ev_action_t ev_actions[NR_EVS];
static unsigned long bound_ports[NR_EVS / (8 * sizeof(unsigned long))];

//void default_handler(evtchn_port_t port, struct pt_regs *regs, void *data);

int in_callback;

void do_hypervisor_callback(struct pt_regs *regs) {
	unsigned long l1, l2, l1i, l2i;
	unsigned int port;
	int cpu = 0;
	shared_info_t *s = g_sharedInfoArea;
	vcpu_info_t *vcpu_info = &s->vcpu_info[cpu];

	in_callback = 1;

	vcpu_info->evtchn_upcall_pending = 0;
	/* NB x86. No need for a barrier here -- XCHG is a barrier on x86. */
#if !defined(__i386__) && !defined(__x86_64__)
	/* Clear master flag /before/ clearing selector flag. */
	wmb();
#endif
	l1 = xchg(&vcpu_info->evtchn_pending_sel, 0);
	while (l1 != 0) {
		l1i = __ffs(l1);
		l1 &= ~(1UL << l1i);

		while ((l2 = active_evtchns(cpu, s, l1i)) != 0) {
			l2i = __ffs(l2);
			l2 &= ~(1UL << l2i);

			port = (l1i * (sizeof(unsigned long) * 8)) + l2i;
			do_event(port, regs);
		}
	}

	in_callback = 0;
}
inline void clear_evtchn(uint32_t port) {
	shared_info_t *s = g_sharedInfoArea;
	synch_clear_bit(port, &s->evtchn_pending[0]);
}
inline void mask_evtchn(uint32_t port) {
	shared_info_t *s = g_sharedInfoArea;
	synch_set_bit(port, &s->evtchn_mask[0]);
}
void default_handler(evtchn_port_t port, struct pt_regs *regs, void *ignore) {
	ut_printf("[Port %d] - event received\n", port);
}

#define smp_processor_id() 0
void force_evtchn_callback(void) {
	int save;
	vcpu_info_t *vcpu;
	vcpu = &g_sharedInfoArea->vcpu_info[smp_processor_id()];
	save = vcpu->evtchn_upcall_mask;

	while (vcpu->evtchn_upcall_pending) {
		vcpu->evtchn_upcall_mask = 1;
		barrier();
		do_hypervisor_callback(NULL);
		barrier();
		vcpu->evtchn_upcall_mask = save;
		barrier();
	};
}

inline void unmask_evtchn(uint32_t port) {
	shared_info_t *s = g_sharedInfoArea;
	vcpu_info_t *vcpu_info = &s->vcpu_info[smp_processor_id()];

	synch_clear_bit(port, &s->evtchn_mask[0]);

	/*
	 * The following is basically the equivalent of 'hw_resend_irq'. Just like
	 * a real IO-APIC we 'lose the interrupt edge' if the channel is masked.
	 */
	if (synch_test_bit (port, &s->evtchn_pending[0])
			&& !synch_test_and_set_bit(port / (sizeof(unsigned long) * 8),
					&vcpu_info->evtchn_pending_sel)) {
		vcpu_info->evtchn_upcall_pending = 1;
		if (!vcpu_info->evtchn_upcall_mask)
			force_evtchn_callback();
	}
}

/*
 * Demux events to different handlers.
 */
int do_event(evtchn_port_t port, struct pt_regs *regs) {
	ev_action_t *action;

	clear_evtchn(port);

	if (port >= NR_EVS) {
		ut_printf("WARN: do_event(): Port number too large: %d\n", port);
		return 1;
	}

	DEBUG("Interrupt recevied on the port :%d \n",port);
	action = &ev_actions[port];
	action->count++;

	/* call the handler */
	action->handler(port, regs, action->data);

	return 1;

}

evtchn_port_t bind_evtchn(evtchn_port_t port, evtchn_handler_t handler,
		void *data) {
	if (ev_actions[port].handler != default_handler)
		printk("WARN: Handler for port %d already registered, replacing\n",
				port);

	ev_actions[port].data = data;
	wmb();
	ev_actions[port].handler = handler;
	set_bit(port, bound_ports);
	DEBUG("xen Register event handler for port : %d \n",port);

	return port;
}

void unbind_evtchn(evtchn_port_t port) {
	struct evtchn_close close;
	int rc;

	if (ev_actions[port].handler == default_handler)
		printk("WARN: No handler for port %d when unbinding\n", port);
	mask_evtchn(port);
	clear_evtchn(port);

	ev_actions[port].handler = default_handler;
	wmb();
	ev_actions[port].data = NULL;
	clear_bit(port, bound_ports);

	close.port = port;
	rc = HYPERVISOR_event_channel_op(EVTCHNOP_close, &close);
	if (rc)
		printk("WARN: close_port %s failed rc=%d. ignored\n", port, rc);

}
#if 0
#if defined(__x86_64__)

char irqstack[2 * PAGE_SIZE];

static struct pda
{
	int irqcount; /* offset 0 (used in x86_64.S) */
	char *irqstackptr; /*        8 */
}cpu0_pda;
#endif
#endif

/*
 * Initially all events are without a handler and disabled
 */
void init_events(void) {
	int i;
#if 0
#if defined(__x86_64__)
	DEBUG("INITIALIZING XEN EVENTS \n");
	asm volatile("movl %0,%%fs ; movl %0,%%gs" :: "r" (0));
	wrmsrl(0xc0000101, &cpu0_pda); /* 0xc0000101 is MSR_GS_BASE */
	cpu0_pda.irqcount = -1;
	cpu0_pda.irqstackptr = (void*) (((unsigned long)irqstack + 2 * PAGE_SIZE)
			& ~(PAGE_SIZE - 1));
#endif
#endif
	/* initialize event handler */
	for (i = 0; i < NR_EVS; i++) {
		ev_actions[i].handler = default_handler;
		mask_evtchn(i);
	}
}
