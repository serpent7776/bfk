/*
 * Copyright © 2013 Midgard Serpent. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Project:   bfk
 * File:      bfk_ex.c
 * Version:   0.30
 * Desc:      brainfuck kit
 * By:        Midgard Serpent
 *
 * brainfuck interpreter
 */

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <XDebug/xdebug.h>
#include "bfk_ex.h"

typedef struct{
	uint32_t	size;		//size of loop
	uint32_t	iloop_no;	//number of first inner loop
	uint32_t	oloop_no;	//number of next outer (sibling) loop
}loopdt_item;

struct _bfk_ex{
	uint8_t*	mem;		//pointer to array of bytes used as memory
	uint32_t	mem_size;	//size of memory in bytes
	uint32_t	mem_ptr;	//position of memory pointer
	uint8_t*	code;		//pointer to buffer for bf code to be executed
	uint32_t	code_size;	//size of buffer for bf code
	uint32_t	code_ptr;	//position of code pointer
	loopdt_item*	loopdt;		//pointer to loop data; array of loopdt_item's
	uint32_t	loopdt_size;	//size of loopdt
};

//declare private function as static
static void bfk_ex_init(void);
static uint32_t bfk_ex_code_prepare(uint8_t*, const char*, uint32_t);
static uint32_t bfk_ex_loopstat(uint8_t*, uint32_t);
static int bfk_ex_mkloopdt(loopdt_item*, uint32_t, uint8_t*, uint32_t);

//const data
static const char* bf_cmds="+-<>,.[]";	//bf comands
#define BFCMD_LOOPSTART_POS	7	/* position of '[' command in bf_cmds */
#define BFCMD_LOOPEND_POS	8	/* position of ']' command in bf_cmds */
static const char* bf_loop_cmds="\x0007\x0008";
/*static const uint8_t bfcmd_loopstart_pos=7;	//position of '[' command in bf_cmds*/
/*static const uint8_t bfcmd_loopend_pos=8;	//position of ']' command in bf_cmds*/

//global variables used by bfk_ex
static pthread_key_t	bfk_key;
static pthread_once_t	bfk_keyonce=PTHREAD_ONCE_INIT;

// public functions: {{{
/*
 * create bfk_ex object
 * parameters:	none
 * return: bfk_ex
 */
bfk_ex bfk_ex_create(void)/*{{{*/
{
	bfk_ex bfk=malloc(sizeof(struct _bfk_ex));
	if (bfk){
		bfk->mem=NULL;
		bfk->mem_size=0;
		bfk->mem_ptr=0;
		bfk->code=NULL;
		bfk->code_size=0;
		bfk->code_ptr=0;
		bfk->loopdt=NULL;
		bfk->loopdt_size=0;
		return bfk;
	}
	return NULL;
}/*}}}*/

/*
 * Destroy bfk_ex created by bfk_ex_create
 * params:
 * 	bfk_ex bfk: bfk_ex to be destroyed
 * return: nothing
 */
void bfk_ex_destroy(bfk_ex bfk)/*{{{*/
{
	if (bfk){
		free(bfk->mem);
		free(bfk->code);
		free(bfk->loopdt);
		free(bfk);
	}
}/*}}}*/

/*
 * Set given bfk_ex as current
 * parmas:
 * 	bfk_ex bfk: bfk_ex to be set as current
 * return: previously selected bfkex
 * remarks:
 * 	passing bfk set to NULL will cause to deselect current bfk_ex.
 */
bfk_ex bfk_ex_makecurrent(bfk_ex bfk)/*{{{*/
{
	pthread_once(&bfk_keyonce, bfk_ex_init);
	bfk_ex bfk_old=(bfk_ex)pthread_getspecific(bfk_key);
	pthread_setspecific(bfk_key, bfk);
	return	bfk_old;
}/*}}}*/

/*
 * Change size of memory for currently selected interpreter
 * params:
 * 	uint16_t size:	new size of memory
 * return:
 * 	previous size of memory
 */
uint32_t bfk_ex_memsize(uint32_t size)/*{{{*/
{
	bfk_ex bfk=(bfk_ex)pthread_getspecific(bfk_key);
	if (bfk){
		const uint32_t memsize=bfk->mem_size;
		if (size>0){
			void* mem=realloc(bfk->mem, size);
			if (mem){
				bfk->mem=mem;
				bfk->mem_size=size;
			}
		}
		return memsize;
	}
	return 0;
}/*}}}*/

/*
 * Return pointer to memory for current interpreter
 * Params:
 * 	none
 * Return:
 * 	pointer to memory for current interpreter
 */
uint8_t* bfk_ex_memget()/*{{{*/
{
	bfk_ex bfk=(bfk_ex)pthread_getspecific(bfk_key);
	if (bfk){
		return bfk->mem;
	}
	return 0;
}/*}}}*/

/*
 * Clear memory
 * Params:
 * 	none
 * Return:
 * 	none
 */
void bfk_ex_memclr(void)/*{{{*/
{
	bfk_ex bfk=(bfk_ex)pthread_getspecific(bfk_key);
	if (bfk){
		memset(bfk->mem, 0, bfk->mem_size);
	}
}/*}}}*/

/*
 *
 */
int bfk_ex_codeset(const char* code, uint32_t size)/*{{{*/
{
	XDEBUG_WRITE("bfk_ex: setting code to %s\n", code);
	bfk_ex bfk=(bfk_ex)pthread_getspecific(bfk_key);
	if (bfk && code && size>0){
		void* buf_code=realloc(bfk->code, size);
		if (buf_code){
			bfk->code=buf_code;
			bfk->code_size=size;
			const uint32_t buf_code_size=bfk_ex_code_prepare(buf_code, code, size);
			if (buf_code_size>0){
				uint32_t loopcount=bfk_ex_loopstat(buf_code, buf_code_size);
				void* buf_loopdt=realloc(bfk->loopdt, (loopcount+1)*sizeof(loopdt_item));
				if (buf_loopdt){
					bfk->loopdt=buf_loopdt;
					bfk->loopdt_size=loopcount;
					return bfk_ex_mkloopdt(buf_loopdt, loopcount, buf_code, buf_code_size);
				}
			}
		}
	}
	XDEBUG_WRITE("bfk_ex: setting code FAILED!");
	return 0;
}/*}}}*/

/*
 * return first nonzero value
 * Params:
 * 	a:	first value
 * 	b:	second value
 * Return:
 * 	if a is nonzero return a; otherwise return b
 */
inline uint32_t nzero(uint32_t a, uint32_t b)/*{{{*/
{
	return a ? a : b;
}/*}}}*/

/*
 * Execute prepared bf code.
 * Parameters:
 * 	steps:	max. number of commands to execute; when 0 unlimited number of steps are ececuted
 * Return:
 * 	nonzero if succeeded; zero otherwise
 */
int bfk_ex_exec(uint32_t steps)/*{{{*/
{
	XDEBUG_WRITE("bfk_ex: executing %u steps\n", steps);
	bfk_ex bfk=(bfk_ex)pthread_getspecific(bfk_key);
	void* jmptab[]={&&bf_end, &&bf_inc, &&bf_dec, &&bf_left, &&bf_right, &&bf_read, &&bf_write, &&bf_loopstart, &&bf_loopend};
	if (bfk){
		XDEBUG_UWRITE("BEGIN\n");
		char buf;	//buffer for I/O
		uint32_t loop_no=0;	//current loop number
		for(; bfk->code[bfk->code_ptr]; bfk->code_ptr++){
			goto *jmptab[bfk->code[bfk->code_ptr]];
			bf_inc:
				XDEBUG_UWRITE("+");
				bfk->mem[bfk->mem_ptr]++;
				goto bf_nextcmd;
			bf_dec:
				XDEBUG_UWRITE("-");
				bfk->mem[bfk->mem_ptr]--;
				goto bf_nextcmd;
			bf_left:
				XDEBUG_UWRITE("<");
				bfk->mem_ptr--;
				if (bfk->mem_ptr<0)
					return 1;
				goto bf_nextcmd;
			bf_right:
				XDEBUG_UWRITE(">");
				bfk->mem_ptr++;
				if (bfk->mem_ptr>bfk->mem_size){
					const uint32_t newsize=bfk->mem_size<<1;
					XDEBUG_UWRITE("\nbfk_ex: resizing memory to %u cells\n", newsize);
					void* pmem=realloc(bfk->mem, newsize);
					if (pmem){
						bfk->mem=pmem;
						bfk->mem_size=newsize;
					} else return 2;
				}
				goto bf_nextcmd;
			bf_read:
				XDEBUG_UWRITE(",");
				read(STDIN_FILENO, &buf, 1);
				bfk->mem[bfk->mem_ptr]=buf;
				goto bf_nextcmd;
			bf_write:
				XDEBUG_UWRITE(".");
				buf=bfk->mem[bfk->mem_ptr];
				write(STDOUT_FILENO, &buf, 1);
				goto bf_nextcmd;
			bf_loopstart:
				{
				XDEBUG_UWRITE("[#%u", loop_no);
				const loopdt_item* loopdt=&bfk->loopdt[loop_no];
				if (bfk->mem[bfk->mem_ptr]==0){
					bfk->code_ptr+=loopdt->size;
					loop_no=nzero(loopdt->oloop_no, loop_no);
					XDEBUG_UWRITE("j%u", loop_no);
				}else loop_no=nzero(loopdt->iloop_no, loop_no);
				goto bf_nextcmd;
				}
			bf_loopend:
				{
				XDEBUG_UWRITE("]#%u", loop_no);
				const loopdt_item* loopdt=&bfk->loopdt[loop_no];
				if (bfk->mem[bfk->mem_ptr]!=0){
					bfk->code_ptr-=loopdt->size;
					loop_no=nzero(loopdt->iloop_no, loop_no);
					XDEBUG_UWRITE("j%u", loop_no);
				}else loop_no=nzero(loopdt->oloop_no, loop_no);
				goto bf_nextcmd;
				}
			bf_nextcmd:
			;
		}
		bf_end:
		XDEBUG_UWRITE("\nEND\n");
	}
	return 0;
}/*}}}*/
///}}}
// private functions: {{{
/*
 * initlialize pthread
 */
void bfk_ex_init(void)/*{{{*/
{
	pthread_key_create(&bfk_key, NULL);	//TODO: use _bfk_ex_destroy as destructor?
}/*}}}*/

/*
 * Prepare given bf code. Encode into internal code and write to given buffer.
 * Parameters:
 * 	buf:	buffer where encoded code should be placed
 * 	code:	bf code to be encoded
 * 	size:	size of code in bytes
 * Return:
 * 	number of bytes written to buf if successful; 0 otherwise.
 */
uint32_t bfk_ex_code_prepare(uint8_t* buf, const char* code, uint32_t size) /*{{{*/
{
	XDEBUG_UWRITE("\n");
	XDEBUG_WRITE("bfk_ex: preparing bf code\n");
	if (buf && code && size>0){
		uint32_t cnt=0;	//bytes written to buf
		for(uint32_t i=0; code[i]; ++i){
			char* pos=strchr(bf_cmds, code[i]);
			if (pos){
				const int index=pos-bf_cmds+1;
				buf[cnt++]=index;
			}
		}
		//write '\0' to buf - exit command
		buf[cnt++]=0;
		XDEBUG_WRITE("bfk_ex: generated code: size=%u, data=%s\n", cnt, buf);
		return cnt;
	}
	XDEBUG_WRITE("bfk_ex: FAILED");
	return 0;
}/*}}}*/

/*
 * Count number of loops in given code
 * Params:
 * 	code:	encoded code
 * 	size:	size of code in bytes
 * Return:
 * 	number of loops if successful; uint_32_t(-1) otherwise
 */
uint32_t bfk_ex_loopstat(uint8_t* code, uint32_t size)/*{{{*/
{
	XDEBUG_UWRITE("\n");
	XDEBUG_WRITE("bfk_ex: stating loops in code\n");
	if (code && size>0){
		uint32_t cnt=0;	//number of loops
		uint8_t* s=code;
		while(s=strchr(s, BFCMD_LOOPSTART_POS)){
			++cnt;
			++s;
			}
		XDEBUG_WRITE("bfk_ex: %u loops found\n", cnt);
		return cnt;
	}
	XDEBUG_WRITE("bfk_ex: FAILED\n");
	return (uint32_t)(-1);
}/*}}}*/

/*
 * Create loop data needed by bfk interpreter.
 * Parameters:
 * 	loopdt:	pointer to array of loopdt_item's where loop data should be placed
 * 	loopcount:
 * 	code:	encoded bf code
 * 	size:	size of code
 * Return:
 * 	nonzero if successful; zero otherwise.
 */
int bfk_ex_mkloopdt(loopdt_item* loopdt, uint32_t loopcount, uint8_t* code, uint32_t size)/*{{{*/
{
	XDEBUG_WRITE("bfk_ex: making loop data\n");
	if (loopdt && code && size>0){
		uint32_t* loopst=malloc(sizeof(uint32_t)*loopcount);
		if (loopst){
			uint32_t loopst_ptr=0;
			uint32_t loop_no=0;
			uint32_t loop_max=0;	//max. numer of loop
			for(uint32_t i=0; code[i]; ++i){
				switch(code[i]){
					case BFCMD_LOOPSTART_POS:
						loopdt[loop_max].size=i;
						loopdt[loop_max].iloop_no=loop_max;
						loopst[loopst_ptr++]=loop_max;
						loop_max++;
						break;
					case BFCMD_LOOPEND_POS:
						loop_no=loopst[--loopst_ptr];
						loopdt[loop_no].size=i-loopdt[loop_no].size;
						loopdt[loop_no].iloop_no= loop_max-loopdt[loop_no].iloop_no-1 ? loop_no+1 : 0;
						const char* pos=strpbrk(&code[i+1], bf_loop_cmds);
						if (pos){
							loopdt[loop_no].oloop_no= *pos==BFCMD_LOOPSTART_POS ? loop_max : loopst[loopst_ptr-1];
						}else{
							loopdt[loop_no].oloop_no=0;
						}
						break;
				}
			}
			free(loopst);
			#if XDEBUG
			XDEBUG_WRITE("bfk_ex: generated loopdt:\n");
			for(uint32_t i=0; i<loop_max; ++i){
				XDEBUG_UWRITE("\tloop %u: size=%u, iloop_no=%u, oloop_no=%u\n", i, loopdt[i].size, loopdt[i].iloop_no, loopdt[i].oloop_no);
			}
			#endif
		}
		return 1;
	}
	XDEBUG_WRITE("bfk_ex: FAILED\n");
	return 0;
}/*}}}*/
///}}}
