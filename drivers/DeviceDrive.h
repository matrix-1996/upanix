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
#ifndef _DEVICE_DRIVE_H_
#define _DEVICE_DRIVE_H_

#include <Global.h>
#include <FSStructures.h>
#include <Atomic.h>
#include <DiskCache.h>
#include <string.h>
#include <drive.h>

#define DeviceDrive_SUCCESS						0
#define DeviceDrive_ERR_UNKNOWN_DEVICE_TYPE		1
#define DeviceDrive_ERR_INVALID_DRIVE_NAME		2
#define	DeviceDrive_ERR_MOUNT					3
#define	DeviceDrive_ERR_UNMOUNT					4
#define	DeviceDrive_ERR_FORMAT					5
#define	DeviceDrive_ERR_PARTITION_UPDATE		6
#define	DeviceDrive_ERR_CURR_DRIVE_UMOUNT		7
#define	DeviceDrive_ERR_DUPLICATE				8
#define	DeviceDrive_ERR_NOTFOUND				9
#define DeviceDrive_FAILURE						10

#define GET_DRIVE_FOR_FS_OPS(DRIVE_ID, ERR) \
	DiskDrive* pDiskDrive = DiskDriveManager::Instance().GetByID(DRIVE_ID, true) ; \
	if(pDiskDrive == NULL) \
		return ERR ;

typedef enum
{
	FS_UNKNOWN = 0x0,
	UPANFS = 0x83
} FS_TYPE;

typedef enum
{
	DEV_FLOPPY = 0x0,
	DEV_CD,
	DEV_ATA_IDE = 0x80,
	DEV_ATAPI,
	DEV_SCSI_USB_DISK = 0x90
} DEVICE_TYPE;

typedef enum
{
	FROM_FILE = -3,
	ROOT_DRIVE = -2,
	CURRENT_DRIVE = -1,
	FD_DRIVE0 = 0,
	FD_DRIVE1,
	FD_DRIVE2,
	FD_DRIVE3,
	CD_DRIVE0,
	CD_DRIVE1,
	HDD_DRIVE0,
	HDD_DRIVE1,
	USD_DRIVE0,
	USD_DRIVE1,
	USD_DRIVE2,
	USD_DRIVE3,
	USD_DRIVE4,
	USD_DRIVE5
} DRIVE_NO;

class RawDiskDrive;

class DiskDrive
{
  private:
    DiskDrive(int id,
      const upan::string& driveName, 
      DEVICE_TYPE deviceType,
      DRIVE_NO driveNumber,
      unsigned uiLBAStartSector,
      unsigned uiSizeInSectors,
      unsigned uiSectorsPerTrack,
      unsigned uiTracksPerHead,
      unsigned uiNoOfHeads,
      void* device, 
      RawDiskDrive* rawDisk,
      unsigned uiMaxSectorsInFreePoolCache, 
      unsigned uiNoOfSectorsInTableCache,
      unsigned uiMountPointStart, 
      unsigned uiMountPointEnd);

  public:
    byte Mount();
    byte UnMount();
    byte Read(unsigned uiStartSector, unsigned uiNoOfSectors, byte* bDataBuffer);
    byte xRead(byte* bDataBuffer, unsigned uiSector, unsigned uiNoOfSectors);
    byte Write(unsigned uiStartSector, unsigned uiNoOfSectors, byte* bDataBuffer);
    byte xWrite(byte* bDataBuffer, unsigned uiSector, unsigned uiNoOfSectors);
    byte FlushDirtyCacheSectors(int iCount = -1);
    byte FlushTableCache(int iFlushSize);
    void ReleaseCache();
    unsigned GetRealSectorNumber(unsigned uiSectorID) const;
    bool IsFreePoolCacheEnabled() const { return _enableFreePoolCache; }
    bool IsTableCacheEnabled() const { return _enableTableCache; }
    byte LoadFreeSectors();
    unsigned GetFreeSector();

    const upan::string& DriveName() const { return _driveName; }
    DEVICE_TYPE DeviceType() const { return _deviceType; }
    FS_TYPE FSType() const { return _fsType; }
    DRIVE_NO DriveNumber() const { return _driveNumber; }
    unsigned LBAStartSector() const { return _uiLBAStartSector; }
    unsigned SizeInSectors() const { return _uiSizeInSectors; }
    unsigned SectorsPerTrack() const { return _uiSectorsPerTrack; }
    unsigned TracksPerHead() const { return _uiTracksPerHead; }
    unsigned NoOfHeads() const { return _uiNoOfHeads; }
    unsigned MaxSectorsInFreePoolCache() const { return _uiMaxSectorsInFreePoolCache; }
    unsigned NoOfSectorsInTableCache() const { return _uiNoOfSectorsInTableCache; }
    unsigned MountPointStart() const { return _uiMountPointStart; }
    unsigned MountPointEnd() const { return _uiMountPointEnd; }
    bool StopReleaseCacheTask() const { return _bStopReleaseCacheTask; }

    int Id() const { return _id; }
    bool Mounted() const { return _mounted; }
    RawDiskDrive* RawDisk() const { return _rawDisk; }
    void* Device() const { return _device; }
    DiskCache& Cache() { return _mCache; }

    void StopReleaseCacheTask(bool value) { _bStopReleaseCacheTask = value; }
    void FSType(FS_TYPE t) { _fsType = t; }
    void Mounted(bool mounted);

    // FileSystem Mount Info
    FileSystemMountInfo	FSMountInfo ;
    byte					bFSCacheFlag ;

  private:
    byte RawRead(unsigned uiStartSector, unsigned uiNoOfSectors, byte* bDataBuffer);
    byte RawWrite(unsigned uiStartSector, unsigned uiNoOfSectors, byte* bDataBuffer);
    bool FlushSector(unsigned uiSectorID, const byte* pBuffer);
    void StartReleaseCacheTask();
    byte ReadFSBootBlock();
    byte ReadRootDirectory();
    byte VerifyBootBlock();

    int          _id;
    upan::string _driveName;
    DEVICE_TYPE  _deviceType;
    DRIVE_NO     _driveNumber;
    unsigned     _uiLBAStartSector;
    unsigned     _uiSizeInSectors;
    unsigned     _uiSectorsPerTrack;
    unsigned     _uiTracksPerHead;
    unsigned     _uiNoOfHeads;
    bool		     _bEnableDiskCache;
	  void*			    _device;
    RawDiskDrive* _rawDisk;
    unsigned      _uiMaxSectorsInFreePoolCache;
    unsigned      _uiNoOfSectorsInTableCache;
    unsigned      _uiMountPointStart;
    unsigned      _uiMountPointEnd;

    FS_TYPE       _fsType;
    bool          _mounted;
    Mutex			    _driveMutex;
    DiskCache		  _mCache;
		bool          _bStopReleaseCacheTask;
    bool          _enableFreePoolCache;
    bool          _enableTableCache;

    friend class DiskDriveManager;
};

typedef enum
{
	ATA_HARD_DISK = 100,
	USB_SCSI_DISK,
	FLOPPY_DISK,
} RAW_DISK_TYPES ;

class PartitionTable;
class RawDiskDrive
{
  private:
    RawDiskDrive(const upan::string& name, RAW_DISK_TYPES type, void* device);
  public:
    void Read(unsigned uiStartSector, unsigned uiNoOfSectors, byte* pDataBuffer);
    void Write(unsigned uiStartSector, unsigned uiNoOfSectors, byte* pDataBuffer);
    void UpdateSystemIndicator(unsigned uiLBAStartSector, unsigned uiSystemIndicator);

    const upan::string& Name() const { return _name; }
    RAW_DISK_TYPES Type() const { return _type; }
    unsigned SectorSize() const { return _sectorSize; }
    unsigned SizeInSectors() const { return _sizeInSectors; }
    void* Device() { return _device; }
  private:
    upan::string _name;
    RAW_DISK_TYPES _type;
    unsigned _sectorSize;
    unsigned _sizeInSectors;
    void* _device;
    Mutex _diskMutex;

    friend class DiskDriveManager;
};

class DriveRemoveClause
{
	public:
		virtual bool operator()(const DiskDrive* pDiskDrive) const = 0 ;
} ;

class DiskDriveManager
{
  private:
    DiskDriveManager();

  public:
    static DiskDriveManager& Instance()
    {
      static DiskDriveManager instance;
      return instance;
    }

    void Create(const upan::string& driveName, 
      DEVICE_TYPE deviceType, DRIVE_NO driveNumber,
      unsigned uiLBAStartSector, unsigned uiSizeInSectors,
      unsigned uiSectorsPerTrack, unsigned uiTracksPerHead, unsigned uiNoOfHeads,
      void* device, RawDiskDrive* rawDisk,
      unsigned uiMaxSectorsInFreePoolCache, unsigned uiNoOfSectorsInTableCache,
      unsigned uiMountPointStart, unsigned uiMountPointEnd);
    void RemoveEntryByCondition(const DriveRemoveClause& removeClause);
    DiskDrive* GetByDriveName(const upan::string& szDriveName, bool bCheckMount);
    DiskDrive* GetByID(int iID, bool bCheckMount);
    void DisplayList();
    byte Change(const upan::string& szDriveName);
    byte GetList(DriveStat** pDriveList, int* iListSize);
    byte MountDrive(const upan::string& szDriveName);
    byte UnMountDrive(const upan::string& szDriveName);
    byte FormatDrive(const upan::string& szDriveName);
    byte GetCurrentDriveStat(DriveStat* pDriveStat);

    RawDiskDrive* CreateRawDisk(const upan::string& name, RAW_DISK_TYPES iType, void* pDevice);
    byte RemoveRawDiskEntry(const upan::string& name);
    RawDiskDrive* GetRawDiskByName(const upan::string& name);

    RESOURCE_KEYS GetResourceType(DEVICE_TYPE deviceType);
    RESOURCE_KEYS GetResourceType(RAW_DISK_TYPES diskType);

    const upan::list<DiskDrive*>& DiskDriveList() const { return _driveList; }
    const upan::list<RawDiskDrive*>& RawDiskDriveList() const { return _rawDiskList; }
  private:
    Mutex _driveListMutex;
    upan::list<DiskDrive*> _driveList;
    upan::list<RawDiskDrive*> _rawDiskList;
    int _idSequence;
};

#endif
