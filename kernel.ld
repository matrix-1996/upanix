/*
 *	Upanix - An x86 based Operating System
 *  Copyright (C) 2011 'Prajwala Prabhakar' 'srinivasa_prajwal@yahoo.co.in'
 *                                                                          
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *                                                                          
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                          
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/
 */
/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader. */
	. = 1M;

	/* First put the multiboot header, as it is required to be put very early
	   early in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text*)
	}

	/* Read-only data. */
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata*)
	}

	/* Read-write data (initialized) */
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data*)
	}

	/* Read-write data (uninitialized) and stack */
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss*)
		*(.bootstrap_stack)
	}

	/* Just make crtstuff happy */
	.tm_clone_table BLOCK(4K) : ALIGN(4K)
	{
		*(.tm_clone_table)
	}
	.jcr BLOCK(4K) : ALIGN(4K)
	{
		*(.jcr)
	}
	.got.plt BLOCK(4K) : ALIGN(4K)
	{
		*(.got.plt)
	}

	.init BLOCK(4K) : ALIGN(4K) 
	{
		*(.init)
	}

	.fini BLOCK(4K) : ALIGN(4K)
	{
		*(.fini)
	}

	/* Global ctors and dtors */
	.ctors BLOCK(4K) : ALIGN(4K)
	{
		*(.ctors*)
	}
	
	.dtors BLOCK(4K) : ALIGN(4K)
	{
		*(.dtors*)
	}

	.eh_frame_hdr BLOCK(4K) : ALIGN(4K)
	{
		*(.eh_frame_hdr*)
	}
	
	.eh_frame BLOCK(4K) : ALIGN(4K)
	{
		*(.eh_frame*)
	}
	
	.gcc_except_table BLOCK(4K) : ALIGN(4K)
	{
		*(.gcc_except_table*)
	}

	/* The compiler may produce other sections, by default it will put them in
	   a segment with the same name. Simply add stuff here as needed. */
}
