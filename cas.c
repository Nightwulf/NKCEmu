/***************************************************************************
 *   Copyright (C) 2007 by Torsten Evers                                   *
 *   tevers@onlinehome.de                                                  *
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
  * Emulates a CAS interface with a Motorla 6850 as the serial converter.
  * Files are written to the home directory of the user or the configured directory.
 */
#include <stdio.h>
#include <unistd.h>
#include "sim.h"
#include "simglb.h"
#include "nkcglobal.h"

BYTE transmitter=0x02;
BYTE receiver=0x01;

BYTE cas_pCA_in() 
{
  return transmitter | receiver;
}


void cas_pCA_out(BYTE data)
{

}

BYTE cas_pCB_in() 
{
  BYTE byte=0xff;
  if (CAS_FILE!=0) 
  {
      read(CAS_FILE,&byte,1);
  }
  return byte;
}

void cas_pCB_out(BYTE data)
{
    if (CAS_FILE!=0) 
    {
      write(CAS_FILE, &data,1);  
    }
}
