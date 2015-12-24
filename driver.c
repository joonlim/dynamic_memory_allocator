#include "sfmm.c"

struct test
{
	int ints[50];
	char chars[40];
	double doubles[60];

};

int main(int argc, char *argv[])
{
	// 32/29/3
	
	// int64 requested_size = 8;
	// int64 region_size = 32;
	// int8 a = 1;

	// int64 header_word = PACK(requested_size, region_size, a);

	// printf("\nsizeof int: %lu\n", sizeof(int));
	// printf("requested_size << 32: %lu\n", requested_size << 32);

	// printf("header value: %lu\n", header_word);

	// int64 memory = 0; 

	// int64 *header = &memory;
	// PUT(header, header_word);

	// printf("requested size: %lu\n", GET_REQUESTED_SIZE(header));
	// printf("region size: %lu\n", GET_REGION_SIZE(header));
	// printf("allocated bit: %lu\n", GET_ALLOC(header));

	//size_t size = sizeof(long double);

	// printf("for a requested size of %luB the region size including header/footer is %luB\n\n", size, REGION_SIZE(size));
	
	// INITIALIZE
	sf_mem_init();

// malloc a requested space of 16 Bytes
	// void* ptr1 = sf_malloc(size);
	// print_region_stats(ptr1);

	int* ptr0 = (int*)sf_calloc(12, sizeof(int));
	//print_region_stats(ptr0);

	int i = 0;
	int* ptr = ptr0;
	for(; i<12; i++)
	{
		printf("%p - ", ptr0);
		printf("%d\n", *ptr0);
		ptr++;
	}


	void* ptr1 = sf_malloc(3980);
	print_region_stats(ptr1);	

	void* ptr2 = sf_malloc(12);
		print_region_stats(ptr2);

	sf_free(ptr1);	

	sf_free(ptr0);

	struct test *t = (struct test*)sf_malloc(sizeof(struct test));
	t->ints[40] = 5;
	t->chars[10] = 'a';
	t->doubles[15] = 9.9;

	printf("%d %c %f\n", t->ints[40], t->chars[10], t->doubles[15]);

	int64 max = 429490000;
	sf_malloc(max);


 print_all_regions();
 


printf("\n\n");
 
 sf_snapshot();



	return EXIT_SUCCESS;
}
