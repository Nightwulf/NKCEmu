/***************************************************************************
 *   Copyright (C) 2007 by Torsten Evers   *
 *   tevers@onlinehome.de   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/**
 * Emulator für den NDR-Klein-Computer
 * Ausbaustufe SBC2+GDP64+KEY+ROA64
 * Basis: Z80-Emulator z80pack von Udo Munk
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <memory.h>
#include <stdbool.h>
#include "sim.h"
#include "simglb.h"
#include "nkcglobal.h"

extern void int_on(void), int_off(void), mon(void);
extern void 


init_io(bool), exit_io(void);
extern int exatoi(char *);
int CAS_FILE=0;

/*
 *  This function saves the CPU and the memory into the file core.z80
 *
 */
static void save_core(void)
{
    int fd;

    if ((fd = open("core.z80", O_WRONLY | O_CREAT, 0600)) == -1)
    {
        puts("can't open file core.z80");
        return;
    }
    write(fd, (char *) &A, sizeof(A));
    write(fd, (char *) &F, sizeof(F));
    write(fd, (char *) &B, sizeof(B));
    write(fd, (char *) &C, sizeof(C));
    write(fd, (char *) &D, sizeof(D));
    write(fd, (char *) &E, sizeof(E));
    write(fd, (char *) &H, sizeof(H));
    write(fd, (char *) &L, sizeof(L));
    write(fd, (char *) &A_, sizeof(A_));
    write(fd, (char *) &F_, sizeof(F_));
    write(fd, (char *) &B_, sizeof(B_));
    write(fd, (char *) &C_, sizeof(C_));
    write(fd, (char *) &D_, sizeof(D_));
    write(fd, (char *) &E_, sizeof(E_));
    write(fd, (char *) &H_, sizeof(H_));
    write(fd, (char *) &L_, sizeof(L_));
    write(fd, (char *) &I, sizeof(I));
    write(fd, (char *) &IFF, sizeof(IFF));
    write(fd, (char *) &R, sizeof(R));
    write(fd, (char *) &PC, sizeof(PC));
    write(fd, (char *) &STACK, sizeof(STACK));
    write(fd, (char *) &IX, sizeof(IX));
    write(fd, (char *) &IY, sizeof(IY));
    write(fd, (char *) ram, 32768);
    write(fd, (char *) ram + 32768, 32768);
    close(fd);
}

/*
 *  This function loads the CPU and memory from the file core.z80
 *
 */
static void load_core(void)
{
    int fd;

    if ((fd = open("core.z80", O_RDONLY)) == -1)
    {
        puts("can't open file core.z80");
        return;
    }
    read(fd, (char *) &A, sizeof(A));
    read(fd, (char *) &F, sizeof(F));
    read(fd, (char *) &B, sizeof(B));
    read(fd, (char *) &C, sizeof(C));
    read(fd, (char *) &D, sizeof(D));
    read(fd, (char *) &E, sizeof(E));
    read(fd, (char *) &H, sizeof(H));
    read(fd, (char *) &L, sizeof(L));
    read(fd, (char *) &A_, sizeof(A_));
    read(fd, (char *) &F_, sizeof(F_));
    read(fd, (char *) &B_, sizeof(B_));
    read(fd, (char *) &C_, sizeof(C_));
    read(fd, (char *) &D_, sizeof(D_));
    read(fd, (char *) &E_, sizeof(E_));
    read(fd, (char *) &H_, sizeof(H_));
    read(fd, (char *) &L_, sizeof(L_));
    read(fd, (char *) &I, sizeof(I));
    read(fd, (char *) &IFF, sizeof(IFF));
    read(fd, (char *) &R, sizeof(R));
    read(fd, (char *) &PC, sizeof(PC));
    read(fd, (char *) &STACK, sizeof(STACK));
    read(fd, (char *) &IX, sizeof(IX));
    read(fd, (char *) &IY, sizeof(IY));
    read(fd, (char *) ram, 32768);
    read(fd, (char *) ram + 32768, 32768);
    close(fd);
}

int main(int argc, char *argv[])
{
    register char *s, *p;
    register char *pn = argv[0];
    CAS_FILE=0;
    bool windowed=false;
    
    while (--argc > 0 && (*++argv)[0] == '-')
    {
        for (s = argv[0] + 1; *s != '\0'; s++)
        {
            switch (*s)
            {
                case 's':   /* save core and CPU on exit */
                    s_flag = 1;
                    break;
                case 'l':   /* load core and CPU from file */
                    l_flag = 1;
                    break;
                case 'h':   /* execute HALT opcode */
                    break_flag = 0;
                    break;
                case 'i':   /* trap I/O on unused ports */
                    i_flag = 1;
                    break;
                case 'm':   /* initialize Z80 memory */
                    m_flag = exatoi(s+1);
                    s += strlen(s+1);
                    break;
                case 'f':
                    f_flag = atoi(s+1);
                    s += strlen(s+1);
                    tmax = f_flag * 10000;
                    break;
                case 'x':   /* get filename with Z80 executable */
                    x_flag = 1;
                    s++;
                    p = xfn;
                    while (*s)
                        *p++ = *s++;
                    *p = '\0';
                    s--;
                    break;
                case 'b':   /* get filename with Z80 EEPROM content */
                    b_flag = 1;
                    s++;
                    p = xfn;
                    while (*s)
                        *p++ = *s++;
                    *p = '\0';
                    s--;
                    break;
		case 'c':
		{
		    s++;
		    char buf[strlen(s)];
                    p = buf;
                    while (*s)
                        *p++ = *s++;
                    *p = '\0';
                    s--;
		    if ((CAS_FILE = open(buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
		    {
			puts("can't open CAS file");
			return;
		    }
		    break;
		}
		case 'w':
		{
		    windowed=true;
		    break;
		}
		case 'a':
		{
		  PC=PC+exatoi(s+1);
		  s += strlen(s+1);
		  break;
		}
                case '?':
                    goto usage;
                default:
                    printf("illegal option %c\n", *s);
    usage:          printf("usage:\t%s -s -l -i -h -mn -fn -xfilename\n", pn);
                    puts("\ts = save core and cpu");
                    puts("\tl = load core and cpu");
                    puts("\ti = trap on I/O to unused ports");
                    puts("\th = execute HALT op-code");
                    puts("\tm = init memory with n");
                    puts("\tf = CPU frequenzy n in MHz");
                    puts("\tx = load and execute filename");
                    puts("\tb = load EPROM content starting at 0x0000");
		    puts("\tc = filename for CAS emulation");
		    puts("\tw = start in windowed mode");
		    puts("\ta = start address");
                    exit(1);
            }
        }
    }
    putchar('\n');
    puts("NDR-Klein-Computer Simulator V0.6");
    puts("=================================");
    puts("based on Z80 emulator 'z80pack'");
    puts("by Udo Munk");
    putchar('\n');
    fflush(stdout);

    wrk_ram = PC = STACK = ram;
    memset((char *) ram, m_flag, 32768);
    memset((char *) ram + 32768, m_flag, 32768);
    if (l_flag)
        load_core();
    int_on();
    init_io(windowed);
    mon();
    if (s_flag)
        save_core();
    exit_io();
    int_off();
    return(0);
}
