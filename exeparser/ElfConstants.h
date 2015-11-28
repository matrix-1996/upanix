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

#ifndef _ELF_CONST_H_
#define _ELF_CONST_H_

#include <MemManager.h>
#include <stdint.h>

#define GLOBAL_REL_ADDR(paddr, base_addr) (paddr + base_addr - GLOBAL_DATA_SEGMENT_BASE) 

#define UNKNOWN "UnKnown"

typedef unsigned Elf32_Addr ;
typedef unsigned Elf32_Off ;
typedef unsigned Elf32_Word ;
typedef unsigned short Elf32_Half ;

typedef int Elf32_Sword ;

#endif
