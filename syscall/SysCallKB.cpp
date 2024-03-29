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
# include <SysCall.h>
# include <SysCallKB.h>

byte SysCallKB_IsPresent(unsigned uiSysCallID)
{
	return (uiSysCallID > SYS_CALL_KB_START && uiSysCallID < SYS_CALL_KB_END) ;
}

void SysCallKB_Handle(
__volatile__ int* piRetVal,
__volatile__ unsigned uiSysCallID, 
__volatile__ bool bDoAddrTranslation,
__volatile__ unsigned uiP1, 
__volatile__ unsigned uiP2, 
__volatile__ unsigned uiP3, 
__volatile__ unsigned uiP4, 
__volatile__ unsigned uiP5, 
__volatile__ unsigned uiP6, 
__volatile__ unsigned uiP7, 
__volatile__ unsigned uiP8, 
__volatile__ unsigned uiP9)
{
	switch(uiSysCallID)
	{
		case SYS_CALL_KB_READ : // Wait for Key In
			// P1 => Address of Variable where Keyed In Value is Stored
			{
				int* ch = KERNEL_ADDR(bDoAddrTranslation, int*, uiP1) ;
				*ch = Keyboard_GetKeyInBlockMode() ;
			}
			break ;
	}
}
