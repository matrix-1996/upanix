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
# include <USBMassBulkStorageDisk.h>
# include <PCIBusHandler.h>
# include <Display.h>
# include <DMM.h>
# include <MemManager.h>
# include <MemUtil.h>
# include <PortCom.h>
# include <stdio.h>
# include <cstring.h>
# include <StringUtil.h>

# include <USBStructures.h>
# include <USBDataHandler.h>
# include <EHCIDataHandler.h>
# include <EHCIController.h>

void EHCIController::DisplayStats()
{
	printf("\n Command: %x", _pOpRegs->uiUSBCommand) ;
	printf("\n Status: %x", _pOpRegs->uiUSBStatus) ;
	printf("\n Interrupt: %x", _pOpRegs->uiUSBInterrupt) ;
}

static void EHCIController_DisplayTransactionState(EHCIQueueHead* pQH, EHCIQTransferDesc* pTDStart)
{
	printf("\n QH Details:- ECap1: %x, ECap2: %x", pQH->uiEndPointCap_Part1, pQH->uiEndPointCap_Part2) ;
	printf("\n QTD Token: %x, Current: %x, Next: %x", pQH->uipQHToken, pQH->uiCurrrentpTDPointer, pQH->uiNextpTDPointer) ;
	printf("\n Alt_NAK: %x, BufferPtr0: %x", pQH->uiAltpTDPointer_NAKCnt, pQH->uiBufferPointer[ 0 ]) ;

	EHCIQTransferDesc* pTDCur = pTDStart ;
	unsigned i = 1 ;
	for(;(unsigned)pTDCur > 1;)
	{
		printf("\n TD %d Token: %x", i++, pTDCur->uipTDToken) ;
		pTDCur = (EHCIQTransferDesc*)KERNEL_VIRTUAL_ADDRESS(pTDCur->uiNextpTDPointer) ;
	}
}

bool EHCIController::PollWait(unsigned* pValue, int iBitPos, unsigned value)
{
	value &= 1;
	if(iBitPos > 31 || iBitPos < 0)
		return false ;

	int iMaxLimit = 10000 ; // 10 Sec
	unsigned uiSleepTime = 10 ; // 10 ms

	while(iMaxLimit > 10)
	{
		if( (((*pValue) >> iBitPos) & 1) == value )
			return true ;

		ProcessManager::Instance().Sleep(uiSleepTime) ;
		iMaxLimit -= uiSleepTime ;
	}

	return false ;
}

EHCITransaction::EHCITransaction(EHCIQueueHead* qh, EHCIQTransferDesc* tdStart)
  : _qh(qh), _tdStart(tdStart)
{
	EHCIQTransferDesc* tdCur = _tdStart;

	for(; (unsigned)tdCur != 1; tdCur = (EHCIQTransferDesc*)(KERNEL_VIRTUAL_ADDRESS(tdCur->uiNextpTDPointer)))
	{
    _dStorageList.push_back(KERNEL_VIRTUAL_ADDRESS(tdCur->uiBufferPointer[0]));
    _dStorageList.push_back((unsigned)tdCur);
	}

	_qh->uiNextpTDPointer = KERNEL_REAL_ADDRESS(_tdStart);
}

bool EHCITransaction::PollWait()
{
	const unsigned uiSleepTime = 10 ; // 10 ms
	int iMaxLimit = 10000 ; // 10 Sec

	EHCIQTransferDesc* tdCur;

	while(iMaxLimit > 10)
	{
		int poll = 10000;
		while(poll > 0)
		{
			if(_qh->uiNextpTDPointer == 1)
			{
				for(tdCur = _tdStart; (unsigned)tdCur != 1; )
				{
					if((tdCur->uipTDToken & 0xFE))
						break ;

				  tdCur = (EHCIQTransferDesc*)(KERNEL_VIRTUAL_ADDRESS(tdCur->uiNextpTDPointer)) ;
				}

				if((unsigned)tdCur == 1)
					return true ;
			}
			--poll ;
		}
		iMaxLimit -= uiSleepTime ;
		if(iMaxLimit == 20)
			ProcessManager::Instance().Sleep(uiSleepTime) ;
	}

	return false ;
}

void EHCITransaction::Clear()
{
	EHCIDataHandler_CleanAysncQueueHead(_qh);
	
  for(auto i : _dStorageList)
	{
		if(i != NULL)
			DMM_DeAllocateForKernel(i);
	}
  _dStorageList.clear();
}

void EHCIController::AddAsyncQueueHead(EHCIQueueHead* pQH)
{
	pQH->uiHeadHorizontalLink = _pAsyncReclaimQueueHead->uiHeadHorizontalLink ;
	_pAsyncReclaimQueueHead->uiHeadHorizontalLink = KERNEL_REAL_ADDRESS(pQH) | 0x2 ;
}

byte EHCIController::SetConfigFlag(bool bSet)
{
	unsigned uiCompareValue = bSet ? 0 : 1 ;
	unsigned uiConfigFlag = _pOpRegs->uiConfigFlag ;

	if((uiConfigFlag & 0x1) == uiCompareValue)
	{
		if(bSet)
			printf("\n Routing ports to main EHCI controller. Turning on CF Bit") ;
		else
			printf("\n Routing ports to companion host controller. Turning off CF Bit") ;

		if(bSet)
			uiConfigFlag |= 1 ;
		else
			uiConfigFlag &= ~(1) ;

		_pOpRegs->uiConfigFlag = uiConfigFlag ;

		ProcessManager::Instance().Sleep(100) ;
		if((_pOpRegs->uiConfigFlag & 0x1) == uiCompareValue)
		{
			printf("\n Failed to Set Config Flag to: %d:", bSet ? 1 : 0) ;
			return EHCIController_FAILURE ;
		}
	}
	else
	{
		printf("\n Config Bit is already set to: %d", bSet ? 1 : 0) ;
	}

	return EHCIController_SUCCESS ;
}

void EHCIController::SetupInterrupts()
{
	_pOpRegs->uiUSBInterrupt = _pOpRegs->uiUSBInterrupt	| INTR_ASYNC_ADVANCE 
																		| INTR_HOST_SYS_ERR 
																		| INTR_PORT_CHG 
																		| INTR_USB_ERR 
																		| INTR_USB ;
	_pOpRegs->uiUSBInterrupt = 0 ;
}

byte EHCIController::SetupPeriodicFrameList()
{
	unsigned uiFreePageNo = MemManager::Instance().AllocatePhysicalPage();

	unsigned* pFrameList = (unsigned*)(uiFreePageNo * PAGE_SIZE - GLOBAL_DATA_SEGMENT_BASE) ;
	
	int i ;
	for(i = 0; i < 1024; i++)
		pFrameList[ i ] = 0x1 ;

	_pOpRegs->uiPeriodicListBase = (unsigned)pFrameList + GLOBAL_DATA_SEGMENT_BASE ;

	return EHCIController_SUCCESS;
}

byte EHCIController::SetupAsyncList()
{
	unsigned uiQHAddress = DMM_AllocateAlignForKernel(sizeof(EHCIQueueHead), 32);
	memset((void*)(uiQHAddress), 0, sizeof(EHCIQueueHead));
	_pOpRegs->uiAsyncListBase = KERNEL_REAL_ADDRESS(uiQHAddress);

	EHCIQueueHead* pQHH = (EHCIQueueHead*)uiQHAddress;

	pQHH->uiCurrrentpTDPointer = 0;
	pQHH->uiNextpTDPointer = 1;
	pQHH->uiAltpTDPointer_NAKCnt = 1;
	
	pQHH->uiEndPointCap_Part1 = (1 << 15);
	pQHH->uipQHToken = (1 << 6);

	pQHH->uiHeadHorizontalLink = KERNEL_REAL_ADDRESS(uiQHAddress) | 0x2;

	_pAsyncReclaimQueueHead = pQHH;

	return EHCIController_SUCCESS;
}

void EHCIController::SetSchedEnable(unsigned uiScheduleType, bool bEnable)
{
	if(bEnable)
		_pOpRegs->uiUSBCommand |= uiScheduleType ;
	else
		_pOpRegs->uiUSBCommand &= ~(uiScheduleType) ;
}

void EHCIController::SetFrameListSize()
{
	if((_pCapRegs->uiHCCParams & 0x20) != 0)
		_pOpRegs->uiUSBCommand &= ~(0xC) ;
}

void EHCIController::Start()
{
	_pOpRegs->uiUSBCommand |= 1 ;
}

void EHCIController::Stop()
{
	_pOpRegs->uiUSBCommand &= 0xFFFFFFFE ;
}

bool EHCIController::CheckHCActive()
{
	return (_pOpRegs->uiUSBCommand & 1) ? true : false ;
} 

bool EHCIController::WaitCheckAsyncScheduleStatus(bool bValue)
{
	bool bStatus = PollWait(&(_pOpRegs->uiUSBCommand), 5, bValue) ;

	if(bStatus)
		bStatus = PollWait(&(_pOpRegs->uiUSBStatus), 15, bValue) ;

	return bStatus ;
}

bool EHCIController::StartAsyncSchedule()
{
	SetSchedEnable(ASYNC_SCHEDULE_ENABLE, true) ;

	if(!WaitCheckAsyncScheduleStatus(true))
	{
		printf("\n Failed to Start Async Schedule") ;
		DisplayStats() ;
		return false ;
	}

	if(!CheckHCActive())
	{
		printf("\n Controller has stopped !!!") ;
		DisplayStats() ;
		return false ;
	}

	return true ;
}

bool EHCIController::StopAsyncSchedule()
{
	SetSchedEnable(ASYNC_SCHEDULE_ENABLE, false) ;

	if(!WaitCheckAsyncScheduleStatus(false))
	{
		printf("\n Failed to Stop Async Schedule") ;
		DisplayStats() ;
		return false ;
	}

	if(!CheckHCActive())
	{
		printf("\n Controller has stopped !!!") ;
		DisplayStats() ;
		return false ;
	}

	return true ;
}

unsigned EHCIController::GetNoOfPortsActual()
{
	return (_pCapRegs->uiHCSParams) & 0xF ;
}

unsigned EHCIController::GetNoOfPorts()
{
	unsigned uiNoOfPorts = GetNoOfPortsActual() ;

	if(uiNoOfPorts > MAX_EHCI_ENTRIES)
	{
		printf("\n No. of Ports available on EHCI Controller is: %d", uiNoOfPorts) ;
		printf("\n This is more than max supported by system software. Using only first %d ports", MAX_EHCI_ENTRIES) ;
		uiNoOfPorts = MAX_EHCI_ENTRIES ;
	}

	return uiNoOfPorts ;
}

void EHCIController::SetupPorts()
{
	printf("\n Setting up EHCI Controller Ports") ;

	unsigned uiNoOfPorts = GetNoOfPorts() ;
	bool bPPC = (_pCapRegs->uiHCSParams & 0x10) == 0x10 ;

	printf("\n No. of ports = %d", uiNoOfPorts) ;
	if(bPPC)
		printf("\n Software Port Power Control") ;
	else
		printf("\n Hardware Port Power Control") ;

	unsigned i ;
	for(i = 0; i < uiNoOfPorts; i++)
	{
		unsigned* pPort = &(_pOpRegs->uiPortReg[ i ]) ;

		printf("\n Setting up Port: %d. Initial Value: %x", i, *pPort) ;

		if(bPPC)
		{
			*pPort |= (1 << 12) ;
			ProcessManager::Instance().Sleep(100) ;
		}

		if((*pPort & (1 << 13)))
		{
			printf("\n EHCI do not own this port. Not setting up this port.") ;
			continue ;
		}

		// Wake up enable on OverCurrent, Disconnect and Connect
		*pPort |= (0x3 << 20) ;

		// Turn off Port Test Control and Port Indicator Control
		*pPort &= (~(0x3F << 14)) ;

		// Perform Port Reset
		*pPort = (*pPort | 0x100) & ~(0x4) ;
		ProcessManager::Instance().Sleep(200) ;
		*pPort &= (~(0x100)) ;
		ProcessManager::Instance().Sleep(500) ;

		if(!(*pPort & 0x4))
		{
			printf("\n Could not enable this port") ;
			continue ;
		}

		printf("\n Port successfully enabled") ;
		if((*pPort & 0x1))
			printf("\n High Speed Device Connected to this port") ;

		printf("\n New value of Port: %d is: %x", i, *pPort) ;
		break ;
	}
}

static byte EHCIController_SetupBuffer(EHCIQTransferDesc* pTD, unsigned uiAddress, unsigned uiSize)
{
	unsigned uiFirstPagePart = PAGE_SIZE - (uiAddress % PAGE_SIZE) ;

	pTD->uiBufferPointer[ 0 ] = KERNEL_REAL_ADDRESS(uiAddress) ;

	if(uiFirstPagePart >= uiSize)
		return EHCIController_SUCCESS ;

	pTD->uiBufferPointer[ 1 ] = pTD->uiBufferPointer[ 0 ] + uiFirstPagePart ;

	uiSize -= uiFirstPagePart ;

	unsigned i ;
	for(i = 2; uiSize > PAGE_SIZE && i < 5; i++, uiSize -= PAGE_SIZE)
	{
		pTD->uiBufferPointer[ i ] = pTD->uiBufferPointer[ i - 1 ] + PAGE_SIZE ;
	}

	if(uiSize > PAGE_SIZE)
	{
		printf("\n EHCI TD Buffer Allocation Math is incorrect in page boundary calculation !") ;
		DMM_DeAllocateForKernel(uiAddress) ;
		return EHCIController_FAILURE ;
	}

	return EHCIController_SUCCESS ;
}

static byte EHCIController_SetupAllocBuffer(EHCIQTransferDesc* pTD, unsigned uiSize)
{
	static const unsigned MAX_LEN = PAGE_SIZE * 4 ;

	if(uiSize > MAX_LEN)
	{
		printf("\n EHCI QTD Buffer Size: %d greater than Limit (PAGE_SIZE * 4): %d", uiSize, MAX_LEN) ;
		return EHCIController_FAILURE ;
	}

	unsigned uiAddress = DMM_AllocateForKernel(uiSize) ;

	return EHCIController_SetupBuffer(pTD, uiAddress, uiSize) ;
}

byte EHCIController::AsyncDoorBell()
{
	if(!(_pOpRegs->uiUSBCommand & (1 << 5)) || !(_pOpRegs->uiUSBStatus & (1 << 15)))
	{
		printf("\n Asycn Schedule is disabled. DoorBell HandShake cannot be performed.") ;
		return EHCIController_FAILURE ;
	}

	if(_pOpRegs->uiUSBCommand & (1 << 6))
	{
		printf("\n Already a DoorBell is in progress!") ;
		return EHCIController_FAILURE ;
	}

	_pOpRegs->uiUSBStatus |= ((1 << 5)) ;
	_pOpRegs->uiUSBCommand |= (1 << 6) ;

	bool bStatus = PollWait(&(_pOpRegs->uiUSBCommand), 6, 0) ;

	if(bStatus)
		bStatus = PollWait(&(_pOpRegs->uiUSBStatus), 5, 1) ;

	_pOpRegs->uiUSBStatus |= ((1 << 5)) ;

	if(!bStatus)
		return EHCIController_FAILURE ;

	return EHCIController_SUCCESS ;
}

EHCIQueueHead* EHCIController::CreateDeviceQueueHead(int iMaxPacketSize, int iEndPtAddr, int iDevAddr)
{
	EHCIQueueHead* pQH = EHCIDataHandler_CreateAsyncQueueHead() ;

	pQH->uiAltpTDPointer_NAKCnt = 1 ;
	pQH->uiNextpTDPointer = 1 ;

	pQH->uiEndPointCap_Part1 = (5 << 28) | ((iMaxPacketSize & 0x7FF) << 16) | (1 << 14) | (2 << 12) | ((iEndPtAddr & 0xF) << 8) | (iDevAddr & 0x7F) ;
	pQH->uiEndPointCap_Part2 = (1 << 30) ;

	AddAsyncQueueHead(pQH) ;

	return pQH ;
}

static byte EHCIController_SetAddress(EHCIDevice* pDevice, int devAddr)
{
	EHCIController* pController = pDevice->pController ;
	EHCIQueueHead* pControlQH = pDevice->pControlQH ;

	// Setup TDs
	EHCIQTransferDesc* pTDStart = EHCIDataHandler_CreateAsyncQTransferDesc() ;

	pTDStart->uiAltpTDPointer = 1 ;
	pTDStart->uipTDToken = (8 << 16) | (3 << 10) | (2 << 8) | (1 << 7) ;

	if(EHCIController_SetupAllocBuffer(pTDStart, sizeof(USBDevRequest)) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		return EHCIController_FAILURE ;
	}

	USBDevRequest* pDevRequest = (USBDevRequest*)(KERNEL_VIRTUAL_ADDRESS(pTDStart->uiBufferPointer[ 0 ])) ;
	pDevRequest->bRequestType = 0x00 ;
	pDevRequest->bRequest = 5 ;
	pDevRequest->usWValue = (devAddr & 0xFF) ;
	pDevRequest->usWIndex = 0 ;
	pDevRequest->usWLength = 0 ;
	
	EHCIQTransferDesc* pTD1 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTDStart->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD1) ;
	pTD1->uiNextpTDPointer = 1 ;
	pTD1->uiAltpTDPointer = 1 ;
	pTD1->uipTDToken = (1 << 31) | (3 << 10) | (1 << 8) | (1 << 7) ;

	EHCITransaction aTransaction(pControlQH, pTDStart);
	
	if(!aTransaction.PollWait())
	{
		printf("\n Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pControlQH, pTDStart) ;
		pController->DisplayStats() ;
    aTransaction.Clear();
		return EHCIController_FAILURE ;
	}

  aTransaction.Clear();

	pControlQH->uiEndPointCap_Part1 |= (devAddr & 0x7F) ;

	if(pController->AsyncDoorBell() != EHCIController_SUCCESS)
	{
		printf("\n Async Door Bell failed while Setting Device Address") ;
		pController->DisplayStats() ;
		return EHCIController_FAILURE ;
	}
	
	return EHCIController_SUCCESS ;
}

static byte EHCIController_GetDescriptor(EHCIDevice* pDevice, unsigned short usDescValue, unsigned short usIndex, int iLen, void* pDestDesc)
{
	EHCIController* pController = pDevice->pController ;
	EHCIQueueHead* pControlQH = pDevice->pControlQH ;

	// Setup TDs
	EHCIQTransferDesc* pTDStart = EHCIDataHandler_CreateAsyncQTransferDesc() ;

	pTDStart->uiAltpTDPointer = 1 ;
	pTDStart->uipTDToken = (8 << 16) | (3 << 10) | (2 << 8) | (1 << 7) ;

	if(EHCIController_SetupAllocBuffer(pTDStart, sizeof(USBDevRequest)) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		return EHCIController_FAILURE ;
	}

	if(iLen < 0)
		iLen = DEF_DESC_LEN ;

	USBDevRequest* pDevRequest = (USBDevRequest*)(KERNEL_VIRTUAL_ADDRESS(pTDStart->uiBufferPointer[ 0 ])) ;
	pDevRequest->bRequestType = 0x80 ;
	pDevRequest->bRequest = 6 ;
	pDevRequest->usWValue = usDescValue ;
	pDevRequest->usWIndex = usIndex ;
	pDevRequest->usWLength = iLen ;

	EHCIQTransferDesc* pTD1 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTDStart->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD1) ;
	pTD1->uiAltpTDPointer = 1 ;
	pTD1->uipTDToken = (1 << 31) | (iLen << 16) | (3 << 10) | (1 << 8) | (1 << 7) ;
	
	if(EHCIController_SetupAllocBuffer(pTD1, iLen) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		DMM_DeAllocateForKernel((unsigned)pTD1) ;
		return EHCIController_FAILURE ;
	}

	// It is important to store the data buffer address now for later read as the data buffer address
	// in TD will be incremented by Host Controller with the number of bytes read
	unsigned uiDataBuffer = KERNEL_VIRTUAL_ADDRESS(pTD1->uiBufferPointer[ 0 ]) ;

	EHCIQTransferDesc* pTD2 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTD1->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD2) ;
	pTD2->uiNextpTDPointer = 1 ;
	pTD2->uiAltpTDPointer = 1 ;
	pTD2->uipTDToken = (3 << 10) | (1 << 7) ;

	EHCITransaction aTransaction(pControlQH, pTDStart);

	if(!aTransaction.PollWait())
	{
		printf("\n Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pControlQH, pTDStart) ;
		pController->DisplayStats() ;
    aTransaction.Clear();
		return EHCIController_FAILURE ;
	}
	
	MemUtil_CopyMemory(MemUtil_GetDS(), uiDataBuffer, MemUtil_GetDS(), (unsigned)pDestDesc, iLen) ;

  aTransaction.Clear();

	return EHCIController_SUCCESS ;
}

static byte EHCIController_GetDeviceDescriptor(EHCIDevice* pDevice, USBStandardDeviceDesc* pDevDesc)
{
	byte bStatus ;

	USBDataHandler_InitDevDesc(pDevDesc) ;

	RETURN_IF_NOT(bStatus, EHCIController_GetDescriptor(pDevice, 0x100, 0, -1, pDevDesc), EHCIController_SUCCESS) ;

	byte len = pDevDesc->bLength ;

	if(len >= sizeof(USBStandardDeviceDesc))
		len = sizeof(USBStandardDeviceDesc) ;

	RETURN_IF_NOT(bStatus, EHCIController_GetDescriptor(pDevice, 0x100, 0, len, pDevDesc), EHCIController_SUCCESS) ;

	return EHCIController_SUCCESS ;
}

static byte EHCIController_GetConfigValue(EHCIDevice* pDevice, char* bConfigValue)
{
	EHCIController* pController = pDevice->pController ;
	EHCIQueueHead* pControlQH = pDevice->pControlQH ;

	EHCIQTransferDesc* pTDStart = EHCIDataHandler_CreateAsyncQTransferDesc() ;

	pTDStart->uiAltpTDPointer = 1 ;
	pTDStart->uipTDToken = (8 << 16) | (3 << 10) | (2 << 8) | (1 << 7) ;

	if(EHCIController_SetupAllocBuffer(pTDStart, sizeof(USBDevRequest)) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		return EHCIController_FAILURE ;
	}

	USBDevRequest* pDevRequest = (USBDevRequest*)(KERNEL_VIRTUAL_ADDRESS(pTDStart->uiBufferPointer[ 0 ])) ;
	pDevRequest->bRequestType = 0x80 ;
	pDevRequest->bRequest = 8 ;
	pDevRequest->usWValue = 0 ;
	pDevRequest->usWIndex = 0 ;
	pDevRequest->usWLength = 1 ;
	
	EHCIQTransferDesc* pTD1 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTDStart->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD1) ;
	pTD1->uiAltpTDPointer = 1 ;
	pTD1->uipTDToken = (1 << 31) | (1 << 16) | (3 << 10) | (1 << 8) | (1 << 7) ;
	
	if(EHCIController_SetupAllocBuffer(pTD1, 1) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		DMM_DeAllocateForKernel((unsigned)pTD1) ;
		return EHCIController_FAILURE ;
	}

	EHCIQTransferDesc* pTD2 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTD1->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD2) ;
	pTD2->uiNextpTDPointer = 1 ;
	pTD2->uiAltpTDPointer = 1 ;
	pTD2->uipTDToken = (3 << 10) | (1 << 7) ;

	unsigned uiDataBuffer = KERNEL_VIRTUAL_ADDRESS(pTD1->uiBufferPointer[ 0 ]) ;

	EHCITransaction aTransaction(pControlQH, pTDStart);

	if(!aTransaction.PollWait())
	{
		printf("\n Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pControlQH, pTDStart) ;
		pController->DisplayStats() ;
    aTransaction.Clear();
		return EHCIController_FAILURE ;
	}
	
	*bConfigValue = *((char*)(uiDataBuffer)) ;
		
  aTransaction.Clear();

	return EHCIController_SUCCESS ;
}

static byte EHCIController_SetConfiguration(EHCIDevice* pDevice, char bConfigValue)
{
	EHCIController* pController = pDevice->pController ;
	EHCIQueueHead* pControlQH = pDevice->pControlQH ;

	// Setup TDs
	EHCIQTransferDesc* pTDStart = EHCIDataHandler_CreateAsyncQTransferDesc() ;

	pTDStart->uiAltpTDPointer = 1 ;
	pTDStart->uipTDToken = (8 << 16) | (3 << 10) | (2 << 8) | (1 << 7) ;

	if(EHCIController_SetupAllocBuffer(pTDStart, sizeof(USBDevRequest)) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		return EHCIController_FAILURE ;
	}

	USBDevRequest* pDevRequest = (USBDevRequest*)(KERNEL_VIRTUAL_ADDRESS(pTDStart->uiBufferPointer[ 0 ])) ;
	pDevRequest->bRequestType = 0x00 ;
	pDevRequest->bRequest = 9 ;
	pDevRequest->usWValue = bConfigValue ;
	pDevRequest->usWIndex = 0 ;
	pDevRequest->usWLength = 0 ;

	EHCIQTransferDesc* pTD1 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTDStart->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD1) ;
	pTD1->uiNextpTDPointer = 1 ;
	pTD1->uiAltpTDPointer = 1 ;
	pTD1->uipTDToken = (1 << 31) | (3 << 10) | (1 << 8) | (1 << 7) ;

	EHCITransaction aTransaction(pControlQH, pTDStart);

	if(!aTransaction.PollWait())
	{
		printf("\n Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pControlQH, pTDStart) ;
		pController->DisplayStats() ;
    aTransaction.Clear();
		return EHCIController_FAILURE ;
	}
	
  aTransaction.Clear();

	return EHCIController_SUCCESS ;
}

static byte EHCIController_CheckConfiguration(EHCIDevice* pDevice, char* bConfigValue, char bNumConfigs)
{
	if(bNumConfigs <= 0)
	{
		printf("\n Invalid NumConfigs: %d", bNumConfigs) ;
		return EHCIController_ERR_CONFIG ;
	}

	if(*bConfigValue <= 0 || *bConfigValue > bNumConfigs)
	{
		byte bStatus ;
		printf("\n Current Configuration %d -> Invalid. Setting to Configuration 1", *bConfigValue) ;
		RETURN_MSG_IF_NOT(bStatus, EHCIController_SetConfiguration(pDevice, 1), EHCIController_SUCCESS, "SetConfiguration Failed") ;
		*bConfigValue = 1 ;
	}

	return EHCIController_SUCCESS ;
}

static byte EHCIController_GetConfigDescriptor(EHCIDevice* pDevice, char bNumConfigs, USBStandardConfigDesc** pConfigDesc)
{
	byte bStatus = EHCIController_SUCCESS ;
	int index ;

	USBStandardConfigDesc* pCD = (USBStandardConfigDesc*)DMM_AllocateForKernel(sizeof(USBStandardConfigDesc) * bNumConfigs) ;

	for(index = 0; index < (int)bNumConfigs; index++)
		USBDataHandler_InitConfigDesc(&pCD[index]) ;

	for(index = 0; index < (int)bNumConfigs; index++)
	{
		unsigned short usDescValue = (0x2 << 8) | (index & 0xFF) ;

		bStatus = EHCIController_GetDescriptor(pDevice, usDescValue, 0, -1, &(pCD[index])) ;

		if(bStatus != EHCIController_SUCCESS)
			break ;

		int iLen = pCD[index].wTotalLength;

		void* pBuffer = (void*)DMM_AllocateForKernel(iLen) ;

		bStatus = EHCIController_GetDescriptor(pDevice, usDescValue, 0, iLen, pBuffer) ;

		if(bStatus != EHCIController_SUCCESS)
		{
			DMM_DeAllocateForKernel((unsigned)pBuffer) ;
			break ;
		}

		USBDataHandler_CopyConfigDesc(&pCD[index], pBuffer, pCD[index].bLength) ;

		USBDataHandler_DisplayConfigDesc(&pCD[index]) ;

		printf("\n Parsing Interface information for Configuration: %d", index) ;
		printf("\n Number of Interfaces: %d", (int)pCD[ index ].bNumInterfaces) ;

		void* pInterfaceBuffer = (char*)pBuffer + pCD[index].bLength ;

		pCD[index].pInterfaces = (USBStandardInterface*)DMM_AllocateForKernel(sizeof(USBStandardInterface) * pCD[index].bNumInterfaces) ;
		int iI ;

		for(iI = 0; iI < (int)pCD[index].bNumInterfaces; iI++)
		{
			USBDataHandler_InitInterfaceDesc(&(pCD[index].pInterfaces[iI])) ;
		}

		for(iI = 0; iI < (int)pCD[index].bNumInterfaces; iI++)
		{
			USBStandardInterface* pInt = (USBStandardInterface*)(pInterfaceBuffer) ;

			int iIntLen = sizeof(USBStandardInterface) - sizeof(USBStandardEndPt*) ;
			int iCopyLen = pInt->bLength ;

			if(iCopyLen > iIntLen)
				iCopyLen = iIntLen ;

			MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pInt, 
								MemUtil_GetDS(), (unsigned)&(pCD[index].pInterfaces[iI]),
								iCopyLen) ;

			USBDataHandler_DisplayInterfaceDesc(&(pCD[index].pInterfaces[iI])) ;

			USBStandardEndPt* pEndPtBuffer = (USBStandardEndPt*)((char*)pInterfaceBuffer + pInt->bLength) ;

			int iNumEndPoints = pCD[index].pInterfaces[iI].bNumEndpoints ;

			pCD[index].pInterfaces[iI].pEndPoints = (USBStandardEndPt*)DMM_AllocateForKernel(
														sizeof(USBStandardEndPt) * iNumEndPoints) ;

			printf("\n Parsing EndPoints for Interface: %d of Configuration: %d", iI, index) ;

			int iE ;
			for(iE = 0; iE < iNumEndPoints; iE++)
				USBDataHandler_InitEndPtDesc(&(pCD[index].pInterfaces[iI].pEndPoints[iE])) ;

			for(iE = 0; iE < iNumEndPoints; iE++)
			{
				USBStandardEndPt* pEndPt = &pEndPtBuffer[iE] ;

				int iELen = sizeof(USBStandardEndPt) ;
				int iECopyLen = pEndPt->bLength ;
				
				if(iECopyLen > iELen)
					iECopyLen = iELen ;

				MemUtil_CopyMemory(MemUtil_GetDS(), (unsigned)pEndPt, 
									MemUtil_GetDS(), (unsigned)&(pCD[index].pInterfaces[iI].pEndPoints[iE]),
									iECopyLen) ;

				USBDataHandler_DisplayEndPtDesc(&(pCD[index].pInterfaces[iI].pEndPoints[iE])) ;
			}

			pInterfaceBuffer = (USBStandardInterface*)((char*)pEndPtBuffer + iNumEndPoints * sizeof(USBStandardEndPt)) ;
		}

		DMM_DeAllocateForKernel((unsigned)pBuffer) ;
		pBuffer = NULL ;
	}

	if(bStatus != EHCIController_SUCCESS)
	{
		USBDataHandler_DeAllocConfigDesc(pCD, bNumConfigs) ;
		return bStatus ;
	}

	*pConfigDesc = pCD ;

	return EHCIController_SUCCESS ;
}

static byte EHCIController_GetStringDescriptorZero(EHCIDevice* pDevice, USBStringDescZero** ppStrDescZero)
{
	unsigned short usDescValue = (0x3 << 8) ;
	char szPart[ 8 ];

	byte bStatus = EHCIController_GetDescriptor(pDevice, usDescValue, 0, -1, szPart) ;

	if(bStatus != EHCIController_SUCCESS)
		return bStatus ;

	int iLen = ((USBStringDescZero*)&szPart)->bLength ;
	printf("\n String Desc Zero Len: %d", iLen) ;

	byte* pStringDescZero = (byte*)DMM_AllocateForKernel(iLen) ;

	bStatus = EHCIController_GetDescriptor(pDevice, usDescValue, 0, iLen, pStringDescZero) ;

	if(bStatus != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pStringDescZero) ;
		return bStatus ;
	}

	*ppStrDescZero = (USBStringDescZero*)DMM_AllocateForKernel(sizeof(USBStringDescZero)) ;

	USBDataHandler_CopyStrDescZero(*ppStrDescZero, pStringDescZero) ;
	USBDataHandler_DisplayStrDescZero(*ppStrDescZero) ;

	DMM_DeAllocateForKernel((unsigned)pStringDescZero) ;

	return EHCIController_SUCCESS ;
}

static byte EHCIController_GetDeviceStringDetails(USBDevice* pUSBDevice)
{
	if(pUSBDevice->usLangID == 0)
	{
		strcpy(pUSBDevice->szManufacturer, "Unknown") ;
		strcpy(pUSBDevice->szProduct, "Unknown") ;
		strcpy(pUSBDevice->szSerialNum, "Unknown") ;
		return EHCIController_SUCCESS ;
	}

	EHCIDevice* pDevice = (EHCIDevice*)pUSBDevice->pRawDevice ;

	unsigned short usDescValue = (0x3 << 8) ;
	char szPart[ 8 ];

	const int index_size = 3 ; // Make sure to change this with index[] below
	char arr_index[] = { pUSBDevice->deviceDesc.indexManufacturer, pUSBDevice->deviceDesc.indexProduct, pUSBDevice->deviceDesc.indexSerialNum } ;
	char* arr_name[] = { pUSBDevice->szManufacturer, pUSBDevice->szProduct, pUSBDevice->szSerialNum } ;

	int i ;
	for(i = 0; i < index_size; i++)
	{
		int index = arr_index[ i ] ;
		byte bStatus = EHCIController_GetDescriptor(pDevice, usDescValue | index, pUSBDevice->usLangID, -1, szPart) ;

		if(bStatus != EHCIController_SUCCESS)
			return bStatus ;

		int iLen = ((USBStringDescZero*)&szPart)->bLength ;
		printf("\n String Index: %u, String Desc Size: %d", index, iLen) ;
		if(iLen == 0)
		{
			strcpy(arr_name[i], "Unknown") ;
			continue ;
		}

		byte* pStringDesc = (byte*)DMM_AllocateForKernel(iLen) ;
		bStatus = EHCIController_GetDescriptor(pDevice, usDescValue | index, pUSBDevice->usLangID, iLen, pStringDesc) ;

		if(bStatus != EHCIController_SUCCESS)
		{
			DMM_DeAllocateForKernel((unsigned)pStringDesc) ;
			return bStatus ;
		}

		int j, k ;
		for(j = 0, k = 0; k < USB_MAX_STR_LEN && j < (iLen - 2); k++, j += 2)
		{
			//TODO: Ignoring Unicode 2nd byte for the time.
			arr_name[i][k] = pStringDesc[j + 2] ;
		}

		arr_name[i][k] = '\0' ;
	}

	USBDataHandler_DisplayDeviceStringDetails(pUSBDevice) ;

	return EHCIController_SUCCESS ;
}

static bool EHCIController_GetMaxLun(USBDevice* pUSBDevice, byte* bLUN)
{
	EHCIDevice* pDevice = (EHCIDevice*)(pUSBDevice->pRawDevice) ;
	EHCIController* pController = pDevice->pController ;
	EHCIQueueHead* pControlQH = pDevice->pControlQH ;

	// Setup TDs
	EHCIQTransferDesc* pTDStart = EHCIDataHandler_CreateAsyncQTransferDesc() ;

	pTDStart->uiAltpTDPointer = 1 ;
	pTDStart->uipTDToken = (8 << 16) | (3 << 10) | (2 << 8) | (1 << 7) ;

	if(EHCIController_SetupAllocBuffer(pTDStart, sizeof(USBDevRequest)) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		return false ;
	}

	USBDevRequest* pDevRequest = (USBDevRequest*)(KERNEL_VIRTUAL_ADDRESS(pTDStart->uiBufferPointer[ 0 ])) ;
	pDevRequest->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN ;
	pDevRequest->bRequest = 0xFE ;
	pDevRequest->usWValue = 0 ;
	pDevRequest->usWIndex = pUSBDevice->bInterfaceNumber ;
	pDevRequest->usWLength = 1 ;

	EHCIQTransferDesc* pTD1 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTDStart->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD1) ;
	pTD1->uiAltpTDPointer = 1 ;
	pTD1->uipTDToken = (1 << 31) | (1 << 16) | (3 << 10) | (1 << 8) | (1 << 7) ;
	
	if(EHCIController_SetupAllocBuffer(pTD1, 1) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		DMM_DeAllocateForKernel((unsigned)pTD1) ;
		return false ;
	}

	// It is important to store the data buffer address now for later read as the data buffer address
	// in TD will be incremented by Host Controller with the number of bytes read
	unsigned uiDataBuffer = KERNEL_VIRTUAL_ADDRESS(pTD1->uiBufferPointer[ 0 ]) ;

	EHCIQTransferDesc* pTD2 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTD1->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD2) ;
	pTD2->uiNextpTDPointer = 1 ;
	pTD2->uiAltpTDPointer = 1 ;
	pTD2->uipTDToken = (3 << 10) | (1 << 7) ;

	EHCITransaction aTransaction(pControlQH, pTDStart);

	if(!aTransaction.PollWait())
	{
		printf("\n Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pControlQH, pTDStart) ;
		pController->DisplayStats() ;
    aTransaction.Clear();
		return false ;
	}
	
	*bLUN = *((char*)uiDataBuffer) ;

  aTransaction.Clear();

	return true ;
}

static bool EHCIController_CommandReset(USBDevice* pUSBDevice)
{
	EHCIDevice* pDevice = (EHCIDevice*)(pUSBDevice->pRawDevice) ;
	EHCIController* pController = pDevice->pController ;
	EHCIQueueHead* pControlQH = pDevice->pControlQH ;

	// Setup TDs
	EHCIQTransferDesc* pTDStart = EHCIDataHandler_CreateAsyncQTransferDesc() ;

	pTDStart->uiAltpTDPointer = 1 ;
	pTDStart->uipTDToken = (8 << 16) | (3 << 10) | (2 << 8) | (1 << 7) ;

	if(EHCIController_SetupAllocBuffer(pTDStart, sizeof(USBDevRequest)) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		return false ;
	}

	USBDevRequest* pDevRequest = (USBDevRequest*)(KERNEL_VIRTUAL_ADDRESS(pTDStart->uiBufferPointer[ 0 ])) ;
	pDevRequest->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE ;
	pDevRequest->bRequest = 0xFF ;
	pDevRequest->usWValue = 0 ;
	pDevRequest->usWIndex = pUSBDevice->bInterfaceNumber ;
	pDevRequest->usWLength = 0 ;
	
	EHCIQTransferDesc* pTD1 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTDStart->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD1) ;
	pTD1->uiNextpTDPointer = 1 ;
	pTD1->uiAltpTDPointer = 1 ;
	pTD1->uipTDToken = (1 << 31) | (3 << 10) | (1 << 8) | (1 << 7) ;

	EHCITransaction aTransaction(pControlQH, pTDStart);
	
	if(!aTransaction.PollWait())
	{
		printf("\n Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pControlQH, pTDStart) ;
		pController->DisplayStats() ;
    aTransaction.Clear();
		return false ;
	}

  aTransaction.Clear();

	return true ;
}

static bool EHCIController_ClearHaltEndPoint(USBulkDisk* pDisk, bool bIn)
{
	EHCIDevice* pDevice = (EHCIDevice*)(pDisk->pUSBDevice->pRawDevice) ;
	EHCIController* pController = pDevice->pController ;
	EHCIQueueHead* pControlQH = pDevice->pControlQH ;

	unsigned uiEndPoint = (bIn) ? pDisk->uiEndPointIn : pDisk->uiEndPointOut ;

	// Setup TDs
	EHCIQTransferDesc* pTDStart = EHCIDataHandler_CreateAsyncQTransferDesc() ;

	pTDStart->uiAltpTDPointer = 1 ;
	pTDStart->uipTDToken = (8 << 16) | (3 << 10) | (2 << 8) | (1 << 7) ;

	if(EHCIController_SetupAllocBuffer(pTDStart, sizeof(USBDevRequest)) != EHCIController_SUCCESS)
	{
		DMM_DeAllocateForKernel((unsigned)pTDStart) ;
		return false ;
	}

	USBDevRequest* pDevRequest = (USBDevRequest*)(KERNEL_VIRTUAL_ADDRESS(pTDStart->uiBufferPointer[ 0 ])) ;
	pDevRequest->bRequestType = USB_RECIP_ENDPOINT ;
	pDevRequest->bRequest = 1 ;
	pDevRequest->usWValue = 0 ;
	pDevRequest->usWIndex = (uiEndPoint & 0xF) | ((bIn) ? 0x80 : 0x00) ;
	pDevRequest->usWLength = 0 ;
	
	EHCIQTransferDesc* pTD1 = EHCIDataHandler_CreateAsyncQTransferDesc() ;
	pTDStart->uiNextpTDPointer = KERNEL_REAL_ADDRESS(pTD1) ;
	pTD1->uiNextpTDPointer = 1 ;
	pTD1->uiAltpTDPointer = 1 ;
	pTD1->uipTDToken = (1 << 31) | (3 << 10) | (1 << 8) | (1 << 7) ;

	EHCITransaction aTransaction(pControlQH, pTDStart);
	
	if(!aTransaction.PollWait())
	{
		printf("\n Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pControlQH, pTDStart) ;
		pController->DisplayStats() ;
    aTransaction.Clear();
		return false ;
	}

  aTransaction.Clear();

	return true ;
}

extern void _UpdateReadStat(unsigned, bool) ;
static bool EHCIController_BulkInTransfer(USBulkDisk* pDisk, void* pDataBuf, unsigned uiLen)
{
  return EHCIManager::Instance().BulkInTransfer(pDisk, pDataBuf, uiLen);
}

static bool EHCIController_BulkOutTransfer(USBulkDisk* pDisk, void* pDataBuf, unsigned uiLen)
{
  return EHCIManager::Instance().BulkOutTransfer(pDisk, pDataBuf, uiLen);
}

EHCIController::EHCIController(PCIEntry* pPCIEntry, int iMemMapIndex) 
  : _pPCIEntry(pPCIEntry), _pCapRegs(nullptr), _pOpRegs(nullptr), _pAsyncReclaimQueueHead(nullptr)
{
	if(!pPCIEntry->BusEntity.NonBridge.bInterruptLine)
    throw upan::exception(XLOC, "EHCI device with no IRQ. Check BIOS/PCI settings!");

	unsigned uiIOAddr = pPCIEntry->BusEntity.NonBridge.uiBaseAddress0;
	printf("\n PCI Base Addr: %x", uiIOAddr);

	uiIOAddr = uiIOAddr & PCI_ADDRESS_MEMORY_32_MASK;
	unsigned uiIOSize = pPCIEntry->GetPCIMemSize(0);
	printf("\n Raw MMIO Base Addr: %x, IO Size: %d", uiIOAddr, uiIOSize);

	if(uiIOSize > PAGE_SIZE)
    throw upan::exception(XLOC, "EHCI IO Size greater then 1 Page (4096b) not supported currently !");

	if((uiIOAddr % PAGE_SIZE) + uiIOSize > PAGE_SIZE)
    throw upan::exception(XLOC, "EHCI MMIO area is spanning across PAGE boundary. This is not supported in MOS!!");

	// Currently an IO Size of not more than a PAGE_SIZE is supported
	// And also the IO Addr is expected not to span a PAGE Boundary.
	// Now, the mapping EHCI_MMIO_BASE_ADDR is choosen in such way that
	// EHCI_MMIO_BASE_ADDR and EHCI_MMIO_BASE_ADDR + 32 * PAGE_SIZE fall
	// within the same PTE Entry
	// Further, mapping is necessary because the IOAddr can be any virtual address
	// within 4 GB space potentially being an address outside the RAM size
	// i.e, PDE/PTE limit
	unsigned uiPDEAddress = MEM_PDBR ;
	unsigned uiMapAddress = EHCI_MMIO_BASE_ADDR + iMemMapIndex * PAGE_SIZE;
	unsigned uiPDEIndex = ((uiMapAddress >> 22) & 0x3FF) ;
	unsigned uiPTEIndex = ((uiMapAddress >> 12) & 0x3FF) ;

	unsigned uiPTEAddress = (((unsigned*)(uiPDEAddress - GLOBAL_DATA_SEGMENT_BASE))[uiPDEIndex]) & 0xFFFFF000 ;
	// This page is a Read Only area for user process. 0x5 => 101 => User Domain, Read Only, Present Bit
	((unsigned*)(uiPTEAddress - GLOBAL_DATA_SEGMENT_BASE))[uiPTEIndex] = (uiIOAddr & 0xFFFFF000) | 0x5 ;
	Mem_FlushTLB();
	
	if(MemManager::Instance().MarkPageAsAllocated(uiIOAddr / PAGE_SIZE) != Success)
	{
	}

	uiMapAddress = uiMapAddress + (uiIOAddr % PAGE_SIZE) - GLOBAL_DATA_SEGMENT_BASE ;
	_pCapRegs = (EHCICapRegisters*)uiMapAddress;
	byte bCapLen = _pCapRegs->bCapLength ;
	_pOpRegs = (EHCIOpRegisters*)(uiMapAddress + bCapLen) ;

	unsigned uiHCSParams = _pCapRegs->uiHCSParams ;
	unsigned uiHCCParams = _pCapRegs->uiHCCParams ;
	unsigned uiConfigFlag = _pOpRegs->uiConfigFlag ;

	printf("\n HCSPARAMS: %x\n HCCPARAMS: %x\n CAP Len: %x\n CF Bit: %d", uiHCSParams, uiHCCParams, bCapLen, uiConfigFlag) ;
	printf("\n Bus: %d, Device: %d, Function: %d", pPCIEntry->uiBusNumber, pPCIEntry->uiDeviceNumber, pPCIEntry->uiFunction) ;
	printf("\n Enabling Bus Master...") ;
	/* Enable busmaster */
	unsigned short usCommand ;
	pPCIEntry->ReadPCIConfig(PCI_COMMAND, 2, &usCommand);
	printf("\n Current value of PCI_COMMAND: %x", usCommand) ;
	pPCIEntry->WritePCIConfig(PCI_COMMAND, 2, usCommand | PCI_COMMAND_IO | PCI_COMMAND_MASTER) ;
	printf("\n After Bus Master Enable, value of PCI_COMMAND: %x", usCommand) ;
}

byte EHCIController::Probe()
{
  RETURN_X_IF_NOT(PerformBiosToOSHandoff(), EHCIController_SUCCESS, EHCIController_FAILURE);

	_pOpRegs->uiCtrlDSSegment = 0 ;

	SetupInterrupts();

	RETURN_X_IF_NOT(SetupPeriodicFrameList(), EHCIController_SUCCESS, EHCIController_FAILURE) ;
	RETURN_X_IF_NOT(SetupAsyncList(), EHCIController_SUCCESS, EHCIController_FAILURE) ;

	SetSchedEnable(PERIODIC_SCHEDULE_ENABLE, false) ;
	SetSchedEnable(ASYNC_SCHEDULE_ENABLE, false) ;
	SetFrameListSize() ;

	Start() ;

	ProcessManager::Instance().Sleep(100) ;

  RETURN_X_IF_NOT(SetConfigFlag(true), EHCIController_SUCCESS, EHCIController_FAILURE);

	if(!CheckHCActive())
	{
		printf("\n Failed to start EHCI Host Controller") ;
		DisplayStats() ;
		return EHCIController_FAILURE ;
	}

	SetupPorts() ;

	RETURN_X_IF_NOT(StartAsyncSchedule(), true, EHCIController_FAILURE) ;

	byte bStatus ;
	EHCIQueueHead* pControlQH = CreateDeviceQueueHead(64, 0, 0) ;

	int devAddr = USBController_GetNextDevNum() ;
	if(devAddr <= 0)
	{
		printf("\n Invalid Next Dev Addr: %d", devAddr) ;
		return EHCIController_FAILURE ;
	}

	EHCIDevice* pEHCIDevice = (EHCIDevice*)DMM_AllocateForKernel(sizeof(EHCIDevice)) ;

	pEHCIDevice->pController = this ;
	pEHCIDevice->pControlQH = pControlQH ;

	RETURN_MSG_IF_NOT(bStatus, EHCIController_SetAddress(pEHCIDevice, devAddr), EHCIController_SUCCESS, "SetAddress Failed") ;

	USBStandardDeviceDesc devDesc ;
	RETURN_MSG_IF_NOT(bStatus, EHCIController_GetDeviceDescriptor(pEHCIDevice, &devDesc), EHCIController_SUCCESS, "GetDevDesc Failed") ;
	USBDataHandler_DisplayDevDesc(&devDesc) ;

	char bConfigValue = 0 ;
	RETURN_MSG_IF_NOT(bStatus, EHCIController_GetConfigValue(pEHCIDevice, &bConfigValue), EHCIController_SUCCESS, "GetConfigVal Failed") ;
	printf("\n\n ConfifValue: %d", bConfigValue) ;

	RETURN_MSG_IF_NOT(bStatus, EHCIController_CheckConfiguration(pEHCIDevice, &bConfigValue, devDesc.bNumConfigs), 
						EHCIController_SUCCESS, "CheckConfig Failed") ;

	USBStandardConfigDesc* pConfigDesc = NULL ;
	RETURN_MSG_IF_NOT(bStatus, EHCIController_GetConfigDescriptor(pEHCIDevice, devDesc.bNumConfigs, &pConfigDesc), 
						EHCIController_SUCCESS, "GeConfigDesc Failed") ;

	USBStringDescZero* pStrDescZero = NULL ;
	RETURN_MSG_IF_NOT(bStatus, EHCIController_GetStringDescriptorZero(pEHCIDevice, &pStrDescZero), EHCIController_SUCCESS, "GetStringDescZero Failed") ;

	USBDevice* pUSBDevice = (USBDevice*)DMM_AllocateForKernel(sizeof(USBDevice)) ;

	pUSBDevice->pRawDevice = pEHCIDevice ;
	pUSBDevice->devAddr = devAddr ;
	USBDataHandler_CopyDevDesc(&(pUSBDevice->deviceDesc), &devDesc, sizeof(USBStandardDeviceDesc)) ;
	pUSBDevice->pArrConfigDesc = pConfigDesc ;
	pUSBDevice->pStrDescZero = pStrDescZero ;

	USBDataHandler_SetLangId(pUSBDevice) ;

	pUSBDevice->iConfigIndex = 0 ;

	for(int i = 0; i < devDesc.bNumConfigs; i++)
	{
		if(pConfigDesc[ i ].bConfigurationValue == bConfigValue)
		{
			pUSBDevice->iConfigIndex = i ;
			break ;
		}
	}
	
	EHCIController_GetDeviceStringDetails(pUSBDevice) ;

	pUSBDevice->GetMaxLun = EHCIController_GetMaxLun ;
	pUSBDevice->CommandReset = EHCIController_CommandReset ;
	pUSBDevice->ClearHaltEndPoint = EHCIController_ClearHaltEndPoint ;

	pUSBDevice->BulkRead = EHCIController_BulkInTransfer ;
	pUSBDevice->BulkWrite = EHCIController_BulkOutTransfer ;

	USBDriver* pDriver = USBController_FindDriver(pUSBDevice) ;

	if(pDriver)
		printf("\n'%s' driver found for the USB Device\n", pDriver->szName) ;
	else
		printf("\nNo Driver found for this USB device\n") ;

	return EHCIController_SUCCESS ;
}

byte EHCIController::PerformBiosToOSHandoff()
{
	byte bEECPOffSet = (_pCapRegs->uiHCCParams >> 8) & 0xFF ;

	if(bEECPOffSet)
	{
		printf("\n Trying to perform complete handoff of EHCI Controller from BIOS to OS...") ;
		printf("\n EECP Offset: %x", bEECPOffSet) ;

		unsigned uiLegSup ;
		_pPCIEntry->ReadPCIConfig(bEECPOffSet, 4, &uiLegSup) ;

		printf("\n USB EHCI LEGSUP: %x", uiLegSup) ;
		if((uiLegSup & (1 << 24)) == 0x1)
		{
			printf("\n EHCI Controller is already owned by OS. No need for Handoff") ;
			return EHCIController_SUCCESS ;
		}

		uiLegSup = uiLegSup | ( 1 << 24 ) ;

		_pPCIEntry->WritePCIConfig(bEECPOffSet, 4, uiLegSup) ;
		ProcessManager::Instance().Sleep(500) ;
		_pPCIEntry->ReadPCIConfig(bEECPOffSet, 4, &uiLegSup) ;

		printf("\n New USB EHCI LEGSUP: %x", uiLegSup) ;
		if((uiLegSup & (1 << 24)) == 0x0)
		{
			printf("\n BIOS to OS Handoff failed") ;
			return EHCIController_FAILURE ;
		}
	}
	else
	{
		printf("\n EHCI: System does not support Extended Capabilities. Cannot perform Bios Handoff") ;
		return EHCIController_FAILURE ;
	}

	return EHCIController_SUCCESS ;
}



EHCIManager::EHCIManager() :
  _seqDevAddr(0),
  _bFirstBulkRead(true),
  _bFirstBulkWrite(true)
{
	byte bControllerFound = false ;

	for(auto pPCIEntry : PCIBusHandler::Instance().PCIEntries())
	{
		if(pPCIEntry->bHeaderType & PCI_HEADER_BRIDGE)
			continue ;

		if(pPCIEntry->bInterface == 32 && 
			pPCIEntry->bClassCode == PCI_SERIAL_BUS && 
			pPCIEntry->bSubClass == PCI_USB)
		{
      printf("\n Interface = %u, Class = %u, SubClass = %u", pPCIEntry->bInterface, pPCIEntry->bClassCode, pPCIEntry->bSubClass);
      int memMapIndex = _controllers.size();
      _controllers.push_back(new EHCIController(pPCIEntry, memMapIndex));
			bControllerFound = true ;
		}
	}
	
	if(bControllerFound)
		KC::MDisplay().LoadMessage("USB EHCI Controller Found", Success) ;
	else
		KC::MDisplay().LoadMessage("No USB EHCI Controller Found", Failure) ;
}

void EHCIManager::ProbeDevice()
{
  for(auto c : _controllers)
    c->Probe();
}

byte EHCIManager::RouteToCompanionController()
{
  for(auto c : _controllers)
	{
		if(c->PerformBiosToOSHandoff() != EHCIController_SUCCESS)
			continue ;

		if(c->SetConfigFlag(false) != EHCIController_SUCCESS)
			continue ;
	}

	return EHCIController_SUCCESS ;
}

bool EHCIManager::BulkInTransfer(USBulkDisk* pDisk, void* pDataBuf, unsigned uiLen)
{
	_UpdateReadStat(uiLen, true) ;
	USBDevice* pUSBDevice = (USBDevice*)(pDisk->pUSBDevice) ;
	EHCIDevice* pDevice = (EHCIDevice*)(pUSBDevice->pRawDevice) ;
	EHCIController* pController = pDevice->pController ;

	if(uiLen == 0)
		return true ;

	unsigned uiMaxLen = pDisk->usInMaxPacketSize * MAX_EHCI_TD_PER_BULK_RW ;
	if(uiLen > uiMaxLen)
	{
		printf("\n Max Bulk Transfer per Frame is %d bytes", uiMaxLen) ;
		return false ;
	}

	int iNoOfTDs = uiLen / EHCI_MAX_BYTES_PER_TD ;
	iNoOfTDs += ((uiLen % EHCI_MAX_BYTES_PER_TD) != 0) ;

	if(iNoOfTDs > MAX_EHCI_TD_PER_BULK_RW)
	{
		printf("\n No. of TDs per Bulk Read/Wrtie exceeded limit %d !!", MAX_EHCI_TD_PER_BULK_RW) ;
		return false ;
	}

  const unsigned uiMaxPacketSize = pDisk->usInMaxPacketSize;

	if(_bFirstBulkRead)
	{
		_bFirstBulkRead = false ;

		for(int i = 0; i < MAX_EHCI_TD_PER_BULK_RW; i++)
			_ppBulkReadTDs[ i ] = EHCIDataHandler_CreateAsyncQTransferDesc() ; 

		pDevice->pBulkInEndPt = pController->CreateDeviceQueueHead(uiMaxPacketSize, pDisk->uiEndPointIn, pUSBDevice->devAddr) ;
	}
	else
	{
		int i ;
		for(i = 0; i < MAX_EHCI_TD_PER_BULK_RW; i++)
			memset((void*)(_ppBulkReadTDs[i]), 0, sizeof(EHCIQTransferDesc)) ;

		EHCIDataHandler_CleanAysncQueueHead(pDevice->pBulkInEndPt) ;
	}

	int iIndex ;

	unsigned uiYetToRead = uiLen ;
	unsigned uiCurReadLen ;
	for(iIndex = 0; iIndex < iNoOfTDs; iIndex++)
	{
		uiCurReadLen = (uiYetToRead > EHCI_MAX_BYTES_PER_TD) ? EHCI_MAX_BYTES_PER_TD : uiYetToRead ;
		uiYetToRead -= uiCurReadLen ;

		EHCIQTransferDesc* pTD = _ppBulkReadTDs[ iIndex ] ;

		pTD->uiAltpTDPointer = 1 ;
		pTD->uipTDToken = (pDisk->bEndPointInToggle << 31) | (uiCurReadLen << 16) | (3 << 10) | (1 << 8) | (1 << 7) ;
		unsigned toggle = uiCurReadLen / uiMaxPacketSize ;
		if(uiCurReadLen % uiMaxPacketSize)
			toggle++ ;
		if(toggle % 2)
			pDisk->bEndPointInToggle ^= 1 ;

		unsigned uiBufAddr = (unsigned)(pDisk->pRawAlignedBuffer + (iIndex * EHCI_MAX_BYTES_PER_TD)) ;
		if(EHCIController_SetupBuffer(pTD, uiBufAddr, uiCurReadLen)	!= EHCIController_SUCCESS)
		{
			printf("\n EHCI Transfer Buffer setup failed") ;
			return false ;
		}

		if(iIndex > 0)
			_ppBulkReadTDs[ iIndex - 1 ]->uiNextpTDPointer = KERNEL_REAL_ADDRESS((unsigned)pTD) ;
	}

	_ppBulkReadTDs[ iIndex - 1 ]->uiNextpTDPointer = 1 ;

	EHCITransaction aTransaction(pDevice->pBulkInEndPt, _ppBulkReadTDs[ 0 ]);

	if(!aTransaction.PollWait())
	{
		printf("\n Bulk Read Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pDevice->pBulkInEndPt, _ppBulkReadTDs[0]) ;
		pController->DisplayStats() ;
		return false ;
	}

	EHCIDataHandler_CleanAysncQueueHead(pDevice->pBulkInEndPt) ;

	memcpy(pDataBuf, pDisk->pRawAlignedBuffer, uiLen) ;

	_UpdateReadStat(uiLen, false) ;
	return true ;
}

bool EHCIManager::BulkOutTransfer(USBulkDisk* pDisk, void* pDataBuf, unsigned uiLen)
{
	USBDevice* pUSBDevice = (USBDevice*)(pDisk->pUSBDevice) ;
	EHCIDevice* pDevice = (EHCIDevice*)(pUSBDevice->pRawDevice) ;
	EHCIController* pController = pDevice->pController ;

	if(uiLen == 0)
		return true ;

	unsigned uiMaxLen = pDisk->usOutMaxPacketSize * MAX_EHCI_TD_PER_BULK_RW ;
	if(uiLen > uiMaxLen)
	{
		printf("\n Max Bulk Transfer per Frame is %d bytes", uiMaxLen) ;
		return false ;
	}

	int iNoOfTDs = uiLen / EHCI_MAX_BYTES_PER_TD ;
	iNoOfTDs += ((uiLen % EHCI_MAX_BYTES_PER_TD) != 0) ;

	if(iNoOfTDs > MAX_EHCI_TD_PER_BULK_RW)
	{
		printf("\n No. of TDs per Bulk Read/Wrtie exceeded limit %d !!", MAX_EHCI_TD_PER_BULK_RW) ;
		return false ;
	}

  const unsigned uiMaxPacketSize = pDisk->usOutMaxPacketSize;

	if(_bFirstBulkWrite)
	{
		_bFirstBulkWrite = false ;

		for(int i = 0; i < MAX_EHCI_TD_PER_BULK_RW; i++)
			_ppBulkWriteTDs[ i ] = EHCIDataHandler_CreateAsyncQTransferDesc() ; 

		pDevice->pBulkOutEndPt = pController->CreateDeviceQueueHead(uiMaxPacketSize, pDisk->uiEndPointOut, pUSBDevice->devAddr) ;
	}
	else
	{
		int i ;
		for(i = 0; i < MAX_EHCI_TD_PER_BULK_RW; i++)
			memset((void*)(_ppBulkWriteTDs[i]), 0, sizeof(EHCIQTransferDesc)) ;

		EHCIDataHandler_CleanAysncQueueHead(pDevice->pBulkOutEndPt) ;
	}

	int iIndex ;

	memcpy(pDisk->pRawAlignedBuffer, pDataBuf, uiLen) ;

	unsigned uiYetToWrite = uiLen ;
	unsigned uiCurWriteLen ;

	for(iIndex = 0; iIndex < iNoOfTDs; iIndex++)
	{
		uiCurWriteLen = (uiYetToWrite > EHCI_MAX_BYTES_PER_TD) ? EHCI_MAX_BYTES_PER_TD : uiYetToWrite ;
		uiYetToWrite -= uiCurWriteLen ;

		EHCIQTransferDesc* pTD = _ppBulkWriteTDs[ iIndex ] ;

		pTD->uiAltpTDPointer = 1 ;
		pTD->uipTDToken = (pDisk->bEndPointOutToggle << 31) | (uiCurWriteLen << 16) | (3 << 10) | (1 << 7) ;
		unsigned toggle = uiCurWriteLen / uiMaxPacketSize ;
		if(uiCurWriteLen % uiMaxPacketSize)
			toggle++ ;
		if(toggle % 2)
			pDisk->bEndPointOutToggle ^= 1 ;

		unsigned uiBufAddr = (unsigned)(pDisk->pRawAlignedBuffer + (iIndex * EHCI_MAX_BYTES_PER_TD)) ;
		if(EHCIController_SetupBuffer(pTD, uiBufAddr, uiCurWriteLen) != EHCIController_SUCCESS)
		{
			printf("\n EHCI Transfer Buffer setup failed") ;
			return false ;
		}

		if(iIndex > 0)
			_ppBulkWriteTDs[ iIndex - 1 ]->uiNextpTDPointer = KERNEL_REAL_ADDRESS((unsigned)pTD) ;
	}

	_ppBulkWriteTDs[ iIndex - 1 ]->uiNextpTDPointer = 1 ;
  
	EHCITransaction aTransaction(pDevice->pBulkOutEndPt, _ppBulkWriteTDs[ 0 ]);

	if(!aTransaction.PollWait())
	{
		printf("\n Bulk Write Transaction Failed: ") ;
		EHCIController_DisplayTransactionState(pDevice->pBulkOutEndPt, _ppBulkWriteTDs[0]) ;
		pController->DisplayStats() ;
		return false ;
	}
  
	EHCIDataHandler_CleanAysncQueueHead(pDevice->pBulkOutEndPt) ;
  return true;
}
