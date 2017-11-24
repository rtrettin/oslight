#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int main(int argc, char *argv[]) {
	static char writebuf1[8] = "01238901";
	static char writebuf2[8] = "45672345";
	static char readbuf[18];
	const char *file1, *file2, *mergefile;
	int fd, rv;

	if(argc == 2) file1 = argv[1];
	else if(argc > 2) errx(1, "usage: meld");

	file1 = "source1";
	file2 = "source2";
	mergefile = "merged";

	printf("creating source1 file\n");
	fd = open(file1, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if(fd < 0) err(1, "%s: open for write", file1);
	rv = write(fd, writebuf1, 8);
	if(rv < 0) err(1, "%s: write", file1);
	rv = close(fd);
	if(rv < 0) err(1, "%s: close", file1);

	printf("creating source2 file\n");
	fd = open(file2, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if(fd < 0) err(1, "%s: open for write", file2);
	rv = write(fd, writebuf2, 8);
	if(rv < 0) err(1, "%s: write", file2);
	rv = close(fd);
	if(rv < 0) err(1, "%s: close", file2);

	printf("melding\n");
	int bytes = meld(file1, file2, mergefile);
	if(bytes < 0) err(1, "%s: merging", mergefile);

	printf("reading merged file\n");
	fd = open(mergefile, O_RDONLY);
	if(fd < 0) err(1, "%s: open for read", mergefile);
	rv = read(fd, readbuf, 16);
	if(rv < 0) err(1, "%s: read", mergefile);
	rv = close(fd);
	if(rv < 0) err(1, "%s: close(merge)", mergefile);

	readbuf[17] = 0;
	printf("bytes written: %d \n Contents:\n %s\n", bytes, readbuf);
	printf("passed meld test if contents line is 0123456789012345\n");
	return 0;
}
