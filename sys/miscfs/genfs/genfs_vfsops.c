/*	$NetBSD: genfs_vfsops.c,v 1.8 2018/10/05 09:51:55 hannken Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: genfs_vfsops.c,v 1.8 2018/10/05 09:51:55 hannken Exp $");

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/fstrans.h>
#include <sys/statvfs.h>
#include <sys/vnode.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>

int
genfs_statvfs(struct mount *mp, struct statvfs *sbp)
{

	sbp->f_bsize = DEV_BSIZE;
	sbp->f_frsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1k to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_bresvd = 0;
	sbp->f_files = 0;
	sbp->f_ffree = 0;
	sbp->f_favail = 0;
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);

	return 0;
}

int
genfs_renamelock_enter(struct mount *mp)
{
	mutex_enter(&mp->mnt_renamelock);
	/* Preserve possible error return in case we become interruptible. */
	return 0;
}

void
genfs_renamelock_exit(struct mount *mp)
{
	mutex_exit(&mp->mnt_renamelock);
}

int
genfs_suspendctl(struct mount *mp, int cmd)
{
	int error;
	int error2 __diagused;

	if ((mp->mnt_iflag & IMNT_HAS_TRANS) == 0)
		return EOPNOTSUPP;

	switch (cmd) {
	case SUSPEND_SUSPEND:
		error = fstrans_setstate(mp, FSTRANS_SUSPENDING);
		if (error)
			return error;
		error = fstrans_setstate(mp, FSTRANS_SUSPENDED);
		if (error == 0) {
			if ((mp->mnt_iflag & IMNT_GONE) != 0)
				error = ENOENT;
			if (error) {
				error2 = fstrans_setstate(mp, FSTRANS_NORMAL);
				KASSERT(error2 == 0);
			}
		}
		return error;

	case SUSPEND_RESUME:
		error2 = fstrans_setstate(mp, FSTRANS_NORMAL);
		KASSERT(error2 == 0);
		return 0;

	default:
		panic("%s: bogus command %d", __func__, cmd);
	}
}
