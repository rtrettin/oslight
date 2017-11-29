/*
 * File-related system call implementations.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>

/*
 * open() - get the path with copyinstr, then use openfile_open and
 * filetable_place to do the real work.
 */
int
sys_open(const_userptr_t upath, int flags, mode_t mode, int *retval)
{
	const int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;

	char *kpath;
	struct openfile *file;
	int result = 0;

	if(flags == allflags) {
		return EINVAL;
	}
	result = copyinstr(upath, kpath, PATH_MAX, NULL);
	if(result) {
		return EFAULT;
	}
	result = openfile_open(kpath, flags, mode, &file);
	if(result) {
		return EFAULT;
	}
	result = filetable_place(curproc->p_filetable, file, retval);
	if(result) {
		return EMFILE;
	}
	kfree(kpath);


	return result;
}

/*
 * read() - read data from a file
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
       int result = 0;

	struct openfile *file;
	struct iovec iov;
	struct uio uio_user;
	char *buffer = (char*)kmalloc(size);
	result = filetable_get(curproc->p_filetable, fd, &file);
	if(result) {
		return result;
	}
	if(file->of_accmode == O_WRONLY) {
		return EPERM;
	}
	uio_kinit(&iov, &uio_user, buffer, size, file->of_offset, UIO_READ);
	result = VOP_READ(file->of_vnode, &uio_user);
	if(result) {
		return result;
	}
	result = copyout(buffer, (userptr_t)buf, size);
	if(result) {
		return result;
	}
	file->of_offset = uio_user.uio_offset;
	filetable_put(curproc->p_filetable, fd, file);
	*retval = size - uio_user.uio_resid;

       return result;
}

/*
 * write() - write data to a file
 */
int sys_write(int fd, userptr_t buf, size_t size, int *retval) {
	int result = 0;
	struct openfile *file;
	struct iovec iov;
	struct uio uio_user;
	size_t size1;
	char *buffer = (char*)kmalloc(size);
	int sizeInt = size;

	result = filetable_get(curproc->p_filetable, fd, &file);
	if(result) {
		return result;
	}
	if(file->of_accmode == O_RDONLY) {
		return EPERM;
	}
	copyinstr((userptr_t)buf, buffer, sizeInt, &size1);
	uio_kinit(&iov, &uio_user, buffer, size, file->of_offset, UIO_WRITE);
	result = VOP_WRITE(file->of_vnode, &uio_user);
	if(result) {
		return result;
	}
	file->of_offset = uio_user.uio_offset;
	filetable_put(curproc->p_filetable, fd, file);
	*retval = size - uio_user.uio_resid;
	kfree(buffer);
	return result;
}

/*
 * close() - remove from the file table.
 */
int sys_close(int fd) {
	struct openfile *file = NULL;
	struct openfile *oldfile;

	if(filetable_okfd(curproc->p_filetable, fd)) {
		filetable_placeat(curproc->p_filetable, file, fd, &oldfile);
		if(oldfile != NULL) {
			openfile_decref(oldfile);
		}
	}
	return 0;
}

/* 
* meld () - combine the content of two files word by word into a new file
*/
int sys_meld(const_userptr_t ufile1, const_userptr_t ufile2, const_userptr_t umerged, int *retval) {
	char *kfile1, *kfile2, *kmerged;
	struct openfile *file1, *file2, *filemerge;
	struct uio read1uio, read2uio, writeuio;
	struct iovec iov;
	int result = 0;
	void *kbuf1, *kbuf2;

	if(ufile1 == NULL || ufile2 == NULL || umerged == NULL) {
		return EINVAL;
	}

	kfile1 = (char *) kmalloc(sizeof(char) * PATH_MAX);
	kfile2 = (char *) kmalloc(sizeof(char) * PATH_MAX);
	kmerged = (char *) kmalloc(sizeof(char) * PATH_MAX);

	result = copyinstr(ufile1, kfile1, strlen(((char *)ufile1)) + 1, NULL);
	if(result) return result;

	result = copyinstr(ufile2, kfile2, strlen(((char *)ufile2)) + 1, NULL);
	if(result) return result;

	result = copyinstr(umerged, kmerged, strlen(((char *)umerged)) + 1, NULL);
	if(result) return result;

	result = openfile_open(kfile1, O_RDONLY, 055, &file1);
	if(result) return result;

	result = openfile_open(kfile2, O_RDONLY, 055, &file2);
	if(result) return result;

	result = openfile_open(kmerged, O_WRONLY|O_CREAT|O_EXCL, 0644, &filemerge);
	if(result) return result;

	kbuf1 = kmalloc(sizeof(char) * 4);
	kbuf2 = kmalloc(sizeof(char) * 4);
	int file1LastOffset = 0, file2LastOffset = 0;
	int loopcnt = 0, file1Read = -1, file2Read = -1;

	while(file1Read != 0 && file2Read != 0) {
		lock_acquire(file1->of_offsetlock);
		uio_kinit(&iov, &read1uio, kbuf1, 4, file1->of_offset, UIO_READ);
		result = VOP_READ(file1->of_vnode, &read1uio);
		file1Read = read1uio.uio_offset - file1LastOffset;
		file1->of_offset = read1uio.uio_offset;
		file1LastOffset = read1uio.uio_offset;
		lock_release(file1->of_offsetlock);

		lock_acquire(file2->of_offsetlock);
		uio_kinit(&iov, &read2uio, kbuf2, 4, file2->of_offset, UIO_READ);
		result = VOP_READ(file2->of_vnode, &read2uio);
		file2Read = read2uio.uio_offset - file2LastOffset;
		file2->of_offset = read2uio.uio_offset;
		file2LastOffset = read2uio.uio_offset;
		lock_release(file2->of_offsetlock);

		if((file1Read > 0 && file1Read < 4) || (file2Read > 0 && file1Read == 0)) {
			char * ptr = (char *) kbuf1;
			int start = file1Read;
			if(file1Read == 0) start = 1;
			for(loopcnt = start - 1; loopcnt < 4; loopcnt++) {
				ptr[loopcnt] = ' ';
			}
		}

		if((file2Read > 0 && file2Read < 4) || (file1Read > 0 && file2Read == 0)) {
			char * ptr = (char *) kbuf2;
			int start = file2Read;
			if(file2Read == 0) start = 1;
			for(loopcnt = start - 1; loopcnt < 4; loopcnt++) {
				ptr[loopcnt] = ' ';
			}
		}

		if(file1Read != 0 || file2Read != 0) {
			lock_acquire(filemerge->of_offsetlock);
			uio_kinit(&iov, &writeuio, kbuf1, 4, filemerge->of_offset, UIO_WRITE);
			result = VOP_WRITE(filemerge->of_vnode, &writeuio);
			filemerge->of_offset = writeuio.uio_offset;
			lock_release(filemerge->of_offsetlock);

			lock_acquire(filemerge->of_offsetlock);
			uio_kinit(&iov, &writeuio, kbuf2, 4, filemerge->of_offset, UIO_WRITE);
			result = VOP_WRITE(filemerge->of_vnode, &writeuio);
			filemerge->of_offset = writeuio.uio_offset;
			lock_release(filemerge->of_offsetlock);
		}
	}

	*retval = filemerge->of_offset;
	openfile_decref(file1);
	openfile_decref(file2);
	openfile_decref(filemerge);
	kfree(kuser1);
	kfree(kuser2);
	kfree(kmerged);
	kfree(kbuf1);
	kfree(kbuf2);
	return 0;
}
