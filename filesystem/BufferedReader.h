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
#ifndef _BUFFERED_READER_H_
#define _BUFFERED_READER_H_

# include <Global.h>

class BufferedReader
{
	private:

		unsigned m_uiOffSet ;
		unsigned m_uiBufferSize ;

		char* m_szBuffer ;

		bool m_bObjectState ;

	public:
		int m_iFD ;
		BufferedReader(const char* szFileName, unsigned uiOffSet, unsigned uiBufferSize) ;
		~BufferedReader() ;

		inline bool GetState() { return m_bObjectState ; }

		bool Seek(unsigned uiOffSet) ;
		bool Read(char* szBuffer, int iLen, unsigned* pReadLen) ;
} ;

#endif
