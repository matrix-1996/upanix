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

#include <Floppy.h>
#include <Display.h>
#include <IDT.h>
#include <AsmUtil.h>
#include <PIC.h>
#include <DMA.h>
#include <MemUtil.h>
#include <PortCom.h>
#include <ProcessManager.h>
#include <DeviceDrive.h>
#include <StringUtil.h>
#include <DMM.h>
#include <KernelUtil.h>

#define	SRA_STATUS_REGA				0x3F0
#define	SRB_STATUS_REGB				0x3F1
#define	DOR_DIGITAL_OUTPUT_REG		0x3F2
#define	TDR_TAPE_DRIVE_REG			0x3F3
#define MSR_MAIN_STATUS_REG			0x3F4
#define DSR_DATA_RATE_SELECT_REG	0x3F4
#define DFR_DATA_FIFO_REG			0x3F5
#define DIR_DIGITAL_INPUT_REG		0x3F7
#define CCR_CONFIG_CONTROL_REG		0x3F7

#define DOR_DMA_GATE				0x08
#define DOR_RESET					0x04
#define FDD_VERSION_ENHANCED		0x90

#define FDD_VERSION_CMD				0x10
#define FDD_RECALIBRATE_CMD			0x07
#define FDD_SENSE_INT_STATUS_CMD	0x08
#define FDD_SENSE_DRIVE_STATUS_CMD	0x04
#define FDD_SEEK_CMD				0x0F
#define FDD_SPECIFY_CMD				0x03
#define FDD_READ_CMD				0xE6 // Test 0xE6 --> MT --> Multi Track
#define FDD_WRITE_CMD				0xC5
#define FDD_FORMAT_TRACK_CMD		0x4D

#define MSR_RQM						0x80
#define MSR_DIO						0x40
#define MSR_NON_DMA					0x20
#define MSR_CMD_BUSY				0x10
#define MSR_DRV3_BUSY				0x08
#define MSR_DRV2_BUSY				0x04
#define MSR_DRV1_BUSY				0x02
#define MSR_DRV0_BUSY				0x01

#define GAP		0x1B
#define RATE	0x00
#define SRTHUT	0xCF	/* StepRate - HeadUnload Times */
#define HLT_ND	0x10	/* StepRate - HeadUnload Times + DMA Flag*/

#define MAX_DRIVES 4

static byte Floppy_ReplyBuffer[MAX_RESULT_PHASE_REPLIES] ;
static byte Floppy_bMotorOn[MAX_DRIVES] ;
static byte Floppy_bRequestMotorOff[MAX_DRIVES] ;

const byte DOR_MOTOR_NO[] = { 0x10, 0x20, 0x40, 0x80 } ;
static bool Floppy_bInitStatus = false ;

typedef struct FloppyFormat
{
	byte bTrack ;
	byte bHead ;
	byte bSector ;
	byte bSectorSize ;
} PACKED Floppy_FormatFields ;

/****************************** Static Functions **************************************/

static  void Floppy_SetDataRate(byte bDataRate) ;
static  void Floppy_EnableMotorAndDrive(DRIVE_NO driveNo) ;
static  void Floppy_DisableMotorAndDrive(DRIVE_NO driveNo) ;
static  void Floppy_Reset() ;
static  byte Floppy_InitDMAController(DMA_MODE DMAMode, unsigned uiWordCount) ;

static byte Floppy_CompleteResultPhase1(byte bNoOfResultReplies) ;
static byte Floppy_SendControlCommand(byte bCmd) ;
static byte Floppy_SendSpecifyCommand() ;
static byte Floppy_CompleteResultPhase(byte bNoOfResultReplies) ;
static byte Floppy_SenseInterruptStatus() ;
static byte Floppy_WaitForInterrupt() ;
static byte Floppy_ReCaliberate(DRIVE_NO driveNo) ;
static byte Floppy_Seek(DRIVE_NO driveNo, Floppy_HEAD_NO headNo, unsigned uiSeekTrack) ;
static byte Floppy_FormatTrack(const DiskDrive* pDiskDrive, const Floppy_HEAD_NO headNo) ;
static byte Floppy_ReadWrite(const DiskDrive* pDiskDrive, unsigned uiStartSectorNo, unsigned uiEndSectorNo, Floppy_MODE mode) ;
static byte Floppy_FullFormat(const DiskDrive* pDiskDrive) ;

static int Floppy_MotorController()
{
	unsigned i ;
	for(i = 0; i < MAX_DRIVES; i++)
	{
		if(Floppy_bRequestMotorOff[i] == true)
		{
			Floppy_bMotorOn[i] = false ;
			Floppy_bRequestMotorOff[i] = false ;
			Floppy_DisableMotorAndDrive((DRIVE_NO)i) ;
		}
	}
	return 1 ;
}

static void Floppy_StartMotor(DRIVE_NO driveNo)
{
	Floppy_bRequestMotorOff[driveNo] = false ;
	if(Floppy_bMotorOn[driveNo] == true)
		return ;

	Floppy_EnableMotorAndDrive(driveNo) ;
	Floppy_bMotorOn[driveNo] = true ;
}

static void Floppy_StopMotor(DRIVE_NO driveNo)
{
	Floppy_bRequestMotorOff[driveNo] = true ;
}

static  void
Floppy_SetDataRate(byte bDataRate)
{
	PortCom_SendByte(CCR_CONFIG_CONTROL_REG, bDataRate & 0x03) ;
}

static  void
Floppy_EnableMotorAndDrive(DRIVE_NO driveNo)
{
	PortCom_SendByte(DOR_DIGITAL_OUTPUT_REG, (DOR_MOTOR_NO [ driveNo ] | DOR_DMA_GATE | DOR_RESET | driveNo)) ;
}

static  void
Floppy_DisableMotorAndDrive(DRIVE_NO driveNo)
{
	PortCom_SendByte(DOR_DIGITAL_OUTPUT_REG, (0x00 | DOR_DMA_GATE | DOR_RESET | driveNo)) ;
}

static  void
Floppy_Reset()
{
	PortCom_SendByte(DOR_DIGITAL_OUTPUT_REG, 0x08) ;
	PortCom_SendByte(DOR_DIGITAL_OUTPUT_REG, 0x0C) ;
}

static  byte
Floppy_InitDMAController(DMA_MODE DMAMode, unsigned uiWordCount)
{
	DMA_Request DMARequest ;
	DMARequest.DMAChannelNo = DMA_CH2 ;
	DMARequest.bDeviceID = DMA_DEVICE_ID_FLOPPY ;
	DMARequest.uiPhysicalAddress = MEM_DMA_FLOPPY_START ;
	DMARequest.uiWordCount = uiWordCount ;
	DMARequest.DMAMode = DMAMode ;

	byte bStatus;
	if((bStatus = DMA_RequestChannel(&DMARequest)) != DMA_SUCCESS)
	{
		printf("\n Init DMA in Floppy Failed. ErrorCode: %d\n", bStatus);
		return Floppy_ERR_DMA ;
	}

	return Floppy_SUCCESS ;
}

static byte
Floppy_SendControlCommand(byte bCmd)
{
	unsigned int uiCounter ;
	byte bStatus ;
	
	for(uiCounter = 0; uiCounter < 100000; uiCounter++)
	{
		bStatus = PortCom_ReceiveByte(MSR_MAIN_STATUS_REG) ;
		bStatus &= (MSR_RQM | MSR_DIO) ;
		if(bStatus == MSR_RQM)
		{
			PortCom_SendByte(DFR_DATA_FIFO_REG, bCmd) ;
			return Floppy_SUCCESS ;
		}
	}

	return Floppy_ERR_TIMEOUT ;
}

static byte
Floppy_SendSpecifyCommand()
{
	if(Floppy_SendControlCommand(FDD_SPECIFY_CMD) == Floppy_SUCCESS) 
		if(Floppy_SendControlCommand(SRTHUT) == Floppy_SUCCESS)
			if(Floppy_SendControlCommand(HLT_ND) == Floppy_SUCCESS)
				return Floppy_SUCCESS ;

	return Floppy_FAILURE ;
}

static byte
Floppy_CompleteResultPhase(byte bNoOfResultReplies)
{
	unsigned int uiCounter ;
	byte bStatus ;
	byte bCurrentNoOfResultReplies ;
	byte bErrCode = Floppy_ERR_TIMEOUT ;
	
	if(bNoOfResultReplies > MAX_RESULT_PHASE_REPLIES)
			return Floppy_ERR_INVALID_NO_OF_RESULT_REPLIES ;
			
	for(uiCounter = 0, bCurrentNoOfResultReplies = 0; uiCounter < 10000; uiCounter++)
	{
		bStatus = PortCom_ReceiveByte(MSR_MAIN_STATUS_REG) ;
		bStatus &= (MSR_RQM | MSR_DIO | MSR_CMD_BUSY) ;

		if(bStatus == MSR_RQM)
		{
			if(bCurrentNoOfResultReplies < bNoOfResultReplies)
			{
				bErrCode = Floppy_ERR_UNDERRUN ;
				break ;
			}

			return Floppy_SUCCESS ;
		}

		if(bStatus == (MSR_RQM | MSR_DIO | MSR_CMD_BUSY))
		{
			if(bCurrentNoOfResultReplies >= bNoOfResultReplies)
			{
				bErrCode = Floppy_ERR_OVERRUN ;
				break ;
			}
			Floppy_ReplyBuffer[bCurrentNoOfResultReplies++] = PortCom_ReceiveByte(DFR_DATA_FIFO_REG) ;
		}
	}

	return bErrCode ;
}

static byte
Floppy_SenseInterruptStatus()
{
	if(Floppy_SendControlCommand(FDD_SENSE_INT_STATUS_CMD) == Floppy_SUCCESS)
		return Floppy_CompleteResultPhase(2) ;
	return Floppy_ERR_TIMEOUT ;
}

static byte
Floppy_WaitForInterrupt()
{
	unsigned int uiCounter ;
	
	for(uiCounter = 0; uiCounter < 100; uiCounter++)
		asm("nop") ;

	return Floppy_SUCCESS ;
}

static byte
Floppy_ReCaliberate(DRIVE_NO driveNo)
{
	Floppy_SendControlCommand(FDD_RECALIBRATE_CMD) ;
	Floppy_SendControlCommand(driveNo) ;

	ProcessManager::Instance().WaitOnInterrupt(StdIRQ::Instance().FLOPPY_IRQ);
	Floppy_SenseInterruptStatus() ;

	if (Floppy_ReplyBuffer[0] & 0x20)
	{
		if (Floppy_ReplyBuffer[1] == 0x00)
			return Floppy_SUCCESS ;
	}

	return Floppy_ERR_RECALIBERATE ;
}

static byte
Floppy_Seek(DRIVE_NO driveNo, Floppy_HEAD_NO headNo, unsigned uiSeekTrack)
{
	Floppy_SendControlCommand(FDD_SEEK_CMD) ;
	Floppy_SendControlCommand((headNo << 2) | driveNo) ;
 	Floppy_SendControlCommand(uiSeekTrack) ;

	ProcessManager::Instance().WaitOnInterrupt(StdIRQ::Instance().FLOPPY_IRQ);
	Floppy_SenseInterruptStatus() ;

	if (Floppy_ReplyBuffer[0] & 0x20)
		return Floppy_SUCCESS ;

	return Floppy_ERR_SEEK ;
}

static byte
Floppy_FormatTrack(const DiskDrive* pDiskDrive, const Floppy_HEAD_NO headNo)
{
	byte bStatus ;
	
	if(Floppy_SendControlCommand(FDD_FORMAT_TRACK_CMD) == Floppy_SUCCESS)
		if(Floppy_SendControlCommand(((headNo << 2) & 4) | pDiskDrive->DriveNumber()) == Floppy_SUCCESS) //Drive 1
			if(Floppy_SendControlCommand(2) == Floppy_SUCCESS) // No of Bytes/Sector = 512
				if(Floppy_SendControlCommand(pDiskDrive->SectorsPerTrack()) == Floppy_SUCCESS)
					if(Floppy_SendControlCommand(GAP) == Floppy_SUCCESS)
						if(Floppy_SendControlCommand(0x00) == Floppy_SUCCESS) // Data Field Filler Value	
						{
							ProcessManager::Instance().WaitOnInterrupt(StdIRQ::Instance().FLOPPY_IRQ);
								
							if((bStatus = Floppy_CompleteResultPhase(MAX_RESULT_PHASE_REPLIES)) != Floppy_SUCCESS)
								return bStatus ;

							if((Floppy_ReplyBuffer[0] & 0xC0) == 0x00)
								return Floppy_SUCCESS ;
						}

	return Floppy_ERR_FORMAT ;	
}

static byte Floppy_ReadWrite(const DiskDrive* pDiskDrive, unsigned uiStartSectorNo, unsigned uiEndSectorNo, Floppy_MODE mode)
{
	byte bStatus ;
	
	if(uiStartSectorNo + 1 > pDiskDrive->SizeInSectors()
			|| uiEndSectorNo + 1 > pDiskDrive->SizeInSectors()
			|| uiStartSectorNo >= uiEndSectorNo)
		return Floppy_INVALID_SECTOR ;
		
	unsigned uiStartSector = uiStartSectorNo % pDiskDrive->SectorsPerTrack();
//	unsigned uiEndSector = uiEndSectorNo % pDiskDrive->SectorsPerTrack();
	unsigned uiRawTrack = uiStartSectorNo /	pDiskDrive->SectorsPerTrack();
	unsigned uiHead = uiRawTrack % pDiskDrive->NoOfHeads();
	unsigned uiTrack = uiRawTrack / pDiskDrive->NoOfHeads() ;
	unsigned uiSeekTrack = uiTrack ;

	Floppy_StartMotor(pDiskDrive->DriveNumber()) ;

//	ProcessManager_Sleep(100) ;
	
//	Floppy_SetDataRate(RATE) ;

	DMA_MODE DMAMode = DMA_MODE_WRITE ;
	byte bFloppyCmd = FDD_READ_CMD ;
	if(mode == FD_WRITE)
	{
		DMAMode = DMA_MODE_READ ;
		bFloppyCmd = FDD_WRITE_CMD ;
	}

	unsigned uiSeekRetryCount ;
	unsigned uiOperationRetryCount ;
	const unsigned uiNoOfRetrys = 3 ;

	bStatus = Floppy_ERR_READ ;
	for(uiSeekRetryCount = 0; uiSeekRetryCount < uiNoOfRetrys; uiSeekRetryCount++)
	{
		if(uiSeekRetryCount > 0)
			if((bStatus = Floppy_ReCaliberate(pDiskDrive->DriveNumber())) != Floppy_SUCCESS)
				continue ;

//		ProcessManager_Sleep(100) ;

		if((bStatus = Floppy_Seek(pDiskDrive->DriveNumber(), (Floppy_HEAD_NO)uiHead, uiSeekTrack)) != Floppy_SUCCESS)
			continue ;

		for(uiOperationRetryCount = 0; uiOperationRetryCount < uiNoOfRetrys; uiOperationRetryCount++)
		{
			DMA_ReleaseChannel(DMA_CH2) ;
			if((bStatus = Floppy_InitDMAController(DMAMode, ((uiEndSectorNo - uiStartSectorNo) * 512 - 1))) != Floppy_SUCCESS)
				continue ;

			if(Floppy_SendControlCommand(bFloppyCmd) == Floppy_SUCCESS)
				if(Floppy_SendControlCommand(((uiHead << 2) & 4) | pDiskDrive->DriveNumber()) == Floppy_SUCCESS) //Drive 1
					if(Floppy_SendControlCommand(uiTrack) == Floppy_SUCCESS) //Track 0
						if(Floppy_SendControlCommand(uiHead) == Floppy_SUCCESS) // Head 0
							if(Floppy_SendControlCommand(uiStartSector + 1) == Floppy_SUCCESS) //sector 1
								if(Floppy_SendControlCommand(2) == Floppy_SUCCESS) // No of Bytes/Sector = 512
									if(Floppy_SendControlCommand(pDiskDrive->SectorsPerTrack())== Floppy_SUCCESS) 
										if(Floppy_SendControlCommand(GAP) == Floppy_SUCCESS)
											if(Floppy_SendControlCommand(0xFF) == Floppy_SUCCESS) // DTL = 0xFF
											{
												ProcessManager::Instance().WaitOnInterrupt(StdIRQ::Instance().FLOPPY_IRQ);

												if((bStatus = Floppy_CompleteResultPhase1(MAX_RESULT_PHASE_REPLIES)) != Floppy_SUCCESS)
													continue ;

												if(Floppy_ReplyBuffer[0] & (0x00 | ((uiHead << 2) & 4) | pDiskDrive->DriveNumber()))
													return Floppy_SUCCESS ;
								
											}
		}
	}

	return bStatus ;
}

static byte Floppy_FullFormat(const DiskDrive* pDiskDrive)
{
	byte bStatus ;
	DRIVE_NO driveNo = pDiskDrive->DriveNumber();
	
	if(driveNo != FD_DRIVE0 && driveNo != FD_DRIVE1)
		return Floppy_ERR_INVALID_DRIVE ;
		
	Floppy_StartMotor(driveNo) ;
	
	Floppy_SetDataRate(RATE) ;

	if((bStatus = Floppy_ReCaliberate(pDiskDrive->DriveNumber())) != Floppy_SUCCESS)
		return bStatus ;

	Floppy_FormatFields Floppy_FormatData[pDiskDrive->SectorsPerTrack()] ;
	
	unsigned uiWordCount = pDiskDrive->SectorsPerTrack() * sizeof(Floppy_FormatFields)/* = 4 bytes */ ;
	unsigned uiTrackCount ;
	for(uiTrackCount = 0; uiTrackCount < pDiskDrive->TracksPerHead(); uiTrackCount++)
	{
		ProcessManager::Instance().Sleep(100) ;

		if((bStatus = Floppy_Seek(pDiskDrive->DriveNumber(), (Floppy_HEAD_NO)0, uiTrackCount)) != Floppy_SUCCESS)
			return bStatus ;

		DMA_ReleaseChannel(DMA_CH2) ;

		if((bStatus = Floppy_InitDMAController(DMA_MODE_READ, uiWordCount)) != Floppy_SUCCESS)
			return bStatus ;
			
		unsigned uiHeadCount ;
		for(uiHeadCount = 0; uiHeadCount < pDiskDrive->NoOfHeads(); uiHeadCount++)
		{
			unsigned uiSectorCount ;
			for(uiSectorCount = 0; uiSectorCount < pDiskDrive->SectorsPerTrack(); uiSectorCount++)
			{
				Floppy_FormatData[uiSectorCount].bTrack = uiTrackCount ; /* 0 based Index */
				Floppy_FormatData[uiSectorCount].bHead = uiHeadCount ;
				Floppy_FormatData[uiSectorCount].bSector = uiSectorCount + 1 ;
				Floppy_FormatData[uiSectorCount].bSectorSize = 2 ; /* 512bytes/Sector */
			}
			
			MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)&Floppy_FormatData,
								SYS_LINEAR_SELECTOR_DEFINED, MEM_DMA_FLOPPY_START, uiWordCount) ;

			if((bStatus = Floppy_FormatTrack(pDiskDrive, (Floppy_HEAD_NO)uiHeadCount)) != Floppy_SUCCESS)
				return bStatus ;
		}
	}
	
	return Floppy_SUCCESS ;	
}

static byte
Floppy_CompleteResultPhase1(byte bNoOfResultReplies)
{
	unsigned int uiCounter ;
	byte bStatus ;
	byte bCurrentNoOfResultReplies ;
	byte bErrCode = Floppy_ERR_TIMEOUT ;
	
	if(bNoOfResultReplies > MAX_RESULT_PHASE_REPLIES)
			return Floppy_ERR_INVALID_NO_OF_RESULT_REPLIES ;
			
	for(uiCounter = 0, bCurrentNoOfResultReplies = 0; uiCounter < 10000; uiCounter++)
	{
		bStatus = PortCom_ReceiveByte(MSR_MAIN_STATUS_REG) ;
		bStatus &= (MSR_RQM | MSR_DIO | MSR_CMD_BUSY) ;

		if(bStatus == MSR_RQM)
		{
			if(bCurrentNoOfResultReplies < bNoOfResultReplies)
			{
				bErrCode = Floppy_ERR_UNDERRUN ;
				break ;
			}

			return Floppy_SUCCESS ;
		}

		if(bStatus == (MSR_RQM | MSR_DIO | MSR_CMD_BUSY))
		{
			if(bCurrentNoOfResultReplies >= bNoOfResultReplies)
			{
				bErrCode = Floppy_ERR_OVERRUN ;
				break ;
			}
			Floppy_ReplyBuffer[bCurrentNoOfResultReplies++] = PortCom_ReceiveByte(DFR_DATA_FIFO_REG) ;
		}
	}

	return bErrCode ;
}

/********************************************************************************************/
void Floppy_Handler()
{
	unsigned GPRStack[NO_OF_GPR] ;
	AsmUtil_STORE_GPR(GPRStack) ;
	AsmUtil_SET_KERNEL_DATA_SEGMENTS

	StdIRQ::Instance().FLOPPY_IRQ.Signal();

	IrqManager::Instance().SendEOI(StdIRQ::Instance().FLOPPY_IRQ);

	AsmUtil_REVOKE_KERNEL_DATA_SEGMENTS
	AsmUtil_RESTORE_GPR(GPRStack) ;

	asm("leave") ;
	asm("IRET") ;
}

void Floppy_Initialize()
{
	bool bInitStatus = false ;
	Floppy_bInitStatus = false ;
	Floppy_Reset() ;

	if(IrqManager::Instance().RegisterIRQ(StdIRQ::Instance().FLOPPY_IRQ, (unsigned)&Floppy_Handler))
	{
		IrqManager::Instance().EnableIRQ(StdIRQ::Instance().FLOPPY_IRQ) ;
		
		Floppy_SetDataRate(RATE) ;

	//	ProcessManager_WaitOnInterrupt(StdIRQ::Instance().FLOPPY_IRQ) ;
		if(Floppy_WaitForInterrupt() == Floppy_SUCCESS)
		{
			bInitStatus = true ;
			int i ;
			for(i = 0; i < 4; i++)
			{
				bInitStatus = false ;
				if(Floppy_SenseInterruptStatus() == Floppy_SUCCESS)
					bInitStatus = true ;
			}
			
			if(bInitStatus == true)
				if(Floppy_SendSpecifyCommand() == Floppy_SUCCESS)
					bInitStatus = true ;
		}
	}
	else
	{
		printf("\n Failed to register Floppy IRQ: %d", StdIRQ::Instance().FLOPPY_IRQ.GetIRQNo());
		bInitStatus = false ;
	}

	if(bInitStatus == true)
	{
		bool boolEnhanced ;
		if(Floppy_IsEnhancedController(&boolEnhanced) == Floppy_SUCCESS)
		{
			if(boolEnhanced)
				KC::MDisplay().Message("\n\tEnhanced Floppy Controller : 8272A", Display::WHITE_ON_BLACK()) ;
			else
				KC::MDisplay().Message("\n\tOlder Floppy Controller : 8272A", Display::WHITE_ON_BLACK()) ;
		}
		else
			KC::MDisplay().Message("\n\tFailed To Get Floppy Controller Version", Display::WHITE_ON_BLACK()) ;
	
    DiskDriveManager::Instance().Create("floppya", DEV_FLOPPY, FD_DRIVE1,
      0, 2880,
      18, 80, 2,
      nullptr, DiskDriveManager::Instance().CreateRawDisk("floppy", FLOPPY_DISK, NULL),
      2048, 24, MEM_FD1_FS_START, MEM_FD1_FS_END);
		
		unsigned i ;
		for(i = 0; i < MAX_DRIVES; i++)
		{
			Floppy_bMotorOn[i] = false ;
			Floppy_bRequestMotorOff[i] = false ;
		}

		KernelUtil::ScheduleTimedTask("fmoncont", 5000, (unsigned)&Floppy_MotorController) ;
	}

	Floppy_bInitStatus = bInitStatus ;
	KC::MDisplay().LoadMessage("Floppy Initialization", bInitStatus ? Success : Failure) ;
}

bool Floppy_GetInitStatus()
{
	return Floppy_bInitStatus ;
}

byte Floppy_IsEnhancedController(bool* boolEnhanced)
{
	*boolEnhanced = false ;
	Floppy_SendControlCommand(FDD_VERSION_CMD) ;

	if(Floppy_CompleteResultPhase(1) != Floppy_SUCCESS)
		return Floppy_FAILURE ;

	if(Floppy_ReplyBuffer[0] == FDD_VERSION_ENHANCED)
		*boolEnhanced = true ;
	
	return Floppy_SUCCESS ;
}

byte Floppy_Read(const DiskDrive* pDiskDrive, unsigned uiStartSectorNo, unsigned uiEndSectorNo, byte* bSectorBuffer)
{
	byte bStatus = Floppy_ReadWrite(pDiskDrive, uiStartSectorNo, uiEndSectorNo, FD_READ) ;
	Floppy_StopMotor(pDiskDrive->DriveNumber()) ;
	DMA_ReleaseChannel(DMA_CH2) ;
	
	if(bStatus == Floppy_SUCCESS)
		MemUtil_CopyMemory(SYS_LINEAR_SELECTOR_DEFINED, MEM_DMA_FLOPPY_START, MemUtil_GetDS(), 
						(unsigned)bSectorBuffer, (uiEndSectorNo - uiStartSectorNo) * 512) ;

	return bStatus ;
}

byte Floppy_Write(const DiskDrive* pDiskDrive, unsigned uiStartSectorNo, unsigned uiEndSectorNo, byte* bSectorBuffer)
{
	MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)bSectorBuffer, SYS_LINEAR_SELECTOR_DEFINED, 
					MEM_DMA_FLOPPY_START, (uiEndSectorNo - uiStartSectorNo) * 512) ;
					
	byte bStatus = Floppy_ReadWrite(pDiskDrive, uiStartSectorNo, uiEndSectorNo, FD_WRITE) ;
	Floppy_StopMotor(pDiskDrive->DriveNumber()) ;
	DMA_ReleaseChannel(DMA_CH2) ;

	return bStatus ;
}

byte
Floppy_Format(const DiskDrive* pDiskDrive)
{
	byte bStatus = Floppy_FullFormat(pDiskDrive) ;
	Floppy_StopMotor(pDiskDrive->DriveNumber()) ;
	return bStatus ;	
}


