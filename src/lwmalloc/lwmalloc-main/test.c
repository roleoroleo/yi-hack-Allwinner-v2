#include <stdlib.h>
#include <stdio.h>

// Snippet to print current memory usage in C
void print_mem_usage() {
    FILE* fp = fopen("/proc/self/status", "r");
    char line[128];
    while (fgets(line, 128, fp)) {
        if (strncmp(line, "Vm", 2) == 0) {
            printf("%s", line);
        }
    }
    fclose(fp);
}

int main(void) {
	print_mem_usage();
	printf("##### 1 #####\n");
	void * ptr = malloc(sizeof(int));
	print_mem_usage();
	printf("##### 2 #####\n");
	realloc(ptr, sizeof(long));
	print_mem_usage();
	printf("##### 3 #####\n");
	free(ptr);
	print_mem_usage();
	printf("##### 4 #####\n");
	ptr = calloc(2, sizeof(int));
	print_mem_usage();
	printf("##### 5 #####\n");
	free(ptr);
	print_mem_usage();
	printf("##### test success #####\n");
	print_mem_usage();
	return 0;
}
