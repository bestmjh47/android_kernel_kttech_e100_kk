/*
 *  KTTech security module
 *
 *  This file contains the kttech hook function implementations.
 *
 *  Authors:
 *	namjja <namjja@kttech.co.kr>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2,
 *      as published by the Free Software Foundation.
 */

#include <linux/kmod.h>
#include <linux/xattr.h>
#include <linux/pagemap.h>
#include <linux/mount.h>
#include <linux/stat.h>
#include <linux/kd.h>
#include <asm/ioctls.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/pipe_fs_i.h>
#include <net/netlabel.h>
#include <net/cipso_ipv4.h>
#include <linux/audit.h>
#include <linux/magic.h>
#include <linux/dcache.h>

static char *check_rooting_cmd[] = {
	"init",
	"vold",
#ifndef KTTECH_FINAL_BUILD
	"adbd",
#endif
};

static const char *root_cmd = "/sbin/rd_aboot";
static const char *root_cmd_argv = "rooting";


/**
 * kttech_encode2 - Encode binary string to ascii string.
 *
 * @str:     String in binary format.
 * @str_len: Size of @str in byte.
 *
 * Returns pointer to @str in ascii format on success, NULL otherwise.
 *
 * This function uses kzalloc(), so caller must kfree() if this function
 * didn't return NULL.
 */
char *kttech_encode2(const char *str, int str_len)
{
	int i;
	int len = 0;
	const char *p = str;
	char *cp;
	char *cp0;

	if (!p)
		return NULL;
	for (i = 0; i < str_len; i++) {
		const unsigned char c = p[i];

		if (c == '\\')
			len += 2;
		else if (c > ' ' && c < 127)
			len++;
		else
			len += 4;
	}
	len++;
	/* Reserve space for appending "/". */
	cp = kzalloc(len + 10, GFP_NOFS);
	if (!cp)
		return NULL;
	cp0 = cp;
	p = str;
	for (i = 0; i < str_len; i++) {
		const unsigned char c = p[i];

		if (c == '\\') {
			*cp++ = '\\';
			*cp++ = '\\';
		} else if (c > ' ' && c < 127) {
			*cp++ = c;
		} else {
			*cp++ = '\\';
			*cp++ = (c >> 6) + '0';
			*cp++ = ((c >> 3) & 7) + '0';
			*cp++ = (c & 7) + '0';
		}
	}
	return cp0;
}


/**
 * kttech_encode - Encode binary string to ascii string.
 *
 * @str: String in binary format.
 *
 * Returns pointer to @str in ascii format on success, NULL otherwise.
 *
 * This function uses kzalloc(), so caller must kfree() if this function
 * didn't return NULL.
 */
char *kttech_encode(const char *str)
{
	return str ? kttech_encode2(str, strlen(str)) : NULL;
}

/**
 * kttech_get_absolute_path - Get the path of a dentry but ignores chroot'ed root.
 *
 * @path:   Pointer to "struct path".
 * @buffer: Pointer to buffer to return value in.
 * @buflen: Sizeof @buffer.
 *
 * Returns the buffer on success, an error code otherwise.
 *
 * If dentry is a directory, trailing '/' is appended.
 */
static char *kttech_get_absolute_path(struct path *path, char * const buffer,
				      const int buflen)
{
	char *pos = ERR_PTR(-ENOMEM);
	if (buflen >= 256) {
		/* go to whatever namespace root we are under */
		pos = d_absolute_path(path, buffer, buflen - 1);
		if (!IS_ERR(pos) && *pos == '/' && pos[1]) {
			struct inode *inode = path->dentry->d_inode;
			if (inode && S_ISDIR(inode->i_mode)) {
				buffer[buflen - 2] = '/';
				buffer[buflen - 1] = '\0';
			}
		}
	}
	return pos;
}

/**
 * kttech_get_dentry_path - Get the path of a dentry.
 *
 * @dentry: Pointer to "struct dentry".
 * @buffer: Pointer to buffer to return value in.
 * @buflen: Sizeof @buffer.
 *
 * Returns the buffer on success, an error code otherwise.
 *
 * If dentry is a directory, trailing '/' is appended.
 */
static char *kttech_get_dentry_path(struct dentry *dentry, char * const buffer,
				    const int buflen)
{
	char *pos = ERR_PTR(-ENOMEM);
	if (buflen >= 256) {
		pos = dentry_path_raw(dentry, buffer, buflen - 1);
		if (!IS_ERR(pos) && *pos == '/' && pos[1]) {
			struct inode *inode = dentry->d_inode;
			if (inode && S_ISDIR(inode->i_mode)) {
				buffer[buflen - 2] = '/';
				buffer[buflen - 1] = '\0';
			}
		}
	}
	return pos;
}

/**
 * kttech_get_local_path - Get the path of a dentry.
 *
 * @dentry: Pointer to "struct dentry".
 * @buffer: Pointer to buffer to return value in.
 * @buflen: Sizeof @buffer.
 *
 * Returns the buffer on success, an error code otherwise.
 */
static char *kttech_get_local_path(struct dentry *dentry, char * const buffer,
				   const int buflen)
{
	struct super_block *sb = dentry->d_sb;
	char *pos = kttech_get_dentry_path(dentry, buffer, buflen);
	if (IS_ERR(pos))
		return pos;
	/* Convert from $PID to self if $PID is current thread. */
	if (sb->s_magic == PROC_SUPER_MAGIC && *pos == '/') {
		char *ep;
		const pid_t pid = (pid_t) simple_strtoul(pos + 1, &ep, 10);
		if (*ep == '/' && pid && pid ==
		    task_tgid_nr_ns(current, sb->s_fs_info)) {
			pos = ep - 5;
			if (pos < buffer)
				goto out;
			memmove(pos, "/self", 5);
		}
		goto prepend_filesystem_name;
	}
	/* Use filesystem name for unnamed devices. */
	if (!MAJOR(sb->s_dev))
		goto prepend_filesystem_name;
	{
		struct inode *inode = sb->s_root->d_inode;
		/*
		 * Use filesystem name if filesystem does not support rename()
		 * operation.
		 */
		if (inode->i_op && !inode->i_op->rename)
			goto prepend_filesystem_name;
	}
	/* Prepend device name. */
	{
		char name[64];
		int name_len;
		const dev_t dev = sb->s_dev;
		name[sizeof(name) - 1] = '\0';
		snprintf(name, sizeof(name) - 1, "dev(%u,%u):", MAJOR(dev),
			 MINOR(dev));
		name_len = strlen(name);
		pos -= name_len;
		if (pos < buffer)
			goto out;
		memmove(pos, name, name_len);
		return pos;
	}
	/* Prepend filesystem name. */
prepend_filesystem_name:
	{
		const char *name = sb->s_type->name;
		const int name_len = strlen(name);
		pos -= name_len + 1;
		if (pos < buffer)
			goto out;
		memmove(pos, name, name_len);
		pos[name_len] = ':';
	}
	return pos;
out:
	return ERR_PTR(-ENOMEM);
}

/**
 * kttech_get_socket_name - Get the name of a socket.
 *
 * @path:   Pointer to "struct path".
 * @buffer: Pointer to buffer to return value in.
 * @buflen: Sizeof @buffer.
 *
 * Returns the buffer.
 */
static char *kttech_get_socket_name(struct path *path, char * const buffer,
				    const int buflen)
{
	struct inode *inode = path->dentry->d_inode;
	struct socket *sock = inode ? SOCKET_I(inode) : NULL;
	struct sock *sk = sock ? sock->sk : NULL;
	if (sk) {
		snprintf(buffer, buflen, "socket:[family=%u:type=%u:"
			 "protocol=%u]", sk->sk_family, sk->sk_type,
			 sk->sk_protocol);
	} else {
		snprintf(buffer, buflen, "socket:[unknown]");
	}
	return buffer;
}

/**
 * kttech_realpath_from_path - Returns realpath(3) of the given pathname but ignores chroot'ed root.
 *
 * @path: Pointer to "struct path".
 *
 * Returns the realpath of the given @path on success, NULL otherwise.
 *
 * If dentry is a directory, trailing '/' is appended.
 * Characters out of 0x20 < c < 0x7F range are converted to
 * \ooo style octal string.
 * Character \ is converted to \\ string.
 *
 * These functions use kzalloc(), so the caller must call kfree()
 * if these functions didn't return NULL.
 */
char *kttech_realpath_from_path(struct path *path)
{
	char *buf = NULL;
	char *name = NULL;
	unsigned int buf_len = PAGE_SIZE / 2;
	struct dentry *dentry = path->dentry;
	struct super_block *sb;
	if (!dentry)
		return NULL;
	sb = dentry->d_sb;
	while (1) {
		char *pos;
		struct inode *inode;
		buf_len <<= 1;
		kfree(buf);
		buf = kmalloc(buf_len, GFP_NOFS);
		if (!buf)
			break;
		/* To make sure that pos is '\0' terminated. */
		buf[buf_len - 1] = '\0';
		/* Get better name for socket. */
		if (sb->s_magic == SOCKFS_MAGIC) {
			pos = kttech_get_socket_name(path, buf, buf_len - 1);
			goto encode;
		}
		/* For "pipe:[\$]". */
		if (dentry->d_op && dentry->d_op->d_dname) {
			pos = dentry->d_op->d_dname(dentry, buf, buf_len - 1);
			goto encode;
		}
		inode = sb->s_root->d_inode;
		/*
		 * Get local name for filesystems without rename() operation
		 * or dentry without vfsmount.
		 */
		if (!path->mnt || (inode->i_op && !inode->i_op->rename))
			pos = kttech_get_local_path(path->dentry, buf,
						    buf_len - 1);
		/* Get absolute name for the rest. */
		else {
			pos = kttech_get_absolute_path(path, buf, buf_len - 1);
			/*
			 * Fall back to local name if absolute name is not
			 * available.
			 */
			if (pos == ERR_PTR(-EINVAL))
				pos = kttech_get_local_path(path->dentry, buf,
							    buf_len - 1);
		}
encode:
		if (IS_ERR(pos))
			continue;
		name = kttech_encode(pos);
		break;
	}
	kfree(buf);

	return name;
}

static int kttech_sb_mount(char *dev_name, struct path *path,
			  char *type, unsigned long flags, void *data)
{
	int i;
	char *argv[3];
	char *envp[3];
	char *realpath = NULL;

	if(path == NULL)
	{
		return 0;
	}

	realpath = kttech_realpath_from_path(path);

	if(realpath == NULL)
	{
		return 0;
	}

	if(!strcmp("/system/", realpath) && current_uid() == 0){

		for(i=0; i < ARRAY_SIZE(check_rooting_cmd); i++){
			if(!strcmp(check_rooting_cmd[i], current->comm)){
//				printk("kttech : it is not rooting %s\n", check_rooting_cmd[i]);
				return 0;
			}
		}
	
//		printk("kttech : rooting\n");
		printk("kttech : [%s][%d] : %s[%s] %s[%s]\n", current->comm, 
					current_uid(), __func__, dev_name, 
					realpath, type);
		argv[0] = (char *) root_cmd;
		argv[1] = (char *) root_cmd_argv;
		argv[2] = NULL;
		envp[0] = "HOME=/";
		envp[1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
		envp[2] = NULL;
		call_usermodehelper(argv[0], argv, envp, 1);
#if 0
		if(flags & MS_REMOUNT)
			printk("kttech : remount\n");
#endif
	}
	return 0;
}

static int kttech_sb_umount(struct vfsmount *mnt, int flags)
{
//	printk("kttech : %s\n", __func__);
	return 0;
}

struct security_operations kttech_ops = {
	.name =				"kttech_lsm",

	.sb_mount = 			kttech_sb_mount,
	.sb_umount = 			kttech_sb_umount,

};


/**
 * kttech_init - initialize the KTTech lsm
 *
 * Returns 0
 */
static __init int kttech_init(void)
{

	printk(KERN_INFO "KTTech :  Initializing.\n");

	if (!security_module_enable(&kttech_ops))
		return 0;

	printk(KERN_INFO "KTTech :  enabled.\n");

	/*
	 * Register with LSM
	 */
	if (register_security(&kttech_ops))
		panic("kttech: Unable to register with kernel.\n");

	return 0;
}

security_initcall(kttech_init);
