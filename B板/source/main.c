#include "STC15F2K60S2.H"
#include "sys.H"
#include "displayer.H"
#include "Key.h"
#include "beep.h"
#include "uart2.h"

code unsigned long SysClock = 11059200;

#ifdef _displayer_H_
code char decode_table[] =
{
    0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,
    0x00,0x08,0x40,0x01,0x41,0x48,
    0x3f|0x80,0x06|0x80,0x5b|0x80,0x4f|0x80,0x66|0x80,
    0x6d|0x80,0x7d|0x80,0x07|0x80,0x7f|0x80,0x6f|0x80
};
#endif

#define LINK_WAIT    0
#define LINK_REQUEST 1
#define LINK_SENT    2
#define LINK_SAFE    3

unsigned char LinkState = LINK_WAIT;

unsigned char RequestBuf[1];
unsigned char CodeBuf[4];

unsigned int RandomCounter = 0;

unsigned char LedValue = 0x01;

unsigned char PasswordVisible = 0;
unsigned char ShowTime = 0;

unsigned char RemoteGameTime = 60;

unsigned char SuccessLedStep = 0;
unsigned char SuccessScrollStep = 0;

unsigned char MusicIndex = 0;
unsigned char MusicPlaying = 0;

code unsigned int MusicFreq[] =
{
    523,659,784,1047,
    784,1047,1319,1568,
    1047,1319,1568,2093,
    1568,1319,1568,2093
};

code unsigned int MusicTime[] =
{
    10,10,10,18,
    10,10,10,18,
    10,10,10,18,
    10,10,10,30
};

#define MUSIC_LENGTH 16


void GenerateCode(void);
void DisplayTime(void);
void UpdateDisplay(void);
void SuccessScrollDisplay(void);
void StartMusic(void);
void MusicProcess(void);


void GenerateCode(void)
{
    CodeBuf[0] =
        (RandomCounter + 3) % 10;

    CodeBuf[1] =
        (RandomCounter / 3 + 5) % 10;

    CodeBuf[2] =
        (RandomCounter / 7 + 2) % 10;

    CodeBuf[3] =
        (RandomCounter / 11 + 8) % 10;
}


void DisplayTime(void)
{
    Seg7Print(
        12,12,12,12,12,12,
        RemoteGameTime / 10,
        RemoteGameTime % 10
    );
}


void UpdateDisplay(void)
{
    if(LinkState == LINK_SAFE)
        return;

    if(ShowTime == 1)
    {
        DisplayTime();
    }
    else if(PasswordVisible == 1)
    {
        Seg7Print(
            12,12,12,12,
            CodeBuf[0],
            CodeBuf[1],
            CodeBuf[2],
            CodeBuf[3]
        );
    }
    else
    {
        Seg7Print(
            12,12,12,12,
            12,12,12,12
        );
    }
}


void SuccessScrollDisplay(void)
{
    if(SuccessScrollStep == 0)
        Seg7Print(6,12,12,12,12,12,12,12);
    else if(SuccessScrollStep == 1)
        Seg7Print(6,6,12,12,12,12,12,12);
    else if(SuccessScrollStep == 2)
        Seg7Print(6,6,6,12,12,12,12,12);
    else if(SuccessScrollStep == 3)
        Seg7Print(6,6,6,6,12,12,12,12);
    else if(SuccessScrollStep == 4)
        Seg7Print(6,6,6,6,6,12,12,12);
    else if(SuccessScrollStep == 5)
        Seg7Print(6,6,6,6,6,6,12,12);
    else if(SuccessScrollStep == 6)
        Seg7Print(6,6,6,6,6,6,6,12);
    else
        Seg7Print(6,6,6,6,6,6,6,6);
}


void myUart2_callback(void)
{
    unsigned char rxData;

    rxData = RequestBuf[0];

    if(rxData & 0x80)
    {
        RemoteGameTime =
            rxData & 0x7F;

        if(ShowTime == 1 &&
           LinkState != LINK_SAFE)
        {
            UpdateDisplay();
        }

        return;
    }

    if(rxData == 'C')
    {
        LinkState = LINK_REQUEST;

        PasswordVisible = 0;

        LedPrint(0xFF);

        SetBeep(1200,10);

        UpdateDisplay();
    }
    else if(rxData == 'H')
    {
        PasswordVisible = 0;

        UpdateDisplay();
    }
    else if(rxData == 'S')
    {
        LinkState = LINK_SAFE;

        PasswordVisible = 0;
        ShowTime = 0;

        SuccessLedStep = 0;
        SuccessScrollStep = 0;

        SuccessScrollDisplay();

        StartMusic();
    }
}


void myKey_callback(void)
{
    unsigned char key1Act;
    unsigned char key2Act;

    key1Act = GetKeyAct(enumKey1);
    key2Act = GetKeyAct(enumKey2);

    if(key1Act == enumKeyPress)
    {
        if(LinkState == LINK_REQUEST)
        {
            GenerateCode();

            PasswordVisible = 1;

            LedPrint(0x0F);

            SetBeep(1600,10);

            Uart2Print(CodeBuf,4);

            LinkState = LINK_SENT;

            UpdateDisplay();
        }
    }

    if(key2Act == enumKeyPress)
    {
        if(LinkState != LINK_SAFE)
        {
            ShowTime = 1;

            UpdateDisplay();
        }
    }
    else if(key2Act == enumKeyRelease)
    {
        if(LinkState != LINK_SAFE)
        {
            ShowTime = 0;

            UpdateDisplay();
        }
    }
}


void my10mS_callback(void)
{
    if(LinkState == LINK_SAFE)
        MusicProcess();
}


void my100mS_callback(void)
{
    if(LinkState == LINK_WAIT)
    {
        RandomCounter++;

        if(RandomCounter >= 10000)
            RandomCounter = 0;

        LedPrint(LedValue);

        LedValue <<= 1;

        if(LedValue == 0)
            LedValue = 0x01;
    }

    if(LinkState == LINK_SAFE)
    {
        if(MusicPlaying == 1)
        {
            SuccessScrollStep++;

            if(SuccessScrollStep > 7)
                SuccessScrollStep = 0;

            SuccessScrollDisplay();
        }
        else
        {
            Seg7Print(
                6,6,6,6,6,6,6,6
            );
        }

        SuccessLedStep++;

        if(SuccessLedStep == 1)
            LedPrint(0x55);
        else if(SuccessLedStep == 2)
            LedPrint(0xAA);
        else if(SuccessLedStep == 3)
            LedPrint(0xFF);
        else
        {
            LedPrint(0x00);

            SuccessLedStep = 0;
        }
    }
}


void StartMusic(void)
{
    MusicIndex = 0;
    MusicPlaying = 1;
}


void MusicProcess(void)
{
    if(MusicPlaying == 0)
        return;

    if(GetBeepStatus() == enumBeepFree)
    {
        if(MusicIndex < MUSIC_LENGTH)
        {
            SetBeep(
                MusicFreq[MusicIndex],
                MusicTime[MusicIndex]
            );

            MusicIndex++;
        }
        else
        {
            MusicPlaying = 0;
        }
    }
}


void main(void)
{
    DisplayerInit();

    SetDisplayerArea(0,7);

    KeyInit();

    BeepInit();

    Uart2Init(
        9600,
        Uart2Usedfor485
    );

    SetUart2Rxd(
        RequestBuf,
        1,
        0,
        0
    );

    SetEventCallBack(
        enumEventKey,
        myKey_callback
    );

    SetEventCallBack(
        enumEventSys10mS,
        my10mS_callback
    );

    SetEventCallBack(
        enumEventSys100mS,
        my100mS_callback
    );

    SetEventCallBack(
        enumEventUart2Rxd,
        myUart2_callback
    );

    MySTC_Init();

    LedPrint(0x00);

    Seg7Print(
        12,12,12,12,
        12,12,12,12
    );

    while(1)
        MySTC_OS();
}