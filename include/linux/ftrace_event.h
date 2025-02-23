#ifndef _LINUX_FTRACE_EVENT_H
#define _LINUX_FTRACE_EVENT_H

#include <linux/ring_buffer.h>
#include <linux/trace_seq.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <linux/perf_event.h>

struct trace_array;
struct tracer;
struct dentry;
struct bpf_prog;

struct trace_print_flags {
	unsigned long		mask;
	const char		*name;
};

struct trace_print_flags_u64 {
	unsigned long long	mask;
	const char		*name;
};

const char *ftrace_print_flags_seq(struct trace_seq *p, const char *delim,
				   unsigned long flags,
				   const struct trace_print_flags *flag_array);

const char *ftrace_print_symbols_seq(struct trace_seq *p, unsigned long val,
				     const struct trace_print_flags *symbol_array);

#if BITS_PER_LONG == 32
const char *ftrace_print_symbols_seq_u64(struct trace_seq *p,
					 unsigned long long val,
					 const struct trace_print_flags_u64
								 *symbol_array);
#endif

const char *ftrace_print_hex_seq(struct trace_seq *p,
				 const unsigned char *buf, int len);

/*
 * The trace entry - the most basic unit of tracing. This is what
 * is printed in the end as a single line in the trace output, such as:
 *
 *     bash-15816 [01]   235.197585: idle_cpu <- irq_enter
 */
struct trace_entry {
	unsigned short		type;
	unsigned char		flags;
	unsigned char		preempt_count;
	int			pid;
	int			padding;
};

#define FTRACE_MAX_EVENT						\
	((1 << (sizeof(((struct trace_entry *)0)->type) * 8)) - 1)

/*
 * Trace iterator - used by printout routines who present trace
 * results to users and which routines might sleep, etc:
 */
struct trace_iterator {
	struct trace_array	*tr;
	struct tracer		*trace;
	void			*private;
	int			cpu_file;
	struct mutex		mutex;
	struct ring_buffer_iter	*buffer_iter[NR_CPUS];
	unsigned long		iter_flags;

	/* trace_seq for __print_flags() and __print_symbolic() etc. */
	struct trace_seq	tmp_seq;

	cpumask_var_t		started;

	/* The below is zeroed out in pipe_read */
	struct trace_seq	seq;
	struct trace_entry	*ent;
	unsigned long		lost_events;
	int			leftover;
	int			ent_size;
	int			cpu;
	u64			ts;

	loff_t			pos;
	long			idx;

	/* All new field here will be zeroed out in pipe_read */
};


struct trace_event;

typedef enum print_line_t (*trace_print_func)(struct trace_iterator *iter,
				      int flags, struct trace_event *event);

struct trace_event_functions {
	trace_print_func	trace;
	trace_print_func	raw;
	trace_print_func	hex;
	trace_print_func	binary;
};

struct trace_event {
	struct hlist_node		node;
	struct list_head		list;
	int				type;
	struct trace_event_functions	*funcs;
};

extern int register_ftrace_event(struct trace_event *event);
extern int unregister_ftrace_event(struct trace_event *event);

/* Return values for print_line callback */
enum print_line_t {
	TRACE_TYPE_PARTIAL_LINE	= 0,	/* Retry after flushing the seq */
	TRACE_TYPE_HANDLED	= 1,
	TRACE_TYPE_UNHANDLED	= 2,	/* Relay to other output functions */
	TRACE_TYPE_NO_CONSUME	= 3	/* Handled but ask to not consume */
};

void tracing_generic_entry_update(struct trace_entry *entry,
				  unsigned long flags,
				  int pc);
struct ring_buffer_event *
trace_current_buffer_lock_reserve(struct ring_buffer **current_buffer,
				  int type, unsigned long len,
				  unsigned long flags, int pc);
void trace_current_buffer_unlock_commit(struct ring_buffer *buffer,
					struct ring_buffer_event *event,
					unsigned long flags, int pc);
void trace_nowake_buffer_unlock_commit(struct ring_buffer *buffer,
				       struct ring_buffer_event *event,
					unsigned long flags, int pc);
void trace_nowake_buffer_unlock_commit_regs(struct ring_buffer *buffer,
					    struct ring_buffer_event *event,
					    unsigned long flags, int pc,
					    struct pt_regs *regs);
void trace_current_buffer_discard_commit(struct ring_buffer *buffer,
					 struct ring_buffer_event *event);

void tracing_record_cmdline(struct task_struct *tsk);

struct event_filter;

enum trace_reg {
	TRACE_REG_REGISTER,
	TRACE_REG_UNREGISTER,
#ifdef CONFIG_PERF_EVENTS
	TRACE_REG_PERF_REGISTER,
	TRACE_REG_PERF_UNREGISTER,
	TRACE_REG_PERF_OPEN,
	TRACE_REG_PERF_CLOSE,
	TRACE_REG_PERF_ADD,
	TRACE_REG_PERF_DEL,
#endif
};

struct ftrace_event_call;

struct ftrace_event_class {
	char			*system;
	void			*probe;
#ifdef CONFIG_PERF_EVENTS
	void			*perf_probe;
#endif
	int			(*reg)(struct ftrace_event_call *event,
				       enum trace_reg type, void *data);
	int			(*define_fields)(struct ftrace_event_call *);
	struct list_head	*(*get_fields)(struct ftrace_event_call *);
	struct list_head	fields;
	int			(*raw_init)(struct ftrace_event_call *);
};

extern int ftrace_event_reg(struct ftrace_event_call *event,
			    enum trace_reg type, void *data);

enum {
	TRACE_EVENT_FL_ENABLED_BIT,
	TRACE_EVENT_FL_FILTERED_BIT,
	TRACE_EVENT_FL_RECORDED_CMD_BIT,
	TRACE_EVENT_FL_CAP_ANY_BIT,
	TRACE_EVENT_FL_NO_SET_FILTER_BIT,
	TRACE_EVENT_FL_IGNORE_ENABLE_BIT,
	TRACE_EVENT_FL_KPROBE_BIT,
};

enum {
	TRACE_EVENT_FL_ENABLED		= (1 << TRACE_EVENT_FL_ENABLED_BIT),
	TRACE_EVENT_FL_FILTERED		= (1 << TRACE_EVENT_FL_FILTERED_BIT),
	TRACE_EVENT_FL_RECORDED_CMD	= (1 << TRACE_EVENT_FL_RECORDED_CMD_BIT),
	TRACE_EVENT_FL_CAP_ANY		= (1 << TRACE_EVENT_FL_CAP_ANY_BIT),
	TRACE_EVENT_FL_NO_SET_FILTER	= (1 << TRACE_EVENT_FL_NO_SET_FILTER_BIT),
	TRACE_EVENT_FL_IGNORE_ENABLE	= (1 << TRACE_EVENT_FL_IGNORE_ENABLE_BIT),
	TRACE_EVENT_FL_KPROBE		= (1 << TRACE_EVENT_FL_KPROBE_BIT),
};

struct ftrace_event_call {
	struct list_head	list;
	struct ftrace_event_class *class;
	char			*name;
	struct dentry		*dir;
	struct trace_event	event;
	const char		*print_fmt;
	struct event_filter	*filter;
	void			*mod;
	void			*data;

	/*
	 * 32 bit flags:
	 *   bit 1:		enabled
	 *   bit 2:		filter_active
	 *   bit 3:		enabled cmd record
	 *
	 * Changes to flags must hold the event_mutex.
	 *
	 * Note: Reads of flags do not hold the event_mutex since
	 * they occur in critical sections. But the way flags
	 * is currently used, these changes do no affect the code
	 * except that when a change is made, it may have a slight
	 * delay in propagating the changes to other CPUs due to
	 * caching and such.
	 */
	unsigned int		flags;

#ifdef CONFIG_PERF_EVENTS
	int				perf_refcount;
	struct hlist_head __percpu	*perf_events;
	struct bpf_prog_array __rcu	*prog_array;
#endif
};

#ifdef CONFIG_PERF_EVENTS
static inline bool bpf_prog_array_valid(struct ftrace_event_call *call)
{
	/*
	 * This inline function checks whether call->prog_array
	 * is valid or not. The function is called in various places,
	 * outside rcu_read_lock/unlock, as a heuristic to speed up execution.
	 *
	 * If this function returns true, and later call->prog_array
	 * becomes false inside rcu_read_lock/unlock region,
	 * we bail out then. If this function return false,
	 * there is a risk that we might miss a few events if the checking
	 * were delayed until inside rcu_read_lock/unlock region and
	 * call->prog_array happened to become non-NULL then.
	 *
	 * Here, READ_ONCE() is used instead of rcu_access_pointer().
	 * rcu_access_pointer() requires the actual definition of
	 * "struct bpf_prog_array" while READ_ONCE() only needs
	 * a declaration of the same type.
	 */
	return !!READ_ONCE(call->prog_array);
}
#endif

#define __TRACE_EVENT_FLAGS(name, value)				\
	static int __init trace_init_flags_##name(void)			\
	{								\
		event_##name.flags = value;				\
		return 0;						\
	}								\
	early_initcall(trace_init_flags_##name);

#define PERF_MAX_TRACE_SIZE	2048

#define MAX_FILTER_STR_VAL	256	/* Should handle KSYM_SYMBOL_LEN */

extern void destroy_preds(struct ftrace_event_call *call);
extern int filter_match_preds(struct event_filter *filter, void *rec);
extern int filter_current_check_discard(struct ring_buffer *buffer,
					struct ftrace_event_call *call,
					void *rec,
					struct ring_buffer_event *event);

#ifdef CONFIG_BPF_EVENTS
unsigned int trace_call_bpf(struct ftrace_event_call *call, void *ctx);
int perf_event_attach_bpf_prog(struct perf_event *event, struct bpf_prog *prog);
void perf_event_detach_bpf_prog(struct perf_event *event);
#else
static inline unsigned int trace_call_bpf(struct ftrace_event_call *call, void *ctx)
{
	return 1;
}
static inline int
perf_event_attach_bpf_prog(struct perf_event *event, struct bpf_prog *prog)
{
	return -EOPNOTSUPP;
}

static inline void perf_event_detach_bpf_prog(struct perf_event *event) { }
#endif

enum {
	FILTER_OTHER = 0,
	FILTER_STATIC_STRING,
	FILTER_DYN_STRING,
	FILTER_PTR_STRING,
	FILTER_TRACE_FN,
};

#define EVENT_STORAGE_SIZE 128
extern struct mutex event_storage_mutex;
extern char event_storage[EVENT_STORAGE_SIZE];

extern int trace_event_raw_init(struct ftrace_event_call *call);
extern int trace_define_field(struct ftrace_event_call *call, const char *type,
			      const char *name, int offset, int size,
			      int is_signed, int filter_type);
extern int trace_add_event_call(struct ftrace_event_call *call);
extern void trace_remove_event_call(struct ftrace_event_call *call);
extern int trace_event_get_offsets(struct ftrace_event_call *call);

#define is_signed_type(type)	(((type)(-1)) < (type)0)

int trace_set_clr_event(const char *system, const char *event, int set);

/*
 * The double __builtin_constant_p is because gcc will give us an error
 * if we try to allocate the static variable to fmt if it is not a
 * constant. Even with the outer if statement optimizing out.
 */
#define event_trace_printk(ip, fmt, args...)				\
do {									\
	__trace_printk_check_format(fmt, ##args);			\
	tracing_record_cmdline(current);				\
	if (__builtin_constant_p(fmt)) {				\
		static const char *trace_printk_fmt			\
		  __attribute__((section("__trace_printk_fmt"))) =	\
			__builtin_constant_p(fmt) ? fmt : NULL;		\
									\
		__trace_bprintk(ip, trace_printk_fmt, ##args);		\
	} else								\
		__trace_printk(ip, fmt, ##args);			\
} while (0)

#ifdef CONFIG_PERF_EVENTS
struct perf_event;

DECLARE_PER_CPU(struct pt_regs, perf_trace_regs);

extern int  perf_trace_init(struct perf_event *event);
extern void perf_trace_destroy(struct perf_event *event);
extern int  perf_trace_add(struct perf_event *event, int flags);
extern void perf_trace_del(struct perf_event *event, int flags);
extern int  ftrace_profile_set_filter(struct perf_event *event, int event_id,
				     char *filter_str);
extern void ftrace_profile_free_filter(struct perf_event *event);
void perf_trace_buf_update(void *record, u16 type);
void *perf_trace_buf_alloc(int size, struct pt_regs *regs, int *rctxp);

void perf_trace_run_bpf_submit(void *raw_data, int size, int rctx,
			       struct ftrace_event_call *call, u64 count,
			       struct pt_regs *regs, struct hlist_head *head,
			       struct task_struct *task);

static inline void
perf_trace_buf_submit(void *raw_data, int size, int rctx, u16 type,
		       u64 count, struct pt_regs *regs, void *head,
		       struct task_struct *task)
{
	perf_tp_event(type, count, raw_data, size, regs, head, rctx, task);
}
#endif

#endif /* _LINUX_FTRACE_EVENT_H */
