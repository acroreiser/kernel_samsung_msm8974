#ifndef _LINUX_UTSNAME_H
#define _LINUX_UTSNAME_H

#define __OLD_UTS_LEN 8

struct oldold_utsname {
	char sysname[9];
	char nodename[9];
	char release[9];
	char version[9];
	char machine[9];
};

#define __NEW_UTS_LEN 64

struct old_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
};

struct new_utsname {
	char sysname[__NEW_UTS_LEN + 1];
	char nodename[__NEW_UTS_LEN + 1];
	char release[__NEW_UTS_LEN + 1];
	char version[__NEW_UTS_LEN + 1];
	char machine[__NEW_UTS_LEN + 1];
	char domainname[__NEW_UTS_LEN + 1];
};

#ifdef __KERNEL__

#include <linux/sched.h>
#include <linux/kref.h>
#include <linux/nsproxy.h>
#include <linux/err.h>

enum uts_proc {
	UTS_PROC_OSTYPE,
	UTS_PROC_OSRELEASE,
	UTS_PROC_VERSION,
	UTS_PROC_HOSTNAME,
	UTS_PROC_DOMAINNAME,
};

struct user_namespace;
extern struct user_namespace init_user_ns;

struct uts_namespace {
	struct kref kref;
	struct new_utsname name;
	struct user_namespace *user_ns;
	unsigned int proc_inum;
};
extern struct uts_namespace init_uts_ns;

#ifdef CONFIG_UTS_NS
static inline void get_uts_ns(struct uts_namespace *ns)
{
	kref_get(&ns->kref);
}

extern struct uts_namespace *copy_utsname(unsigned long flags,
					  struct task_struct *tsk);
extern void free_uts_ns(struct kref *kref);

static inline void put_uts_ns(struct uts_namespace *ns)
{
	kref_put(&ns->kref, free_uts_ns);
}
#else
static inline void get_uts_ns(struct uts_namespace *ns)
{
}

static inline void put_uts_ns(struct uts_namespace *ns)
{
}

static inline struct uts_namespace *copy_utsname(unsigned long flags,
						 struct task_struct *tsk)
{
	if (flags & CLONE_NEWUTS)
		return ERR_PTR(-EINVAL);

	return tsk->nsproxy->uts_ns;
}
#endif

#ifdef CONFIG_PROC_SYSCTL
extern void uts_proc_notify(enum uts_proc proc);
#else
static inline void uts_proc_notify(enum uts_proc proc)
{
}
#endif

#ifdef CONFIG_ANDROID_TREBLE_SPOOF_KERNEL_VERSION
static struct new_utsname utsname_spoofed;
#endif
static inline struct new_utsname *utsname(void)
{
#ifdef CONFIG_ANDROID_TREBLE_SPOOF_KERNEL_VERSION
#ifdef CONFIG_ANDROID_TREBLE_BYPASS_KERNEL_VERSION_CHECKS
	if (!strcmp(current->comm, "system_server") ||
            !strcmp(current->comm, "zygote") ||
	    !strcmp(current->comm, "bpfloader") ||
            !strcmp(current->comm, "perfetto") ||
            !strcmp(current->comm, "init"))
	{
#endif
		char fake_release_prepended[64];

		strcpy(fake_release_prepended, CONFIG_ANDROID_TREBLE_SPOOF_KERNEL_VERSION_PREFIX);
	        strcat(fake_release_prepended, "-");
		strcat(fake_release_prepended, current->nsproxy->uts_ns->name.release);
		utsname_spoofed = current->nsproxy->uts_ns->name;
		strcpy(utsname_spoofed.release, fake_release_prepended);

		return &utsname_spoofed;
#ifdef CONFIG_ANDROID_TREBLE_BYPASS_KERNEL_VERSION_CHECKS
	} else
                return &current->nsproxy->uts_ns->name;
#endif
#else
	return &current->nsproxy->uts_ns->name;
#endif
}

static inline struct new_utsname *init_utsname(void)
{
	return &init_uts_ns.name;
}

extern struct rw_semaphore uts_sem;

#endif /* __KERNEL__ */

#endif /* _LINUX_UTSNAME_H */
