/*
 * Z80SIM  -  a	Z80-CPU	simulator
 *
 * Copyright (C) 1987-2007 by Udo Munk
 *
 * This modul of the simulator contains a simple terminal I/O
 * simulation as an example.
 *
 * History:
 * 28-SEP-87 Development on TARGON/35 with AT&T Unix System V.3
 * 11-JAN-89 Release 1.1
 * 08-FEB-89 Release 1.2
 * 13-MAR-89 Release 1.3
 * 09-FEB-90 Release 1.4  Ported to TARGON/31 M10/30
 * 20-DEC-90 Release 1.5  Ported to COHERENT 3.0
 * 10-JUN-92 Release 1.6  long casting problem solved with COHERENT 3.2
 *			  and some optimization
 * 25-JUN-92 Release 1.7  comments in english and ported to COHERENT 4.0
 * 02-OCT-06 Release 1.8  modified to compile on modern POSIX OS's
 * 18-NOV-06 Release 1.9  modified to work with CP/M sources
 * 08-DEC-06 Release 1.10 modified MMU for working with CP/NET
 * 17-DEC-06 Release 1.11 TCP/IP sockets for CP/NET
 * 25-DEC-06 Release 1.12 CPU speed option
 * 19-FEB-07 Release 1.13 various improvements
 */

/*
 *	Sample I/O-handler
 *
 *	Port 0 input:	reads the next byte from stdin
 *	Port 0 output:	writes the byte to stdout
 *
 *	All the other ports are connected to an I/O-trap handler,
 *	I/O to this ports stops the simulation with an I/O error.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "sim.h"
#include "simglb.h"

/*
 *	Forward declarations of the I/O functions
 *	for all port addresses.
 */
static BYTE io_trap_in(void);
static void io_trap_out(BYTE);
extern BYTE gdp64_p60_in();
extern void gdp64_p60_out(BYTE);
extern BYTE key_p68_in();
extern void key_p68_out(BYTE);
extern BYTE key_p69_in();
extern void key_p69_out(BYTE);
extern BYTE gdp64_p70_in();
extern void gdp64_p70_out(BYTE);
extern BYTE gdp64_p71_in();
extern void gdp64_p71_out(BYTE);
extern BYTE gdp64_p72_in();
extern void gdp64_p72_out(BYTE);
extern BYTE gdp64_p73_in();
extern void gdp64_p73_out(BYTE);
extern BYTE gdp64_p75_in();
extern void gdp64_p75_out(BYTE);
extern BYTE gdp64_p77_in();
extern void gdp64_p77_out(BYTE);
extern BYTE gdp64_p78_in();
extern void gdp64_p78_out(BYTE);
extern BYTE gdp64_p79_in();
extern void gdp64_p79_out(BYTE);
extern BYTE gdp64_p7A_in();
extern void gdp64_p7A_out(BYTE);
extern BYTE gdp64_p7B_in();
extern void gdp64_p7B_out(BYTE);
extern BYTE cas_pCA_in();
extern void cas_pCA_out(BYTE);
extern BYTE cas_pCB_in();
extern void cas_pCB_out(BYTE);

/*
 *	This two arrayw contains function pointers
 *	for every I/O port (0 - 255), to do the required I/O.
 *	The first entry is for input, the second for output.
 */
static BYTE (*in_port[256]) ();
static void (*out_port[256]) (BYTE);
/*
 *	This function is to initiate the I/O devices.
 *	It will be called from the CPU simulation before
 *	any operation with the Z80 is possible.
 *
 *	In this sample I/O simulation we initialize all
 *	unused port with an error trap handler, so that
 *	simulation stops at I/O on the unused ports.
 *
 *	See the I/O simulation of CP/M for a more complex
 *	example.
 */

FILE *f;

void init_io(bool windowed)
{
	register int i;

	for (i = 1; i <= 255; i++)
    {
		in_port[i]= io_trap_in;
        out_port[i]=io_trap_out;
    }
    // set I/O hooks for NKC
    in_port[0x60]=gdp64_p60_in;
    out_port[0x60]=gdp64_p60_out;
    in_port[0x68]=key_p68_in;
    out_port[0x68]=key_p68_out;
    in_port[0x69]=key_p69_in;
    out_port[0x69]=key_p69_out;
    in_port[0x70]=gdp64_p70_in;
    out_port[0x70]=gdp64_p70_out;
    in_port[0x71]=gdp64_p71_in;
    out_port[0x71]=gdp64_p71_out;
    in_port[0x72]=gdp64_p72_in;
    out_port[0x72]=gdp64_p72_out;
    in_port[0x73]=gdp64_p73_in;
    out_port[0x73]=gdp64_p73_out;
    in_port[0x75]=gdp64_p75_in;
    out_port[0x75]=gdp64_p75_out;
    in_port[0x77]=gdp64_p77_in;
    out_port[0x77]=gdp64_p77_out;
    in_port[0x78]=gdp64_p78_in;
    out_port[0x78]=gdp64_p78_out;
    in_port[0x79]=gdp64_p79_in;
    out_port[0x79]=gdp64_p79_out;
    in_port[0x7A]=gdp64_p7A_in;
    out_port[0x7A]=gdp64_p7A_out;
    in_port[0x7B]=gdp64_p7B_in;
    out_port[0x7B]=gdp64_p7B_out;
    in_port[0xCA]=cas_pCA_in;
    out_port[0xCA]=cas_pCA_out;
    in_port[0xCB]=cas_pCB_in;
    out_port[0xCB]=cas_pCB_out;
    // initialize GDP64 card
    initGDP64(windowed);
    //f=fopen("iolog.txt","w");
}

/*
 *	This function is to stop the I/O devices. It is
 *	called from the CPU simulation on exit.
 *
 *	Nothing to do here, see the I/O simulation
 *	of CP/M for a more complex example.
 */
void exit_io(void)
{
   SDL_Quit();
   //fclose(f);
}

/*
 *	This is the main handler for all IN op-codes,
 *	called by the simulator. It calls the input
 *	function for port adr.
 */
BYTE io_in(BYTE adr)
{
    //if (in_port[adr]=io_trap_in) fprintf(f,"IN [%2X]\n",adr);
	return((*in_port[adr]) ());
}

/*
 *	This is the main handler for all OUT op-codes,
 *	called by the simulator. It calls the output
 *	function for port adr.
 */
void io_out(BYTE adr, BYTE data)
{
    //fprintf(f,"OUT[%2X]: %2X\n",adr,data);
	(*out_port[adr])(data);
}

/*
 *	I/O trap funtion
 *	This function should be added into all unused
 *	entrys of the port array. It stops the emulation
 *	with an I/O error.
 */
static BYTE io_trap_in(void)
{
	if (i_flag) {
		cpu_error = IOTRAP;
		cpu_state = STOPPED;
	}
	return((BYTE) 0);
}

static void io_trap_out(BYTE b)
{
    if (i_flag) {
        cpu_error = IOTRAP;
        cpu_state = STOPPED;
    }
    return;
}

