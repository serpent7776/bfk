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
 * File:      bfk.c
 * Version:   0.30
 * Desc:      brainfuck kit
 * By:        Midgard Serpent
 */

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <XDebug/xdebug.h>
#include "bfk_ex.h"

#define BFKVERINT	30
#define BFKVERSTR	"bfk v0.30"

//==============================================================================
static const char s_help[]=
	BFKVERSTR " by Midgard Serpent\n"
	"usage:\tbfk [-m N] [file.bf]\n";

//==============================================================================
int main(int argc, char** argv)
{
	int ret=0;
	if (argc<2) fprintf(stdout, s_help);
	else
		{
		char* f_in=argv[1];	//pointer to input file name
		const int memsz=0xFFFF;	//default memory size is 64kB

		XDEBUG_BEGIN("bfk.log", BFKVERSTR);
		XDEBUG_WRITE("mem size: %u blocks\n", memsz);

		XDEBUG_WRITE("opening input file: %s\n", f_in);
		int f=open(f_in, O_RDONLY);
		if (f!=-1)
			{
			posix_fadvise(f, 0, 0, POSIX_FADV_NOREUSE);
			struct stat fst;
			const int _r=stat(f_in, &fst);
			XDEBUG_WRITE("result: %i, file size = %u\n", _r, fst.st_size);
			if (_r==0)
				{
				const size_t codesize=fst.st_size;
				uint8_t* code=(uint8_t*)malloc(codesize+1);
				if (code)
					{
					XDEBUG_WRITE("reading code\n");
					read(f, code, codesize);	//TODO:	use mmap?
					code[codesize]=0;
					close(f);
					f=-1;
					XDEBUG_WRITE("loaded code:\n/BEGIN\n%s\n/END\n", code);

					XDEBUG_UWRITE("\n");
					XDEBUG_WRITE("creating bfk_ex\n");
					bfk_ex bfk=bfk_ex_create();
					if (bfk)
						{
						XDEBUG_WRITE("OK.\nSelecting created bfk_ex as current\n");
						bfk_ex_makecurrent(bfk);
						XDEBUG_WRITE("setting code to execute\n");
						bfk_ex_codeset(code, codesize);
						XDEBUG_WRITE("setting memory size to %u\n", memsz);
						bfk_ex_memsize(memsz);

						XDEBUG_WRITE("clearing memory\n");
						bfk_ex_memclr();
						XDEBUG_WRITE("executing bf code\n");
						bfk_ex_exec(0);
						XDEBUG_WRITE("done.\n");

						bfk_ex_makecurrent(NULL);
						bfk_ex_destroy(bfk);
						}
					else {fprintf(stdout, "ERROR: Cannot create bfk_ex\n"); ret=1;}
					free(code);
					}
				else {fprintf(stdout, "ERROR: Cannot allocate enough memory\n"); ret=2;}
				}
			else {fprintf(stdout, "ERROR: Cannot get input file size\n"); ret=3;}

			if (f!=-1)
				close(f);
			}
		else {fprintf(stdout, "ERROR: Cannot open input file %s\n", f_in); ret=4;}
		XDEBUG_END();
		}
	return ret;
}
//==============================================================================
