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
 * Emulates a GDP64 graphics card with a Thomson EF9366 graphics processor
 * it uses the SDL library for graphical output
 * since handling of  key events is also done by SDL lib, KEY functions
 * reside in this file too
 */
#include <SDL/SDL.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "sim.h"
#include "simglb.h"
#include "ef9366charset.h"
#include "nkcglobal.h"

extern void resetVsyncTimer();

/* global variables GDP64 */
BYTE status=0x04;            /* status of the gdp, here initialized with b00000100 */
BYTE ctrl1=0x00;              /* CTRL1 register */
BYTE ctrl2=0x00;             /* CTRL2 register */
int penX=0;                  /* Pen X and Y position */
int penY=0;
BYTE csize=0x11;             /* CSIZE register */
BYTE deltax=0x00;            /* DELTAX register */
BYTE deltay=0x00;            /* DELTAY register */
BYTE seite=0;                /* PAGE register */
WORD lineStyle[4]={0xFFFF,   /* Line styles of EF9366 in hexadecimal representation, first a continous line */
                   0xCCCC,   /* Dotted (2 dots on, 2 dots off) */
                   0xF0F0,   /* Dashed (4 dots on, 4 dots off) */
                   0xFFCC};  /* Dotted-dashed (10 dots on, 2 dots off, 2 dots on, 2 dots off) */


/* global variables SDL */
SDL_Surface *screen,*pages[4];
SDL_Color bg={0x00,0x00,0x00,0xFF}; Uint32 bg32=0x000000FF;
SDL_Color fg={0x10,0xA4,0x13,0xFF}; Uint32 fg32=0x10A413FF;
int screenModeChanged=0;     /* 1 if system just finished switching from fs to window or vice versa */
int actualWritePage=0;       /* on which page do we write at the moment? */
int actualReadPage=0;        /* which page is shown at the moment? */
int contentChanged=0;        /* something new written? */

/* KEY specific stuff */
BYTE keyReg68=0x80;           /* initialize with a value which has bit 7 set */
BYTE keyReg69=0xE7;           /* DIL settings, here standard is used */
char keyBuf[100];             /* remember 100 chars maximum */
int bufCount=0;               /* how many chars are unread */
int readPos=0;                /* position for next char to read? */
int writePos=0;               /* position where next key will be stored */
SDL_GrabMode grabMode=SDL_GRAB_OFF; /* initial grab mode */

/*
  Begin of all SDL related functions
*/
void DrawPixel(SDL_Surface *sc, int x, int y,Uint8 R, Uint8 G,Uint8 B)
{
    Uint32 color = SDL_MapRGB(sc->format, R, G, B);
    
    switch (sc->format->BytesPerPixel)
    {
        case 1:
        { /* vermutlich 8 Bit */
            Uint8 *bufp;

            bufp = (Uint8 *)sc->pixels + y*sc->pitch + x;
            *bufp = color;
        }
        break;

        case 2:
        { /* vermutlich 15 Bit oder 16 Bit */
            Uint16 *bufp;

            bufp = (Uint16 *)sc->pixels + y*sc->pitch/2 + x;
            *bufp = color;
        }
        break;

        case 3:
        { /* langsamer 24-Bit-Modus, selten verwendet */
            Uint8 *bufp;

            bufp = (Uint8 *)sc->pixels + y*sc->pitch + x * 3;
            if(SDL_BYTEORDER == SDL_LIL_ENDIAN)
            {
                bufp[0] = color;
                bufp[1] = color >> 8;
                bufp[2] = color >> 16;
            }
            else
            {
                bufp[2] = color;
                bufp[1] = color >> 8;
                bufp[0] = color >> 16;
            }
        }
        break;

        case 4:
        { /* vermutlich 32 Bit */
            Uint32 *bufp;

            bufp = (Uint32 *)sc->pixels + y*sc->pitch/4 + x;
            *bufp = color;
        }
        break;
    }
    contentChanged=1;
}

void DrawChar(unsigned char c)
{
    /* draws a char at the current position of the pen */
    int realX=penX;                             /* x coordinate is running exactly as it's on the PC */
    int realY=255-penY;                         /* PC has y-coordinate 0 in the upper left corner, NKC has it in the lower left */
    unsigned char  c_off=c-' ';                 /* calulate down to array base */
    int x,y;                                    /* loop variables */
    int xMag=0;
    int yMag=0;
    
    if (realX<0 || realX>512) return;           /* Pen is outside of the screen */
    if (realY<0 || realY>256) return;           /* same in y direction */
    if (c_off>96) return;            /* wasn't an ASCII character or block */
    if (ctrl1 & 1)                               /* is Pen down? else just calculate new coordinates */
    {
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            if ( SDL_LockSurface(pages[actualWritePage]) < 0 )
            {
                return;
            }
        }
        /* is pen in writing or deleting mode? */
        SDL_Color pen;
        if (ctrl1&2)
            pen=fg;
        else
            pen=bg;
        /* calculate sizing in X and Y directions */
        xMag=(csize & 0xF0)>> 4;
        if (xMag==0) xMag=16;
        yMag=(csize & 0x0F) ;
        if (yMag==0) yMag=16;
        for ( x=0; x<5; x++)
        {
            for (y=0; y<8; y++)
            {
                if ((charset[c_off][x] & 128>>y)!=0)
                {
                    int x1=0;
                    int y1=0;
                    // draw pixel block, depending on magnification
                    for ( x1=0; x1<xMag; x1++)
                    {
                        for ( y1=0; y1<yMag; y1++)
                        {
                            if (ctrl2&8)
                                DrawPixel(pages[actualWritePage],realX-y*yMag-y1,realY-x*xMag-x1,pen.r,pen.g,pen.b);
                            else
                                DrawPixel(pages[actualWritePage],realX+x*xMag+x1,realY-y*yMag-y1,pen.r,pen.g,pen.b);
                        }
                    }
                }
                if (ctrl2&4)
                {
                    if (ctrl2&8)
                        realY--;
                    else
                        realX++;
                }
            }
            if (ctrl2&4)
            {
                if (ctrl2&8)
                    realY+=8;
                else
                    realX-=8;
            }
        }
        
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            SDL_UnlockSurface(pages[actualWritePage]);
        }
    }
    // Now correct penX and penY
    if (ctrl2&8)
        penY+=6*xMag;
    else
        penX+=6*xMag;        // char width + 1
}

/*
 Draws a horizontal Line from x1,y to x2,y
 */
void DrawHLine(int x1,int y,int x2)
{
    /* which line style do we use? */
    WORD style=lineStyle[(ctrl2 & 3)];
    penX=x2;        /* adjust X coordinate even if pen is up */
    /* should wie exchange x values? */
    if (x2<x1)
    {
        int h=x1; x1=x2; x2=h;      /* triangular exchange */
    }
    WORD bit=0x8000;                 /* bit mask for line style */
    if (ctrl1&1)                     /* is pen up? */
    {
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            if ( SDL_LockSurface(pages[actualWritePage]) < 0 ) {
                return;
            }
        }
        /* is pen in writing or deleting mode? */
        SDL_Color pen;
        SDL_Color inv;
        if (ctrl1&2)
        {
            pen=fg;
            inv=bg;
        }
        else
        {
            pen=bg;
            inv=fg;
        } 
        for (x1; x1<=x2; x1++)
        {
            if (style & bit)
                DrawPixel(pages[actualWritePage],x1,y,pen.r,pen.g,pen.b);
            else
                DrawPixel(pages[actualWritePage],x1,y,inv.r,inv.g,inv.b);
            bit=(bit>>1);
            if (bit==0) bit=0x8000;
        }
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            SDL_UnlockSurface(pages[actualWritePage]);
        }
    }
}

/*
 Draws a vertical Line from x,y1 to x,y2
 */
void DrawVLine(int x,int y1,int y2)
{
    /* which line style do we use? */
    WORD style=lineStyle[(ctrl2 & 3)];
    penY=255-y2;        /* adjust Y coordinate even if pen is up */
    /* should wie exchange x values? */
    if (y2<y1)
    {
        int h=y1; y1=y2; y2=h;      /* triangular exchange */
    }
    WORD bit=0x8000;                 /* bit mask for line style */
    if (ctrl1&1)                     /* is pen up? */
    {
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            if ( SDL_LockSurface(pages[actualWritePage]) < 0 ) {
                return;
            }
        }
        /* is pen in writing or deleting mode? */
        SDL_Color pen;
        SDL_Color inv;
        if (ctrl1&2)
        {
            pen=fg;
            inv=bg;
        }
        else
        {
            pen=bg;
            inv=fg;
        } 
        for (y1; y1<=y2; y1++)
        {
            if (style & bit)
                DrawPixel(pages[actualWritePage],x,y1,pen.r,pen.g,pen.b);
            else
                DrawPixel(pages[actualWritePage],x,y1,inv.r,inv.g,inv.b);
            bit=(bit>>1);
            if (bit==0) bit=0x8000;
        }
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            SDL_UnlockSurface(pages[actualWritePage]);
        }
    }
}

/*
 Draws a Line from x1,y1 to x2,y2 using the bresenham algorithm
 adopted from an implementation of 
 */
void DrawLine(int x1,int y1,int x2,int y2)
{
    /* which line style do we use? */
    WORD style=lineStyle[(ctrl2 & 3)];
    /* adjust coordinates, even if pen is up */
    //penY=255-y2;
    //penX=x2;
    
    /* should wie exchange x values? */
    if (x2<x1)
    {
        int h=y1; y1=y2; y2=h;      /* triangular exchange */
        h=x1; x1=x2; x2=h;
    }
    WORD bit=0x8000;                 /* bit mask for line style */
    if (ctrl1&1)                     /* is pen up? */
    {
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            if ( SDL_LockSurface(pages[actualWritePage]) < 0 )
            {
                return;
            }
        }
        /* is pen in writing or deleting mode? */
        SDL_Color pen;
        SDL_Color inv;
        if (ctrl1&2)
        {
            pen=fg;
            inv=bg;
        }
        else
        {
            pen=bg;
            inv=fg;
        }
        /* now start bresenham */
        int dx=abs(x2-x1);
        int dy=abs(y2-y1);
        int inc_dec=((y2>=y1)?1:-1);

        if(dx>dy)
        {
            int two_dy=(2*dy);
            int two_dy_dx=(2*(dy-dx));
            int p=((2*dy)-dx);

            int x=x1;
            int y=y1;

            if (style & bit)
                DrawPixel(pages[actualWritePage],x,y,pen.r,pen.g,pen.b);
            else
                DrawPixel(pages[actualWritePage],x,y,inv.r,inv.g,inv.b);
            bit=(bit>>1);
            if (bit==0) bit=0x8000;

            while(x<x2)
            {
                x++;

                if(p<0)
                    p+=two_dy;

                else
                {
                    y+=inc_dec;
                    p+=two_dy_dx;
                }

                if (style & bit)
                    DrawPixel(pages[actualWritePage],x,y,pen.r,pen.g,pen.b);
                else
                    DrawPixel(pages[actualWritePage],x,y,inv.r,inv.g,inv.b);
                bit=(bit>>1);
                if (bit==0) bit=0x8000;
            }
        }
        else
        {
            int two_dx=(2*dx);
            int two_dx_dy=(2*(dx-dy));
            int p=((2*dx)-dy);

            int x=x1;
            int y=y1;

            if (style & bit)
            DrawPixel(pages[actualWritePage],x,y,pen.r,pen.g,pen.b);
            else
            DrawPixel(pages[actualWritePage],x,y,inv.r,inv.g,inv.b);
            bit=(bit>>1);
            if (bit==0) bit=0x8000;

            while(y!=y2)
            {
                y+=inc_dec;

                if(p<0)
                    p+=two_dx;

                else
                {
                    x++;
                    p+=two_dx_dy;
                }

                if (style & bit)
                    DrawPixel(pages[actualWritePage],x,y,pen.r,pen.g,pen.b);
                else
                    DrawPixel(pages[actualWritePage],x,y,inv.r,inv.g,inv.b);
                bit=(bit>>1);
                if (bit==0) bit=0x8000;
            }
        }
        if ( SDL_MUSTLOCK(pages[actualWritePage]) )
        {
            SDL_UnlockSurface(pages[actualWritePage]);
        }
    }
}



void clearScreen()
{
    /* fills the SDL surface with the background color */
    SDL_FillRect(pages[actualWritePage], NULL, SDL_MapRGB(pages[actualWritePage]->format, bg.r, bg.g ,bg.b));
}

/*
 Begin of I/O functions for port 0x60 and 0x70-0x7F
*/

BYTE gdp64_p60_in()
{
    return seite;
}

void gdp64_p60_out(BYTE b)
{
    actualReadPage=(b & 0x30) >> 4;
    actualWritePage=(b & 0xC0) >> 6;
    if (seite!=b)
    {
        contentChanged=1;
    }
    seite=b;
}

BYTE gdp64_p70_in()
{
    /* read status register of EF9366 
     * meanings of the bits:
     * bit 0: status of lightpen sequence 1 means that a sequence just ended
     * bit 1: vertical sync signal, 1 means it's vertical sync time
     * bit 2: ready status, 0 means no command my be given, 1 means ready
     * bit 3: position of the writing pen, 1 means out of the screen borders
     * bit 4: lightpen sequence IRQ
     * bit 5: vertical sync IRQ
     * bit 6: ready sigbal IRQ
     * bit 7: ORed bits 4-7
    */
    // TODO: implement timers for 20ms vsync signal!
    return status;
}

void gdp64_p70_out(BYTE b)
{
    Uint32 pen;
    status=(status & 0xFB);
    /* accept commands for the EF9366 and call the SDL implementations for them */
    if (b>=0x20 && b<=0x7F)                         /* was an ASCII character */
    {
        DrawChar(b);
        status=(status | 4);
        return;
    }
    /* short vector command? */
    if (b>=128)
    {
        signed char dirMul[][2]={{1,0},{1,1},{0,1},{-1,1},{0,-1},{1,-1},{-1,0},{-1,-1}};
        BYTE dx=(b & 0x60) >>5;
        BYTE dy=((b & 0x18) >>3);
        BYTE dir=(b & 7);
        int i;
        if (ctrl1&2)
            pen=fg32;
        else
            pen=bg32;
	
        DrawLine(penX, 255-penY, penX+dx*dirMul[dir][0], 255-(penY+dy*dirMul[dir][1]));
        contentChanged=1;
        penX=penX+dx*dirMul[dir][0];
        penY=penY+dy*dirMul[dir][1];
        status=status | 4;
        return;
    }
    /* must be a GDP command */
    switch(b)
    {
        case 0: /* pen selection */
                ctrl1=(ctrl1 | 2);
                break;

        case 1: /* eraser selection */
                ctrl1=(ctrl1 & 0xFD);
                break;

        case 2: /* pen down */
                ctrl1=(ctrl1 | 1);
                break;

        case 3: /* pen up */
                ctrl1=(ctrl1 & 0xFE);
                break;
                
        case 4: /* clear screen */
                clearScreen();
                break;

        case 5: /* set X and Y registers to 0 */
                penX=0;
                penY=0;
                break;
                
        case 6: /* clear screen and reset coordinates */
                clearScreen();
                penX=0;
                penY=0;
                break;
        
        case 7: /* clear screen, set CSIZE to 1, other registers to 0 */
                clearScreen();
                penX=0;
                penY=0;
                csize=0x11;
                ctrl1=0;
                status=4;
                break;

        case 10: /* block drawing 5x8 */
                DrawChar(128);
                break;

        case 13: /* set X to 0 */
                penX=0;
                break;

        case 14: /* set Y to 0 */
                penY=0;
                break;
                
        case 16: /* draw horizontal line in positive x direction */
                DrawHLine(penX, 255-penY,penX+deltax);
                contentChanged=1;
                break;

        case 17: /* draw line in positive x and y direction */
                if (ctrl1&2)
                    pen=fg32;
                else
                    pen=bg32;
            
                DrawLine(penX, 255-penY, penX+deltax, 255-(penY+deltay));
                contentChanged=1;
                penX=penX+deltax;
                penY=penY+deltay;
                break;
                
        case 18: /* draw vertical line in positive y direction */
                DrawVLine(penX,255-penY,255-(penY+deltay));
                contentChanged=1;
                break;

        case 19: /* draw line in negative x and positive y direction */
                if (ctrl1&2)
                    pen=fg32;
                else
                    pen=bg32;
            
                DrawLine(penX, 255-penY, penX-deltax, 255-(penY+deltay));
                contentChanged=1;
                penX=penX-deltax;
                penY=penY+deltay;
                break;
                
        case 20: /* draw vertical line in negative y direction */
                DrawVLine(penX, 255-penY, 255-(penY-deltay));
                contentChanged=1;
                break;

        case 21: /* draw line in positive x and negative y direction */
                if (ctrl1&2)
                    pen=fg32;
                else
                    pen=bg32;
            
                DrawLine(penX, 255-penY, penX+deltax, 255-(penY-deltay));
                contentChanged=1;
                penX=penX+deltax;
                penY=penY-deltay;
                break;
                
        case 22: /* draw horizontal line in negative x direction */
                DrawHLine(penX,255-penY, penX-deltax);
                contentChanged=1;
                break;

        case 23: /* draw line in negative x and positive y direction */
                if (ctrl1&2)
                    pen=fg32;
                else
                    pen=bg32;
            
                DrawLine(penX, 255-penY, penX-deltax, 255-(penY-deltay));
                contentChanged=1;
                penX=penX-deltax;
                penY=penY-deltay;
                break;
                
        default: /* unimplemented command, do nothing and return */
                break;
    }
    status=(status | 4);
    return;
}

BYTE gdp64_p71_in()
{
    /* read CTRL1 register of EF9366
     * meanings of the bits:
     * bit 0: pen position, 1=down, 0=up
     * bit 1: pen mode, 1=writing, 0=deleting
     * bit 2: writing on the screen, BLK is always 1, no video signal output
     * bit 3: screen window closed, writing position cannot leave screen
     * bit 4: enable IRQs for lightpens
     * bit 5: enable IRQs for vertical sync
     * bit 6: enables IRQs for GDP64 ready signal
     * bit 7: n/a
    */
    return ctrl1;
}

void gdp64_p71_out(BYTE b)
{
    /* accept values for CTRL1 register of the EF9366 */
    ctrl1=b;
    return;
}

BYTE gdp64_p72_in()
{
    /* read CTRL2 register of EF9366
     * meanings of the bits:
     * bit 0: type of vectors, LSB
     * bit 1: type of vectors, MSB
     * bit 2: 1=tilted character
     * bit 3: 1=character on vertical axis
     * bit 4: n/a
     * bit 5: n/a
     * bit 6: n/a
     * bit 7: n/a
    */
    return ctrl2;
}

void gdp64_p72_out(BYTE b)
{
    /* accept values for CTRL2 register of the EF9366 */
    ctrl2=b;
    return;
}


BYTE gdp64_p73_in()
{
    /* read CSIZE register of EF9366 */
    return csize;
}

void gdp64_p73_out(BYTE b)
{
    /* set CSIZE register of EF9366 */
    csize=b;
}

BYTE gdp64_p75_in()
{
    /* read DELTAX register of EF9366 */
    return deltax;
}

void gdp64_p75_out(BYTE b)
{
    /* set DELTAX register of EF9366 */
    deltax=b;
}

BYTE gdp64_p77_in()
{
    /* read DELTAY register of EF9366 */
    return deltay;
}

void gdp64_p77_out(BYTE b)
{
    /* set DELTAY register of EF9366 */
    deltay=b;
}
BYTE gdp64_p78_in()
{
    /* read X MSB register of EF9366 */
    return (BYTE)((penX & 0xFF00)>>8);    /* most significant 8 bits */
}

void gdp64_p78_out(BYTE b)
{
    /* set X MSB register of EF9366 */
    penX=(((int)b)<<8) | (penX & 0xFF);
}

BYTE gdp64_p79_in()
{
    /* read X LSB register of EF9366 */
    return (BYTE)(penX & 0xFF);
}

void gdp64_p79_out(BYTE b)
{
    /* set X LSB register of EF9366 */
    penX=((int)b) | (penX & 0xFF00);
}

BYTE gdp64_p7A_in()
{
    /* read Y MSB register of EF9366 */
    return (BYTE)((penY & 0xFF00)>>8);    /* most significant 8 bits */
}

void gdp64_p7A_out(BYTE b)
{
    /* set Y MSB register of EF9366 */
    penY=(((int)b)<<8) | (penY & 0xFF);
    
}

BYTE gdp64_p7B_in()
{
    /* read Y LSB register of EF9366 */
    return (BYTE)(penY & 0xFF);
}

void gdp64_p7B_out(BYTE b)
{
    /* set Y LSB register of EF9366 */
    penY=((int)b) | (penY & 0xFF00);
}

int gdp64_set_vsync(BYTE vs)
{
    if (vs!=0)
    {
        status=(status | 2);
        /* now blit actual readed page if something has changed */
        if (contentChanged==1)
        {
            SDL_BlitSurface(pages[actualReadPage],NULL,screen,NULL);
            SDL_UpdateRect(screen,0,0,0,0);
            contentChanged=0;
        }
    }
    else
    {
        status=(status & 0xFD);
    }
    if (screenModeChanged==1)
    {
        screenModeChanged=0;
        return 1;
    }
    return 0;
}

/*
* KEY functions
*/
BYTE key_p68_in()
{
    SDL_Event event;
    
    /* first read Key if available */

    if (SDL_PollEvent(&event))
    {
        if (event.type==SDL_KEYDOWN)
        {
            /* first evaluate simulation keys */
            if (event.key.keysym.sym==SDLK_F1)
            {
                SDL_WM_ToggleFullScreen(screen);
                screenModeChanged=1;
            }
            if (event.key.keysym.sym==SDLK_F12)
            {
                *--STACK = (PC - ram);
                PC = ram;
            }
            if (event.key.keysym.sym==SDLK_F10)
	    {
		if (grabMode==SDL_GRAB_ON) {
		  grabMode=SDL_GRAB_OFF;
		} else {
		  grabMode=SDL_GRAB_ON;
		}
		SDL_WM_GrabInput(grabMode);
	    }
	    if (event.key.keysym.sym==SDLK_F2 && CAS_FILE!=0)
	    {
		lseek(CAS_FILE, 0,SEEK_SET);
	    }
            if (event.key.keysym.unicode==0)
                keyReg68=0x80;
            else
                keyReg68=event.key.keysym.unicode;
            
        }
        else
        {
            keyReg68=0x80;
        }
    }
    return keyReg68;
}

void key_p68_out(BYTE b)
{
    return;                 /* no output on key ports */
}

BYTE key_p69_in()
{
    keyReg68=0x80;
    return keyReg69;
}

void key_p69_out(BYTE b)
{
    return;                 /* no output on key ports */
}
/*
 * initialize GDP64 graphics card emulation routines
*/

void initGDP64(bool windowed)
{
    int i;
    /* Initialize Graphics Window */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTTHREAD) == -1)
    {
        fprintf(stderr,"Can't init SDL:  %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    screen = SDL_SetVideoMode(512, 256, 32, SDL_SWSURFACE);
    if (screen == NULL)
    {
        fprintf(stderr,"Can't set video mode: %s\n", SDL_GetError());
        exit(1);
    }

    /* create the four pages of the EF9366 */
    for (i=0; i<4; i++)
    {
        pages[i]=SDL_CreateRGBSurface(SDL_SWSURFACE,512,256,32,0,0,0,0);
        if (pages[i]==NULL)
        {
            fprintf(stderr,"Cant't create page %d for EF9366. SDL-Error:%s\n",SDL_GetError());
            exit(2);
        }
    }
    if (!windowed) {
      SDL_WM_ToggleFullScreen(screen);
      screenModeChanged=1;
    }
    /* set window title */
    SDL_WM_SetCaption("GDP64 graphics output for NKC", NULL);
    /* Disable Mouse-Cursor */
    SDL_WM_GrabInput(grabMode);
    SDL_ShowCursor(SDL_DISABLE);
    /* Enable UNICODE string input for keyboard */
    SDL_EnableUNICODE(1);
}
