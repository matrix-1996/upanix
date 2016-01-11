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
# include <stdio.h>
# include <UpanixMain.h>
# include <MemUtil.h>

# include <FileSystem.h>
# include <ProcessManager.h>
# include <SysCall.h>
# include <MemManager.h>
# include <PCIBusHandler.h>
# include <DMM.h>
# include <Directory.h>
# include <Floppy.h>
# include <MemUtil.h>
# include <StringUtil.h>
# include <FSCommand.h>
# include <KernelService.h>
# include <ATADeviceController.h>
# include <ProcFileManager.h>
# include <SessionManager.h>
# include <UserManager.h>
# include <PartitionManager.h>
# include <RTC.h>
# include <MultiBoot.h>
# include <MountManager.h>
# include <DisplayManager.h>
# include <UHCIController.h>
# include <EHCIManager.h>
# include <exception.h>

/**** Global Variable declaration/definition *****/
byte KERNEL_MODE ;
byte SPECIAL_TASK ;
int debug_point ;
/***********************************************/

static int UpanixMain_KernelPID ;

void DummyPrint()
{
  static int i = 1;
  while(true)
  {
    ProcessManager::Instance().Sleep(1000);
    printf("\n COUNTER: %d", ++i);
  }
}

void UpanixMain_KernelProcess()
{
	int pid ;

	//MountManager_MountDrives() ;
	
	UpanixMain_KernelPID = ProcessManager::GetCurrentProcessID() ;

	KC::MKernelService().Spawn() ;
	KC::MKernelService().Spawn() ;

//	ProcessManager::Instance().CreateKernelImage((unsigned)&DummyPrint, ProcessManager::GetCurrentProcessID(), true, NULL, NULL, &pid, "dummy") ;
	ProcessManager::Instance().CreateKernelImage((unsigned)&Console_StartUpanixConsole, ProcessManager::GetCurrentProcessID(), true, NULL, NULL, &pid, "console") ;
//	ProcessManager_CreateKernelImage((unsigned)&FloatProcess, ProcessManager::GetCurrentProcessID(), false, NULL, NULL, &pid, "float") ;
//	ProcessManager_CreateKernelImage((unsigned)&FloatProcess, ProcessManager::GetCurrentProcessID(), false, NULL, NULL, &pid, "float1") ;
//	ProcessManager_CreateKernelImage((unsigned)&SessionManager_StartSession, NO_PROCESS_ID, true, NULL, NULL, &pid, "sesman") ;
//	SessionManager_SetSessionIDMap(SessionManager_KeyToSessionIDMap(Keyboard_F1), pid) ;

	ProcessManager::Instance().WaitOnChild(pid) ;
	ProcessManager_EXIT() ;
}

extern "C" void _cxx_global_init();

void TestThrowStr()
{
	throw "String Exception";
}

void TestThrowInt()
{
	throw 100;
}

class Exception
{
	public:
	Exception(const char* x) : _val(x) {}
	const char* _val;
};

void TestThrowObj()
{
	throw Exception("Exception Class");
}


void TestException()
{
	try { TestThrowStr(); } catch(const char* ex) { printf("\nCaught Exception: %s", ex); }
	try { TestThrowInt(); } catch(int ex) { printf("\nCaught Exception: %d", ex); }
	try { TestThrowObj(); } catch(const Exception& ex) { printf("\nCaught Exception: %s", ex._val); }

	try {
	try { TestThrowStr(); } catch(const char* ex) { printf("\nRethrow Caught Exception: %s", ex); throw; }
	} catch(const char* ex) { printf("\nCaught Rethrown Exception: %s", ex); }

	try {
	try { TestThrowStr(); } catch(const char* ex) { printf("\nRethrow Caught Exception: %s", ex); throw 100; }
	} catch(const char* ex) { printf("\nCaught Rethrown Exception: %s", ex); }
	catch(...) { printf("\nCaught Rethrown unknown Exception"); }
}

void Initialize()
{
	debug_point = 0 ;

	KERNEL_MODE = true ;
	SPECIAL_TASK = false ;

	DisplayManager::Initialize();
	KC::MDisplay().Message("\n****    Welcome To Upanix   ****\n", Display::Attribute(' ')) ;
	ProcFileManager_InitForKernel();
	MultiBoot::Instance();

  const framebuffer_info_t* f = MultiBoot::Instance().VideoFrameBufferInfo();
//  //writing to video mem before page table setup as well work
//  if(f)
//  {
//    for(unsigned y = 0; y < f->framebuffer_height; ++y)
//    {
//      for(unsigned x = 0; x < f->framebuffer_width; ++x)
//      {
//        unsigned* ptr = (unsigned*)(f->framebuffer_addr + y * f->framebuffer_pitch + x * 4);
//        *ptr = 0xFFAA0000;
//      }
//    }
//  }

	MemManager::Instance();

	//defined in osutils/crti.s - this is C++ init to call global objects' constructor
	_cxx_global_init();
//	TestException(); while(1);
  try
  {
//    if(f)
//    {
//      const unsigned framebuffer_addr = f->framebuffer_addr;
//      unsigned uiPDEAddress = MEM_PDBR;
//      unsigned noOfPages = ((f->framebuffer_height * f->framebuffer_width * 4) / PAGE_SIZE) + 1;
//      for(unsigned i = 0; i < noOfPages; ++i)
//      {
//        unsigned addr = framebuffer_addr + PAGE_SIZE * i;
//        unsigned uiPDEIndex = ((framebuffer_addr >> 22) & 0x3FF);
//        unsigned uiPTEAddress = (((unsigned*)(uiPDEAddress - GLOBAL_DATA_SEGMENT_BASE))[uiPDEIndex]) & 0xFFFFF000;
//        unsigned uiPTEIndex = ((addr >> 12) & 0x3FF);
//        // This page is a Read Only area for user process. 0x5 => 101 => User Domain, Read Only, Present Bit
//        ((unsigned*)(uiPTEAddress - GLOBAL_DATA_SEGMENT_BASE))[uiPTEIndex] = (addr & 0xFFFFF000) | 0x5;
//        MemManager::Instance().MarkPageAsAllocated(addr / PAGE_SIZE);
//      }
//      Mem_FlushTLB();
//
//      for(unsigned y = 0; y < f->framebuffer_height; ++y)
//      {
//        for(unsigned x = 0; x < f->framebuffer_width; ++x)
//        {
//          unsigned* ptr = (unsigned*)(framebuffer_addr + y * f->framebuffer_pitch + x * 4);
//          *ptr = 0xFFAAAA00;
//        }
//      }
//    }

    IDT::Instance();
    PIC::Instance();
    DMA_Initialize();
    PIT_Initialize();

    ProcessManager::Instance();
    SysCall_Initialize() ;

    KC::MKernelService() ;

    PIC::Instance().EnableInterrupt(PIC::Instance().TIMER_IRQ) ;

  /* Start - Peripheral Device Initialization */
  //TODO: An Abstract Bus Handler which should internally take care of different
  //types of bus like ISA, PCI etc... 
    PCIBusHandler::Instance().Initialize();

    DiskDriveManager::Instance();

    Keyboard_Initialize() ;

    //Floppy_Initialize() ;
    //ATADeviceController_Initialize() ;

    //MountManager_Initialize() ;

  /*End - Peripheral Device Initialization */

    RTC::Initialize() ;
    //while(1) ;
    
    //USB
    USBController::Instance();
    EHCIManager::Instance();
    UHCIManager::Instance();
    USBMassBulkStorageDisk_Initialize() ;

    SessionManager_Initialize() ;

    Console_Initialize() ;
  }
  catch(const upan::exception& ex)
  {
    printf("%s\n", ex.Error().c_str());
    printf("KERNEL PANIC!\n");
    while(1);
  }
  catch(...)
  {
    printf("\n Unknown error!! KERNEL PANIC!\n");
    while(1);
  }
}

Mutex& UpanixMain_GetDMMMutex()
{
	static Mutex mDMMMutex ;
	return mDMMMutex ;
}

byte* GetArea()
{
	static byte area[0x1000] ;
	return area ;
}

extern void VM86_Test() ;
void UpanixMain()
{
	byte* bios = (byte*)(0 - GLOBAL_DATA_SEGMENT_BASE) ;

	for(int i = 0; i < 0x500; i++) 
		GetArea()[i] = bios[i] ;

	Initialize() ;

	int pid ;
	ProcessManager::Instance().CreateKernelImage((unsigned)&UpanixMain_KernelProcess, NO_PROCESS_ID, true, NULL, NULL, &pid, "kerparent") ;
//	ProcessManager_CreateKernelImage((unsigned)&Console_StartMOSConsole, NO_PROCESS_ID, true, NULL, NULL, &pid) ;

	ProcessManager::Instance().StartScheduler();
	while(1) ;
}

bool UpanixMain_IsKernelDebugOn()
{
	const char* szVal = getenv("UPANIX_KDEBUG") ;
	if(szVal != NULL)
		if(strcmp(szVal, "1") == 0)
			return true ;

	return false ;
}

bool UpanixMain_isCoProcPresent()
{
	if(CO_PROC_FPU_TYPE == NO_CO_PROC)
		return false ;
	return true ;
}

int UpanixMain_KernelProcessID()
{
	return UpanixMain_KernelPID ;
}
