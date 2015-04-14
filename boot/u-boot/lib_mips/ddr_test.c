#include <common.h>
#include <command.h>
#include <malloc.h>
#include <devices.h>
#include <version.h>
#include <net.h>
#include <environment.h>
#include <watchdog.h>
#include <ar7240_soc.h>
#include <asm/addrspace.h>

#include "ddr_test.h"
//#define KSEG1ADDR(a)		((__typeof__(a))(((unsigned long)(a) & 0x1fffffff) | KSEG1))
const static unsigned long long pattern[] = {
	0xaaaaaaaaaaaaaaaaULL,
	0xccccccccccccccccULL,
	0xf0f0f0f0f0f0f0f0ULL,
	0xff00ff00ff00ff00ULL,
	0xffff0000ffff0000ULL,
	0xffffffff00000000ULL,
	0x00000000ffffffffULL,
	0x0000ffff0000ffffULL,
	0x00ff00ff00ff00ffULL,
	0x0f0f0f0f0f0f0f0fULL,
	0x3333333333333333ULL,
	0x5555555555555555ULL
};
const unsigned long long otherpattern = 0x0123456789abcdefULL;
static void move64(unsigned long long *src, unsigned long long *dest)
{
#if defined(CONFIG_MPC8260) || defined(CONFIG_MPC824X)
	asm ("lfd  0, 0(3)\n\t" /* fpr0	  =  *scr	*/
	 "stfd 0, 0(4)"		/* *dest  =  fpr0	*/
	 : : : "fr0" );		/* Clobbers fr0		*/
    return;
#else
	*dest = *src;
#endif
}
int post_log (char *format, ...)
{
	va_list args;
	uint i;
	char printbuffer[CFG_PBSIZE];

	va_start (args, format);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf (printbuffer, format, args);
	va_end (args);

#ifdef CONFIG_LOGBUFFER
	/* Send to the logbuffer */
	logbuff_log (printbuffer);
#else
	/* Send to the stdout file */
	puts (printbuffer);
#endif

	return 0;
}
static int memory_post_test2 (unsigned long start, unsigned long size)
{
	unsigned long i;
	ulong *mem = (ulong *) start;
	ulong readback;
	int ret = 0;

	for (i = 0; i < size / sizeof (ulong); i++) {
		mem[i] = 1 << (i % 32);
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}

	for (i = 0; i < size / sizeof (ulong) && ret == 0; i++) {
		readback = mem[i];
		if (readback != (1 << (i % 32))) {
			post_log ("Memory error at %08x, "
				  "wrote %08x, read %08x !\n",
					  mem + i, 1 << (i % 32), readback);

			ret = -1;
			break;
		}
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}

	return ret;
}

static int memory_post_test3 (unsigned long start, unsigned long size)
{
	unsigned long i;
	ulong *mem = (ulong *) start;
	ulong readback;
	int ret = 0;

	for (i = 0; i < size / sizeof (ulong); i++) {
		mem[i] = i;
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}

	for (i = 0; i < size / sizeof (ulong) && ret == 0; i++) {
		readback = mem[i];
		if (readback != i) {
			post_log ("Memory error at %08x, "
				  "wrote %08x, read %08x !\n",
					  mem + i, i, readback);

			ret = -1;
			break;
		}
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}

	return ret;
}

static int memory_post_test4 (unsigned long start, unsigned long size)
{
	unsigned long i;
	ulong *mem = (ulong *) start;
	ulong readback;
	int ret = 0;

	for (i = 0; i < size / sizeof (ulong); i++) {
		mem[i] = ~i;
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}

	for (i = 0; i < size / sizeof (ulong) && ret == 0; i++) {
		readback = mem[i];
		if (readback != ~i) {
			post_log ("Memory error at %08x, "
				  "wrote %08x, read %08x !\n",
					  mem + i, ~i, readback);

			ret = -1;
			break;
		}
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}

	return ret;
}

static int memory_post_dataline(unsigned long long * pmem)
{
	unsigned long long temp64 = 0;
	int num_patterns = sizeof(pattern)/ sizeof(pattern[0]);
	int i;
	unsigned int hi, lo, pathi, patlo;
	int ret = 0;
	unsigned int val ;
	for ( i = 0; i < num_patterns; i++) {
		move64((unsigned long long *)&(pattern[i]), pmem);
		/*
		 * Put a different pattern on the data lines: otherwise they
		 * may float long enough to read back what we wrote.
		 */
		//move64((unsigned long long *)&otherpattern, pmem--);
		move64(pmem, &temp64);
		if (temp64 != pattern[i]){
			pathi = (pattern[i]>>32) & 0xffffffff;
			patlo = pattern[i] & 0xffffffff;

			hi = (temp64>>32) & 0xffffffff;
			lo = temp64 & 0xffffffff;

			post_log ("Memory (date line) error at %08x, "
				  "wrote %08x%08x, read %08x%08x !\n",
					  pmem, pathi, patlo, hi, lo);
			ret = -1;
			
			//val = ar7240_reg_rd(0x1806001C);
			//val |= 1<<24; 
			//ar7240_reg_wr (0x1806001C, val);
		}
		pmem++;
	}
	if(ret == 0)
		printf(".");
	return ret;
}


uint32_t
test_addr_bus (uint32_t max_addr)
{
	ulong *p1;
	uint32_t	base;
	uint32_t 	offset;
	uint32_t 	failures;
	static	uint32_t	base_address32 = (1ul << 31);
	failures = 0;

	printf("\n\tTesting the address lines");

	/* wrtie value in memory */
	base = 0x08;
	while(1){

		offset = 0x04;
		p1 = base;
		while (1) {
			//debug("Write Operation:\tADDR == 0x%qX\tValue == 0x%qX\n",p1,0x55555555ul);
			//cvmx_write_csr(p1+base_address64,0x55555555ul);
			*(p1+base_address32) = 0x55555555ul;
			//asm volatile ("syncw");
			//flush_l2_cache_ddr_test_specific(p1);
			//flush_cache(p1,32);
			offset = offset << 1;

			if(base & offset){
				base = base << 1;
				break;
			}else{
				p1 = base | offset;
			}
		}

		if(base > max_addr-1){
			break;
		}
	}

	//flush_cache(0,0x1000000ul);
	/* read value from memory */
	base = 0x08;
	while(1){

		offset = 0x04;
		p1 = base;
		while (1) {		
			//debug("Read Operation:\tADDR == 0x%qX\tValue == 0x%qX\n",p1,*(p1+base_address32));
			
			if(*(p1+base_address32)!=0x55555555ull){
				printf("\nError\tADDR == 0x%qX\tRead Value == 0x%qX\tExpected Value == 0x%qX\n",
						p1,*(p1+base_address32),0x55555555);
				failures = failures + 1;
			}

			offset = offset << 1;

			if(base & offset){
				base = base << 1;
				break;
			}else{
				p1 = base | offset;
			}
			
		}

		if(failures == 0)
			printf(".");

		if(base > max_addr-1){
			break;
		}

		
	}
	printf("......");
	if(failures == 0)
		printf(" OK\n");

	return (failures);
	
}

static int memory_post_addrline(ulong *testaddr, ulong *base, ulong size)
{
	ulong *target;
	ulong *end;
	ulong readback;
	ulong xor;
	int   ret = 0;
	end = (ulong *)((ulong)base + size);	/* pointer arith! */
	xor = 0;
	for(xor = sizeof(ulong); xor > 0; xor <<= 1) {
		//printf("##xor=%x",xor);
		target = (ulong *)((ulong)testaddr ^ xor);
		if((target >= base) && (target < end)) {
			*testaddr = ~*target;
			readback  = *target;

#ifdef INJECT_ADDRESS_ERRORS
			if(xor == 0x00008000) {
				readback = *testaddr;
			}
#endif
			if(readback == *testaddr) {
				post_log ("Memory (address line) error at %08x<->%08x, "
				  	"XOR value %08x !\n",
					testaddr, target, xor);
				ret = -1;
			}
			//else 
				//printf(".");
		}
	}
	printf(".");
	return ret;
}

static int memory_post_test1 (unsigned long start,
			      unsigned long size,
			      unsigned long val)
{
	unsigned long i;
	ulong *mem = (ulong *) start;
	ulong readback;
	int ret = 0;

	for (i = 0; i < size / sizeof (ulong); i++) {
		mem[i] = val;
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}

	for (i = 0; i < size / sizeof (ulong) && ret == 0; i++) {
		readback = mem[i];
		if (readback != val) {
			post_log ("Memory error at %08x, "
				  "wrote %08x, read %08x !\n",
					  mem + i, val, readback);

			ret = -1;
			break;
		}
		if (i % 1024 == 0)
			WATCHDOG_RESET ();
	}
	return ret;
}
static int memory_post_addr_tests (unsigned long start, unsigned long size)
{
	int ret = 0;
	if (ret == 0)
		ret = memory_post_addrline ((ulong *)start, (ulong *)start, size);
	WATCHDOG_RESET ();
#if 0	
	if (ret == 0)
		ret = memory_post_addrline ((ulong *)(start + size - 8),
					    (ulong *)start, size);
	WATCHDOG_RESET ();
#endif	
	return ret;
}
static int memory_post_tests (unsigned long start, unsigned long size)
{
	int ret = 0;
	
	if (ret == 0)
		ret = memory_post_dataline ((unsigned long long *)start);
	WATCHDOG_RESET ();
#if 0
	if (ret == 0)
		ret = memory_post_test1 (start, size, 0x00000000);
	WATCHDOG_RESET ();
	if (ret == 0)
		ret = memory_post_test1 (start, size, 0xffffffff);
	WATCHDOG_RESET ();
	if (ret == 0)
		ret = memory_post_test1 (start, size, 0x55555555);
	WATCHDOG_RESET ();
	if (ret == 0)
		ret = memory_post_test1 (start, size, 0xaaaaaaaa);
	WATCHDOG_RESET ();
	if (ret == 0)
		ret = memory_post_test2 (start, size);
	WATCHDOG_RESET ();
	if (ret == 0)
		ret = memory_post_test3 (start, size);
	WATCHDOG_RESET ();
	if (ret == 0)
		ret = memory_post_test4 (start, size);
	WATCHDOG_RESET ();
#endif	

	return ret;
}

void ddr_test(void)
{
	int ret = 0;
	//bd_t *bd = gd->bd;
	//unsigned long memsize = (bd->bi_memsize >= 256 << 20 ?
	//			 256 << 20 : bd->bi_memsize) - (1 << 20);
	//printf("###memsize=%d\n",memsize);
	//return 0;
#if 1
	if (0) {
		printf("now start the slow memory test");
		//ret = memory_post_tests (CFG_SDRAM_BASE, memsize);
	} else {			/* POST_NORMAL */
	
		unsigned long i;
		printf("\tTesting the data lines");
		for (i = 0; i < 32 && ret == 0; i++) {
			if (ret == 0)
				ret = memory_post_tests (i << 20, 0x800);
		}
		if(ret == 0)
			printf(" OK");
		//printf("\n\t\tTesting the addr lines");
		test_addr_bus(0x4000000ul);
#if 0		
		for (i = 0; i < 32 && ret == 0; i++) {
			if (ret == 0)
				ret = memory_post_addr_tests (i << 20, 0x800);
		}
		if(ret == 0)
			printf(" ok");
#endif		
	}
	return ret;
#endif	
	
}


