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
#ifndef _PROCESS_ALLOCATOR_H_
#define _PROCESS_ALLOCATOR_H_

#define ProcessAllocator_SUCCESS		0
#define ProcessAllocator_FAILURE		1

#include <DLLLoader.h>

byte ProcessAllocator_AllocateAddressSpace(const unsigned uiNoOfPagesForProcess, const unsigned uiNoOfPagesForPTE,
		unsigned* uiPDEAddress, unsigned* uiStartPDEForDLL, unsigned uiProcessBase) ;
void ProcessAllocator_DeAllocateAddressSpace(ProcessAddressSpace* processAddressSpace) ; 
byte ProcessAllocator_AllocateAddressSpaceForKernel(ProcessAddressSpace* processAddressSpace, unsigned* uiStackAddress) ;
void ProcessAllocator_DeAllocateAddressSpaceForKernel(ProcessAddressSpace* processAddressSpace) ;
byte ProcessAllocator_AllocatePagesForDLL(unsigned uiNoOfPagesForDLL, ProcessSharedObjectList* pProcessSharedObjectList) ;

#endif

