#include "F28x_Project.h"
#include <string.h>
#include "flash_programming_dcsm_c28.h"
#include "F021_F2837xD_C28x.h"
//#include "Firmware.h"

//
// Defines
//
#define  WORDS_IN_FLASH_BUFFER    16


#ifdef __TI_COMPILER_VERSION__
#if __TI_COMPILER_VERSION__ >= 15009000
#define ramFuncSection ".TI.ramfunc"
#else
#define ramFuncSection "ramfuncs"
#endif
#endif

//
// Globals
//


uint32 u32Index = 0UL;
uint16 i = 0U;
Fapi_StatusType oReturnCheck;
volatile Fapi_FlashStatusType oFlashStatus;
Fapi_FlashStatusWordType oFlashStatusWord;


char tmpFlag = 1;


// Reload
#pragma DATA_SECTION(firmwareBufferInRam, "FirmwareBufferSection");
uint16 firmwareBufferInRam[20000];
uint16 firmwareBufferInFlash;

#pragma DATA_SECTION(firmwareLengthInRam, "FirmwareLengthSection");
uint16 firmwareLengthInRam;
uint16 firmwareLengthInFlash;

#pragma DATA_SECTION(firmwareSumInRam, "FirmwareSumSection");
uint16 firmwareSumInRam;
uint16 firmwareSumInFlash;

int firmwareCounter = 0;
/// Reload

uint32  *firmwareBuffer32 = (uint32 *)firmwareBufferInRam;

uint32 *memoryPointer = 0;
uint16 tmpFirmwareSum = 0;

//uint16 firmwareLength = 16000;



#define FIRMWARE_BUFFER_ADDRESS_IN_FLASH    0x0A0000
#define FIRMWARE_LENGTH_ADDRESS_IN_FLASH    0x0A7F00
#define FIRMWARE_SUM_ADDRESS_IN_FLASH       0x0A7FA0

#define FIRMWARE_BUFFER_ADDRESS_IN_RAM      0x014000
#define FIRMWARE_LENGTH_ADDRESS_IN_RAM      0x01A000
#define FIRMWARE_SUM_ADDRESS_IN_RAM         0x01B000

#define JUMP_TO_FIRMWARE asm("    JMP 0x0A0000")
#define JUMP_TO_GOLD asm("    JMP 0x0B0000")


char firmwareSumIsCorrect = 0;



//
// Function Prototypes
//
void Example_Error(Fapi_StatusType status);
void Example_Done(void);
void Example_CallFlashAPI(void);
void ExampleEraseProgramSector(void);

void InitFlashApi(void);
void CloseFlashApi(void);

void ProgramSector(uint32 sectorStartAddress, uint16* firmwareBuffer, uint16 firmwareBufferLength);
void EraseSector(uint32 sectorStartAddress, uint32 sectorLength);
void VerifySector(uint32 sectorStartAddress, uint16* firmwareBuffer, uint16 firmwareBufferLength);

void LoadFirmware(void);

void CheckSum();
int CheckFirmwareInRam();
int CheckFirmwareInFlash();


//
// TODO Main
//
void main(void)
{
    InitSysCtrl();
    DINT;
    InitPieCtrl();

    IER = 0x0000;
    IFR = 0x0000;

    InitPieVectTable();

    InitFlashApi();

    if (CheckFirmwareInRam())
    {
        LoadFirmware();
        while(1);
    }
    else
    {
        if (CheckFirmwareInFlash())
            JUMP_TO_FIRMWARE;
        else
            JUMP_TO_GOLD;
    }
}

//
// Example_CallFlashAPI - This function will interface to the flash API.
//                        Flash API functions used in this function are
//                        executed from RAM.
//
#pragma CODE_SECTION(Example_CallFlashAPI, ramFuncSection);
void Example_CallFlashAPI(void)
{}

//
// Example_Error - For this example, if an error is found just stop here
//
#pragma CODE_SECTION(Example_Error,ramFuncSection);
void Example_Error(Fapi_StatusType status)
{
    //
    // Error code will be in the status parameter.
    // ESTOP0.
    //
    __asm("    ESTOP0");
}

//
// Example_Done - For this example, once we are done just stop here
//
#pragma CODE_SECTION(Example_Error,ramFuncSection);
void Example_Done(void)
{
    //
    // ESTOP0.
    //
    __asm("    ESTOP0");

    for(;;)
    {
    }
}


//
//TODO Functions
//

int CheckFirmwareInFlash()
{
    int j = 0;
    tmpFirmwareSum = 0;

    memoryPointer = (uint32*)FIRMWARE_LENGTH_ADDRESS_IN_FLASH;
    firmwareLengthInFlash = *memoryPointer;


    memoryPointer = (uint32*)FIRMWARE_SUM_ADDRESS_IN_FLASH;
    firmwareSumInFlash = *memoryPointer;

    memoryPointer = (uint32*)FIRMWARE_BUFFER_ADDRESS_IN_FLASH;
    firmwareBufferInFlash = *memoryPointer;

    if ((firmwareLengthInFlash == 0xFFFF) || (firmwareSumInFlash == 0xFFFF) || (firmwareBufferInFlash == 0xFFFF))
        return 0;
    else
    {
        for (j = 0; j < firmwareLengthInFlash; j++)
        {
            //memoryPointer = (Bzero_SectorH_start + j);

            memoryPointer = (uint32*)FIRMWARE_BUFFER_ADDRESS_IN_FLASH + j;
            firmwareBufferInFlash = *memoryPointer;
            tmpFirmwareSum = (tmpFirmwareSum ^ firmwareBufferInFlash) & 0xFFFF;
        }



        if ((firmwareSumInFlash == tmpFirmwareSum) && (firmwareLengthInFlash != 0xFFFF))
            firmwareSumIsCorrect = 1;


        if (firmwareSumIsCorrect)
        {
            CloseFlashApi();
            return 1;
        }
        else
            return 0;
    }
}

int CheckFirmwareInRam()
{
    int j = 0;

    tmpFirmwareSum = 0;
    if ((firmwareLengthInRam != 0x0000) && (firmwareLengthInRam != 0xFFFF))
    {
        for (j = 0; j < firmwareLengthInRam; j++)
        {
            tmpFirmwareSum = (tmpFirmwareSum ^ firmwareBufferInRam[j]) & 0xFFFF;
        }
        if (tmpFirmwareSum == firmwareSumInRam)
            return 1;
        else
            return 0;
    }
    else
        return 0;
}



void LoadFirmware()
{
    InitFlashApi();

    EraseSector(Bzero_SectorH_start, Bzero_64KSector_u32length);

    ProgramSector(FIRMWARE_BUFFER_ADDRESS_IN_FLASH, firmwareBufferInRam, firmwareLengthInRam);
    ProgramSector(FIRMWARE_LENGTH_ADDRESS_IN_FLASH, &firmwareLengthInRam, 16);
    ProgramSector(FIRMWARE_SUM_ADDRESS_IN_FLASH, &firmwareSumInRam, 16);

    int i = 0;
    for (i = 0; i < firmwareLengthInRam; i++)
        firmwareBufferInRam[i] = 0;
    firmwareLengthInRam = 0;
    firmwareSumInRam = 0;

    CloseFlashApi();
}



void InitFlashApi(void)
{
    EALLOW;
    DcsmCommonRegs.FLSEM.all = 0xA501;
    EDIS;

    //
    // Call flash initialization to setup flash waitstates.
    // This function must reside in RAM.
    //
    InitFlash();

    //
    // Disable ECC.
    //
    EALLOW;
    Flash0EccRegs.ECC_ENABLE.bit.ENABLE = 0x0;

    //
    // This function is required to initialize the Flash API based on system
    // frequency before any other Flash API operation can be performed.
    //
    oReturnCheck = Fapi_initializeAPI(F021_CPU0_BASE_ADDRESS, 120);

    if(oReturnCheck != Fapi_Status_Success)
    {
        //
        // Check Flash API documentation for possible errors.
        //
        Example_Error(oReturnCheck);
    }

    //
    // Fapi_setActiveFlashBank function sets the flash bank and FMC for further
    // flash operations to be performed on the bank.
    //
    oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank0);

    if(oReturnCheck != Fapi_Status_Success)
    {
        //
        // Check Flash API documentation for possible errors.
        //
        Example_Error(oReturnCheck);
    }


}



void CloseFlashApi(void)
{
    //
    // Enable ECC.
    //
    Flash0EccRegs.ECC_ENABLE.bit.ENABLE = 0xA;

    //
    // Release flash semaphore.
    //
    DcsmCommonRegs.FLSEM.all = 0xA500;

    EDIS;
}

#pragma CODE_SECTION(EraseSector, ramFuncSection);
void EraseSector(uint32 sectorStartAddress, uint32 sectorLength)
{
    oReturnCheck = Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, (uint32 *)sectorStartAddress);

    //
    // Wait until FSM is done with erase sector operation.
    //
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady)
    {

    }

    oReturnCheck = Fapi_doBlankCheck((uint32 *)sectorStartAddress,
                                     sectorLength,
                                     &oFlashStatusWord);

    if(oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }
}

#pragma CODE_SECTION(ProgramSector, ramFuncSection);
void ProgramSector(uint32 sectorStartAddress, uint16* firmwareBuffer, uint16 firmwareBufferLength)
{

    for(i=0, u32Index = sectorStartAddress;
            (u32Index < (sectorStartAddress + firmwareBufferLength))
                                                                                   && (oReturnCheck == Fapi_Status_Success); i += 8, u32Index += 8)
    {
        oReturnCheck = Fapi_issueProgrammingCommand((uint32 *)u32Index, firmwareBuffer + i,
                                                    8,
                                                    0,
                                                    0,
                                                    Fapi_AutoEccGeneration);

        //
        // Wait until FSM is done with program operation.
        //
        while(Fapi_checkFsmForReady() == Fapi_Status_FsmBusy)
        {
        }

        if(oReturnCheck != Fapi_Status_Success)
        {
            //
            // Check Flash API documentation for possible errors.
            //
            Example_Error(oReturnCheck);
        }

        //
        // Read FMSTAT register contents to know the status of FSM after
        // program command for any debug.
        //
        oFlashStatus = Fapi_getFsmStatus();
    }
}

#pragma CODE_SECTION(VerifySector, ramFuncSection);
void VerifySector(uint32 sectorStartAddress, uint16* firmwareBuffer, uint16 firmwareBufferLength)
{
    for(i = 0, u32Index = sectorStartAddress;
            (u32Index < (sectorStartAddress + firmwareBufferLength)) && (oReturnCheck == Fapi_Status_Success);
            i += 8, u32Index += 8)
    {
        //
        // Verify the values programmed. The program step itself does
        // verification as it goes. This verify is a second verification that
        // can be done.
        //
        oReturnCheck = Fapi_doVerify((uint32 *)u32Index,
                                     4,
                                     firmwareBuffer32+(i/2),
                                     &oFlashStatusWord);

        if(oReturnCheck != Fapi_Status_Success)
        {
            //
            // Check Flash API documentation for possible errors.
            //
            Example_Error(oReturnCheck);
        }
    }
}