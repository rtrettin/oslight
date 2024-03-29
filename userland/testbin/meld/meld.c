#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int main(int argc, char *argv[]) {
	static char testwrite1[8] = "01238901";
	static char testwrite2[8] = "45672345";
	static char readbuf[18];
	const char *file1, *file2, *mergefile;
	int fd, rv;

	if(argc == 2) file1 = argv[1];
	else if(argc > 2) errx(1, "invalid usage");

	file1 = "src1";
	file2 = "src2";
	mergefile = "merged";

	printf("creating src1 file\n");
	fd = open(file1, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if(fd < 0) err(1, "%s: open for write", file1);
	rv = write(fd, testwrite1, 8);
	if(rv < 0) err(1, "%s: write", file1);
	rv = close(fd);
	if(rv < 0) err(1, "%s: close", file1);

	printf("creating src2 file\n");
	fd = open(file2, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if(fd < 0) err(1, "%s: open for write", file2);
	rv = write(fd, testwrite2, 8);
	if(rv < 0) err(1, "%s: write", file2);
	rv = close(fd);
	if(rv < 0) err(1, "%s: close", file2);

	printf("merging\n");
	int bytes = meld(file1, file2, mergefile);
	if(bytes < 0) err(1, "%s: merging failed", mergefile);

	printf("reading merged file\n");
	fd = open(mergefile, O_RDONLY);
	if(fd < 0) err(1, "%s: open for read", mergefile);
	rv = read(fd, readbuf, 16);
	if(rv < 0) err(1, "%s: read", mergefile);
	rv = close(fd);
	if(rv < 0) err(1, "%s: close(merge)", mergefile);

	readbuf[17] = 0;
	printf("bytes written: %d \n Contents:\n %s\n", bytes, readbuf);
	printf("passed meld test if contents are 0123456789012345\n");
	return 0;
}
