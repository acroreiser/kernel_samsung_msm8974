#define COMMON_FILE_SOCK_PERMS "ioctl", "read", "write", "create", \
    "getattr", "setattr", "lock", "relabelfrom", "relabelto", "append"

#define COMMON_FILE_PERMS COMMON_FILE_SOCK_PERMS, "unlink", "link", \
    "rename", "execute", "quotaon", "mounton", "audit_access", \
    "open", "execmod"

#define COMMON_SOCK_PERMS COMMON_FILE_SOCK_PERMS, "bind", "connect", \
    "listen", "accept", "getopt", "setopt", "shutdown", "recvfrom",  \
    "sendto", "name_bind"

#define COMMON_IPC_PERMS "create", "destroy", "getattr", "setattr", "read", \
	    "write", "associate", "unix_read", "unix_write"

/*
 * Note: The name for any socket class should be suffixed by "socket",
 *	 and doesn't contain more than one substr of "socket".
 */
struct security_class_mapping secclass_map[] = {
	{ "security",
	  { "compute_av", "compute_create", "compute_member",
	    "check_context", "load_policy", "compute_relabel",
	    "compute_user", "setenforce", "setbool", "setsecparam",
	    "setcheckreqprot", "read_policy", NULL } },
	{ "process",
	  { "fork", "transition", "sigchld", "sigkill",
	    "sigstop", "signull", "signal", "ptrace", "getsched", "setsched",
	    "getsession", "getpgid", "setpgid", "getcap", "setcap", "share",
	    "getattr", "setexec", "setfscreate", "noatsecure", "siginh",
	    "setrlimit", "rlimitinh", "dyntransition", "setcurrent",
	    "execmem", "execstack", "execheap", "setkeycreate",
	    "setsockcreate", NULL } },
	{ "system",
	  { "ipc_info", "syslog_read", "syslog_mod",
	    "syslog_console", "module_request", NULL } },
	{ "capability",
	  { "chown", "dac_override", "dac_read_search",
	    "fowner", "fsetid", "kill", "setgid", "setuid", "setpcap",
	    "linux_immutable", "net_bind_service", "net_broadcast",
	    "net_admin", "net_raw", "ipc_lock", "ipc_owner", "sys_module",
	    "sys_rawio", "sys_chroot", "sys_ptrace", "sys_pacct", "sys_admin",
	    "sys_boot", "sys_nice", "sys_resource", "sys_time",
	    "sys_tty_config", "mknod", "lease", "audit_write",
	    "audit_control", "setfcap", NULL } },
	{ "filesystem",
	  { "mount", "remount", "unmount", "getattr",
	    "relabelfrom", "relabelto", "associate", "quotamod",
	    "quotaget", NULL } },
	{ "file",
	  { COMMON_FILE_PERMS,
	    "execute_no_trans", "entrypoint", NULL } },
	{ "dir",
	  { COMMON_FILE_PERMS, "add_name", "remove_name",
	    "reparent", "search", "rmdir", NULL } },
	{ "fd", { "use", NULL } },
	{ "lnk_file",
	  { COMMON_FILE_PERMS, NULL } },
	{ "chr_file",
	  { COMMON_FILE_PERMS, NULL } },
	{ "blk_file",
	  { COMMON_FILE_PERMS, NULL } },
	{ "sock_file",
	  { COMMON_FILE_PERMS, NULL } },
	{ "fifo_file",
	  { COMMON_FILE_PERMS, NULL } },
	{ "socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "tcp_socket",
	  { COMMON_SOCK_PERMS,
	    "node_bind", "name_connect",
	    NULL } },
	{ "udp_socket",
	  { COMMON_SOCK_PERMS,
	    "node_bind", NULL } },
	{ "rawip_socket",
	  { COMMON_SOCK_PERMS,
	    "node_bind", NULL } },
	{ "node",
	  { "recvfrom", "sendto", NULL } },
	{ "netif",
	  { "ingress", "egress", NULL } },
	{ "netlink_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "packet_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "key_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "unix_stream_socket",
	  { COMMON_SOCK_PERMS, "connectto", NULL } },
	{ "unix_dgram_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "sem",
	  { COMMON_IPC_PERMS, NULL } },
	{ "msg", { "send", "receive", NULL } },
	{ "msgq",
	  { COMMON_IPC_PERMS, "enqueue", NULL } },
	{ "shm",
	  { COMMON_IPC_PERMS, "lock", NULL } },
	{ "ipc",
	  { COMMON_IPC_PERMS, NULL } },
	{ "netlink_route_socket",
	  { COMMON_SOCK_PERMS,
	    "nlmsg_read", "nlmsg_write", NULL } },
	{ "netlink_tcpdiag_socket",
	  { COMMON_SOCK_PERMS,
	    "nlmsg_read", "nlmsg_write", NULL } },
	{ "netlink_nflog_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_xfrm_socket",
	  { COMMON_SOCK_PERMS,
	    "nlmsg_read", "nlmsg_write", NULL } },
	{ "netlink_selinux_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_iscsi_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_audit_socket",
	  { COMMON_SOCK_PERMS,
	    "nlmsg_read", "nlmsg_write", "nlmsg_relay", "nlmsg_readpriv",
	    "nlmsg_tty_audit", NULL } },
	{ "netlink_fib_lookup_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_connector_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_netfilter_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_dnrt_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "association",
	  { "sendto", "recvfrom", "setcontext", "polmatch", NULL } },
	{ "netlink_kobject_uevent_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_generic_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_scsitransport_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_rdma_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "netlink_crypto_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "appletalk_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "packet",
	  { "send", "recv", "relabelto", "forward_in", "forward_out", NULL } },
	{ "key",
	  { "view", "read", "write", "search", "link", "setattr", "create",
	    NULL } },
	{ "dccp_socket",
	  { COMMON_SOCK_PERMS,
	    "node_bind", "name_connect", NULL } },
	{ "memprotect", { "mmap_zero", NULL } },
	{ "peer", { "recv", NULL } },
	{ "capability2",
	  { "mac_override", "mac_admin", "syslog", "wake_alarm", "block_suspend",
	    NULL } },
	{ "kernel_service", { "use_as_override", "create_files_as", NULL } },
	{ "tun_socket",
	  { COMMON_SOCK_PERMS, NULL } },
	{ "binder", { "impersonate", "call", "set_context_mgr", "transfer", NULL } },
	{ "bpf", { "map_create", "map_read", "map_write", "prog_load", "prog_run" } },
	{ NULL }
  };
