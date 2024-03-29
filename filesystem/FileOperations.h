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
#ifndef _FILE_OPERATIONS_H_
#define _FILE_OPERATIONS_H_

#define FileOperations_SUCCESS					0
#define FileOperations_ERR_NO_WRITE_PERM		1
#define FileOperations_ERR_NOT_EXISTS			2
#define FileOperations_ERR_NO_PERM				3
#define FileOperations_ERR_INVALID_FILE_ATTR	4
#define FileOperations_FAILURE					5

# include <Global.h>
# include <FSStructures.h>

#define ATTR_DIR_DEFAULT	0x01ED  //0000 0001 1110 1101 => 0000(Rsv) 000(Dir) 111(u:rwx) 101(g:r-x) 101(o:r-x)
#define ATTR_FILE_DEFAULT	0x03A4  //0000(Rsv) 001(File) 110(u:rw-) 100(g:r--) 100(o:r--)
#define ATTR_DELETED_DIR	0x1000
#define ATTR_TYPE_DIRECTORY	0x2000
#define ATTR_TYPE_FILE		0x4000

#define ATTR_READ	0x4
#define ATTR_WRITE	0x2
#define ATTR_EXE	0x1

#define S_OWNER(perm)		((perm & 0x7) << 6)
#define S_GROUP(perm)		((perm & 0x7) << 3)
#define S_OTHERS(perm)		(perm & 0x7)

#define G_OWNER(perm)		((perm >> 6) & 0x7)
#define G_GROUP(perm)		((perm >> 3) & 0x7)
#define G_OTHERS(perm)		(perm & 0x7)

#define FILE_PERM_MASK	0x1FF
#define FILE_TYPE_MASK	0xF000

#define HAS_READ_PERM(perm)		((perm & 0x7) & ATTR_READ)
#define HAS_WRITE_PERM(perm)	((perm & 0x7) & ATTR_WRITE)
#define HAS_EXE_PERM(perm)		((perm & 0x7) & ATTR_EXE)

#define FILE_PERM(attr)	(attr & FILE_PERM_MASK)
#define FILE_TYPE(attr) (attr & FILE_TYPE_MASK)

#define S_ISDIR(attr) (FILE_TYPE(attr) == ATTR_TYPE_DIRECTORY)

#define FILE_STDOUT "STDOUT"
#define FILE_STDIN  "STDIN"
#define FILE_STDERR "STDERR"

typedef enum
{
	DIR_ACCESS_TIME = 0x01,
	DIR_MODIFIED_TIME = 0x02
} TIME_TYPE ;

typedef enum
{
	USER_OWNER,
	USER_GROUP,
	USER_OTHERS
} USER_TYPE ;

byte FileOperations_Open(int* fd, const char* szFileName, const byte mode) ;
byte FileOperations_Close(int fd) ;
bool FileOperations_ReadLine(int fd, upan::string& line);
byte FileOperations_Read(int fd, char* buffer, int len, unsigned* pReadLen) ;
byte FileOperations_Write(int fd, const char* buffer, int len, int* pWriteLen) ;
byte FileOperations_Create(const char* szFilePath, unsigned short usFileType, unsigned short usMode) ;
byte FileOperations_Delete(const char* szFilePath) ;
byte FileOperations_Exists(const char* szFileName, unsigned short usFileType) ;
byte FileOperations_Seek(int fd, int iOffset, int seekType) ;
byte FileOperations_UpdateTime(const char* szFileName, int iDriveID, byte bTimeType) ;
byte FileOperations_GetOffset(int fd, unsigned* uiOffset) ;
byte FileOperations_GetDirEntry(const char* szFileName, FileSystem_DIR_Entry* pDirEntry) ;
byte FileOperations_GetStat(const char* szFileName, int iDriveID, FileSystem_FileStat* pFileStat) ;
byte FileOperations_GetFileOpenMode(int fd, byte* mode) ;
byte FileOperations_GetStatFD(int iFD, FileSystem_FileStat* pFileStat) ;
byte FileOperations_SyncPWD() ;
byte FileOperations_ChangeDir(const char* szFileName) ;
byte FileOperations_GetDirectoryContent(const char* szPathAddress, FileSystem_DIR_Entry** pDirList, int* iListSize) ;
byte FileOperations_FileAccess(const char* szFileName, int iDriveID, int mode) ;
byte FileOperations_Dup2(int oldFD, int newFD) ;
byte FileOperations_ReInitStdFd(int StdFD) ;
byte FileOperations_GetCWD(char* szPathBuf, int iBufSize) ;

#endif
