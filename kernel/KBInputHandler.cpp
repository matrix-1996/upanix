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
# include <KBInputHandler.h>
# include <Keyboard.h>
# include <SessionManager.h>

/******************* Static Functions ***************************/

byte KBInputHandler_InRegList(byte key)
{
	switch(key)
	{
		case Keyboard_F1:
		case Keyboard_F2:
		case Keyboard_F3:
		case Keyboard_F4:
		case Keyboard_F5:
		case Keyboard_F6:
		case Keyboard_F7:
		case Keyboard_F8:
			return true ;
	}

	return false ;
}

/***************************************************************/

byte KBInputHandler_Process(byte key) 
{
	byte bKeyRelease = false ;
	
	if(key & 0x80)
	{
		key -= 0x80 ;
		bKeyRelease = true ;
	}

	key = Keyboard_MapKey(key) ;

	if(KBInputHandler_InRegList(key))
	{
		if(bKeyRelease)
			return true ;

		SessionManager_SwitchToSession(SessionManager_KeyToSessionIDMap(key)) ;
		return true ;
	}

	return false ;
}

