/* -*- linux-c -*-
 * sysctl_net_core.c: sysctl interface to net core subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net/core directory entry (empty =) ). [MS]
 */

#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/netdevice.h>
#include <linux/ratelimit.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kmemleak.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/net_ratelimit.h>

static int zero = 0;
static int ushort_max = USHRT_MAX;
static int one __maybe_unused = 1;
static int two __maybe_unused = 2;
static long long_one __maybe_unused = 1;
static long long_max __maybe_unused = LONG_MAX;

#ifdef CONFIG_RPS
static int rps_sock_flow_sysctl(ctl_table *table, int write,
				void __user *buffer, size_t *lenp, loff_t *ppos)
{
	unsigned int orig_size, size;
	int ret, i;
	ctl_table tmp = {
		.data = &size,
		.maxlen = sizeof(size),
		.mode = table->mode
	};
	struct rps_sock_flow_table *orig_sock_table, *sock_table;
	static DEFINE_MUTEX(sock_flow_mutex);

	mutex_lock(&sock_flow_mutex);

	orig_sock_table = rcu_dereference_protected(rps_sock_flow_table,
					lockdep_is_held(&sock_flow_mutex));
	size = orig_size = orig_sock_table ? orig_sock_table->mask + 1 : 0;

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);

	if (write) {
		if (size) {
			if (size > 1<<30) {
				/* Enforce limit to prevent overflow */
				mutex_unlock(&sock_flow_mutex);
				return -EINVAL;
			}
			size = roundup_pow_of_two(size);
			if (size != orig_size) {
				sock_table =
				    vmalloc(RPS_SOCK_FLOW_TABLE_SIZE(size));
				if (!sock_table) {
					mutex_unlock(&sock_flow_mutex);
					return -ENOMEM;
				}

				sock_table->mask = size - 1;
			} else
				sock_table = orig_sock_table;

			for (i = 0; i < size; i++)
				sock_table->ents[i] = RPS_NO_CPU;
		} else
			sock_table = NULL;

		if (sock_table != orig_sock_table) {
			rcu_assign_pointer(rps_sock_flow_table, sock_table);
			if (sock_table)
				static_key_slow_inc(&rps_needed);
			if (orig_sock_table) {
				static_key_slow_dec(&rps_needed);
				synchronize_rcu();
				vfree(orig_sock_table);
			}
		}
	}

	mutex_unlock(&sock_flow_mutex);

	return ret;
}
#endif /* CONFIG_RPS */

#ifdef CONFIG_BPF_JIT
static int proc_dointvec_minmax_bpf_enable(struct ctl_table *table, int write,
					   void __user *buffer, size_t *lenp,
					   loff_t *ppos)
{
	int ret, jit_enable = *(int *)table->data;
	struct ctl_table tmp = *table;

	if (write && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	tmp.data = &jit_enable;
	ret = proc_dointvec_minmax(&tmp, write, buffer, lenp, ppos);
	if (write && !ret) {
		*(int *)table->data = jit_enable;
		if (jit_enable == 2)
			pr_warn("bpf_jit_enable = 2 was set! NEVER use this in production, only for JIT debugging!\n");
	}
	return ret;
}

# ifdef CONFIG_HAVE_EBPF_JIT
static int
proc_dointvec_minmax_bpf_restricted(struct ctl_table *table, int write,
				    void __user *buffer, size_t *lenp,
				    loff_t *ppos)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	return proc_dointvec_minmax(table, write, buffer, lenp, ppos);
}
# endif /* CONFIG_HAVE_EBPF_JIT */

static int
proc_dolongvec_minmax_bpf_restricted(struct ctl_table *table, int write,
				     void __user *buffer, size_t *lenp,
				     loff_t *ppos)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	return proc_doulongvec_minmax(table, write, buffer, lenp, ppos);
}
#endif

static struct ctl_table net_core_table[] = {
#ifdef CONFIG_NET
	{
		.procname	= "wmem_max",
		.data		= &sysctl_wmem_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &one,
	},
	{
		.procname	= "rmem_max",
		.data		= &sysctl_rmem_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &one,
	},
	{
		.procname	= "wmem_default",
		.data		= &sysctl_wmem_default,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &one,
	},
	{
		.procname	= "rmem_default",
		.data		= &sysctl_rmem_default,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &one,
	},
	{
		.procname	= "dev_weight",
		.data		= &weight_p,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "netdev_max_backlog",
		.data		= &netdev_max_backlog,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
#ifdef CONFIG_BPF_JIT
	{
		.procname	= "bpf_jit_enable",
		.data		= &bpf_jit_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax_bpf_enable,
# ifdef CONFIG_BPF_JIT_ALWAYS_ON
		.extra1		= &one,
		.extra2		= &one,
# else
		.extra1		= &zero,
		.extra2		= &two,
# endif
	},
# ifdef CONFIG_HAVE_EBPF_JIT
	{
		.procname	= "bpf_jit_harden",
		.data		= &bpf_jit_harden,
		.maxlen		= sizeof(int),
		.mode		= 0600,
		.proc_handler	= proc_dointvec_minmax_bpf_restricted,
		.extra1		= &zero,
		.extra2		= &two,
	},
# endif
	{
		.procname	= "bpf_jit_limit",
		.data		= &bpf_jit_limit,
		.maxlen		= sizeof(long),
		.mode		= 0600,
		.proc_handler	= proc_dolongvec_minmax_bpf_restricted,
		.extra1		= &long_one,
		.extra2		= &long_max,
	},
#endif
	{
		.procname	= "netdev_tstamp_prequeue",
		.data		= &netdev_tstamp_prequeue,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "message_cost",
		.data		= &net_ratelimit_state.interval,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "message_burst",
		.data		= &net_ratelimit_state.burst,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "optmem_max",
		.data		= &sysctl_optmem_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
#ifdef CONFIG_RPS
	{
		.procname	= "rps_sock_flow_entries",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= rps_sock_flow_sysctl
	},
#endif
#endif /* CONFIG_NET */
	{
		.procname	= "netdev_budget",
		.data		= &netdev_budget,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "warnings",
		.data		= &net_msg_warn,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{ }
};

static struct ctl_table netns_core_table[] = {
	{
		.procname	= "somaxconn",
		.data		= &init_net.core.sysctl_somaxconn,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.extra1		= &zero,
		.extra2		= &ushort_max,
		.proc_handler	= proc_dointvec_minmax
	},
	{ }
};

__net_initdata struct ctl_path net_core_path[] = {
	{ .procname = "net", },
	{ .procname = "core", },
	{ },
};

static __net_init int sysctl_core_net_init(struct net *net)
{
	struct ctl_table *tbl;

	net->core.sysctl_somaxconn = SOMAXCONN;

	tbl = netns_core_table;
	if (!net_eq(net, &init_net)) {
		tbl = kmemdup(tbl, sizeof(netns_core_table), GFP_KERNEL);
		if (tbl == NULL)
			goto err_dup;

		tbl[0].data = &net->core.sysctl_somaxconn;
	}

	net->core.sysctl_hdr = register_net_sysctl(net, "net/core", tbl);
	if (net->core.sysctl_hdr == NULL)
		goto err_reg;

	return 0;

err_reg:
	if (tbl != netns_core_table)
		kfree(tbl);
err_dup:
	return -ENOMEM;
}

static __net_exit void sysctl_core_net_exit(struct net *net)
{
	struct ctl_table *tbl;

	tbl = net->core.sysctl_hdr->ctl_table_arg;
	unregister_net_sysctl_table(net->core.sysctl_hdr);
	BUG_ON(tbl == netns_core_table);
	kfree(tbl);
}

static __net_initdata struct pernet_operations sysctl_core_ops = {
	.init = sysctl_core_net_init,
	.exit = sysctl_core_net_exit,
};

static __init int sysctl_core_init(void)
{
	static struct ctl_table empty[1];

	kmemleak_not_leak(register_net_sysctl_table(&init_net, net_core_path, empty));
	register_net_sysctl(&init_net, "net/core", net_core_table);
	return register_pernet_subsys(&sysctl_core_ops);
}

fs_initcall(sysctl_core_init);
