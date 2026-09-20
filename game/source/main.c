#include "STC15F2K60S2.H"
#include "sys.H"
#include "displayer.H"
#include "Key.h"
#include "adc.h"
#include "beep.h"
#include "uart1.h"
#include "uart2.h"
#include "hall.h"

code unsigned long SysClock = 11059200;

#ifdef _displayer_H_
code char decode_table[] =
{
    0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,
    0x00,0x08,0x40,0x01,0x41,0x48,
    0x3f|0x80,0x06|0x80,0x5b|0x80,0x4f|0x80,0x66|0x80,
    0x6d|0x80,0x7d|0x80,0x07|0x80,0x7f|0x80,0x6f|0x80,
    0x38,0x3f,0x6d,0x79
};
#endif

#define DISP_L 26
#define DISP_O 27
#define DISP_S 28
#define DISP_E 29

#define GAME_IDLE      0
#define GAME_LIGHT     1
#define GAME_LINK      2
#define GAME_HALL      3
#define GAME_PASSWORD  4
#define GAME_REACTION  5
#define GAME_SUCCESS   6
#define GAME_FAILED    7

#define EASY_MODE      1
#define NORMAL_MODE    2
#define HARD_MODE      3

#define LIGHT_THRESHOLD  20
#define LIGHT_KEEP_COUNT 50

#define PASSWORD_PENALTY 5
#define HINT_PENALTY     10

#define MUSIC_NONE      0
#define MUSIC_SUCCESS   1
#define MUSIC_FAIL      2

#define PC_QUEUE_SIZE   4
#define PC_PACKET_SIZE  16

unsigned char GameState = GAME_IDLE;
unsigned char GameTime = 60;

unsigned char Difficulty = EASY_MODE;
unsigned int ReactionLimit = 3000;

unsigned char LightKeepCounter = 0;
unsigned char LedValue = 0x01;
unsigned int LightValue = 0;

unsigned char LinkRequest = 'C';
unsigned char HideCommand = 'H';
unsigned char SafeCommand = 'S';
unsigned char TimePacket = 0;

unsigned char LinkRxBuf[4];
unsigned char BombCode[4];
unsigned char LinkHoldTime = 0;

unsigned char InputCode[4] = {0,0,0,0};
unsigned char InputPos = 0;

unsigned char HintIndex = 0;
unsigned char HintShowTime = 0;

unsigned long RandomCounter = 0;

unsigned char ReactionStarted = 0;
unsigned char ReactionFlowLed = 0x01;
unsigned int ReactionDelay = 0;
unsigned int ReactionWait = 0;
unsigned int ReactionTime = 0;
unsigned char SignalStage = 0;
unsigned int SignalDelayCounter = 0;
unsigned char ReactionReady = 0;

unsigned char SuccessLedStep = 0;
unsigned char SuccessScrollStep = 0;

unsigned char FailEffect = 0;
unsigned char FailEffectCount = 0;

unsigned char MusicMode = MUSIC_NONE;
unsigned char MusicIndex = 0;
unsigned char MusicGap = 0;

unsigned char ResetNotifyCount = 0;
unsigned char ResetNotifyTick = 0;

xdata char PcQueue[PC_QUEUE_SIZE][PC_PACKET_SIZE];
xdata unsigned char PcQueueLen[PC_QUEUE_SIZE];
xdata char PcTxBuf[PC_PACKET_SIZE];

unsigned char PcQueueHead = 0;
unsigned char PcQueueTail = 0;
unsigned char PcQueueCount = 0;
unsigned char PcTxLen = 0;
unsigned char PcTxActive = 0;

code unsigned int SuccessFreq[] =
{
    523,659,784,1047,
    784,1047,1319,1568,
    1047,1319,1568,2093
};

code unsigned char SuccessTime[] =
{
    8,8,8,15,
    8,8,8,15,
    8,8,10,25
};

#define SUCCESS_LEN 12

code unsigned int FailFreq[] =
{
    784,659,523,392,330,262,196
};

code unsigned char FailTime[] =
{
    10,10,12,14,16,20,35
};

#define FAIL_LEN 7


void GameStart(void);

void EnterLightStage(void);
void LightStageProcess(void);
void LightStagePass(void);
void DisplayLightValue(unsigned int value);

void EnterLinkStage(void);
void LinkStagePass(void);

void EnterHallStage(void);
void HallStagePass(void);

void EnterPasswordStage(void);
void DisplayPassword(void);
void CheckPassword(void);
void PasswordStagePass(void);
void ShowHint(void);

void EnterReactionStage(void);
void RestartReaction(void);
void ReactionSignal(void);
void ReactionStagePass(void);

void ShowDifficulty(void);
void SetReactionLimit(void);

void SyncTimeToB(void);

void SuccessScrollDisplay(void);

void GameSuccess(void);
void GameFail(void);

void StartSuccessMusic(void);
void StartFailMusic(void);
void MusicProcess(void);

void PcQueueCommit(unsigned char len);
void PcSendProcess(void);
void PcSendDifficulty(void);
void PcSendStage(unsigned char stage);
void PcSendTime(void);
void PcSendStart(void);
void PcSendWin(void);
void PcSendFail(void);
void PcSendReset(void);


void PcQueueCommit(unsigned char len)
{
    PcQueueLen[PcQueueTail] = len;

    PcQueueTail++;

    if(PcQueueTail >= PC_QUEUE_SIZE)
        PcQueueTail = 0;

    PcQueueCount++;
}


void PcSendDifficulty(void)
{
    if(PcQueueCount >= PC_QUEUE_SIZE)
        return;

    PcQueue[PcQueueTail][0] = 'D';
    PcQueue[PcQueueTail][1] = 'I';
    PcQueue[PcQueueTail][2] = 'F';
    PcQueue[PcQueueTail][3] = 'F';
    PcQueue[PcQueueTail][4] = ':';
    PcQueue[PcQueueTail][5] = Difficulty + '0';
    PcQueue[PcQueueTail][6] = '\r';
    PcQueue[PcQueueTail][7] = '\n';

    PcQueueCommit(8);
}


void PcSendStage(unsigned char stage)
{
    if(PcQueueCount >= PC_QUEUE_SIZE)
        return;

    PcQueue[PcQueueTail][0] = 'S';
    PcQueue[PcQueueTail][1] = 'T';
    PcQueue[PcQueueTail][2] = 'A';
    PcQueue[PcQueueTail][3] = 'G';
    PcQueue[PcQueueTail][4] = 'E';
    PcQueue[PcQueueTail][5] = ':';
    PcQueue[PcQueueTail][6] = stage + '0';
    PcQueue[PcQueueTail][7] = '\r';
    PcQueue[PcQueueTail][8] = '\n';

    PcQueueCommit(9);
}


void PcSendTime(void)
{
    if(PcQueueCount >= PC_QUEUE_SIZE)
        return;

    PcQueue[PcQueueTail][0] = 'T';
    PcQueue[PcQueueTail][1] = 'I';
    PcQueue[PcQueueTail][2] = 'M';
    PcQueue[PcQueueTail][3] = 'E';
    PcQueue[PcQueueTail][4] = ':';

    PcQueue[PcQueueTail][5] = GameTime / 10 + '0';
    PcQueue[PcQueueTail][6] = GameTime % 10 + '0';

    PcQueue[PcQueueTail][7] = '\r';
    PcQueue[PcQueueTail][8] = '\n';

    PcQueueCommit(9);
}


void PcSendStart(void)
{
    if(PcQueueCount >= PC_QUEUE_SIZE)
        return;

    PcQueue[PcQueueTail][0] = 'S';
    PcQueue[PcQueueTail][1] = 'T';
    PcQueue[PcQueueTail][2] = 'A';
    PcQueue[PcQueueTail][3] = 'R';
    PcQueue[PcQueueTail][4] = 'T';
    PcQueue[PcQueueTail][5] = ':';

    PcQueue[PcQueueTail][6] = Difficulty + '0';
    PcQueue[PcQueueTail][7] = ':';

    PcQueue[PcQueueTail][8] = GameTime / 10 + '0';
    PcQueue[PcQueueTail][9] = GameTime % 10 + '0';

    PcQueue[PcQueueTail][10] = ':';
    PcQueue[PcQueueTail][11] = '1';
    PcQueue[PcQueueTail][12] = '\r';
    PcQueue[PcQueueTail][13] = '\n';

    PcQueueCommit(14);
}


void PcSendWin(void)
{
    if(PcQueueCount >= PC_QUEUE_SIZE)
        return;

    PcQueue[PcQueueTail][0] = 'W';
    PcQueue[PcQueueTail][1] = 'I';
    PcQueue[PcQueueTail][2] = 'N';
    PcQueue[PcQueueTail][3] = '\r';
    PcQueue[PcQueueTail][4] = '\n';

    PcQueueCommit(5);
}


void PcSendFail(void)
{
    if(PcQueueCount >= PC_QUEUE_SIZE)
        return;

    PcQueue[PcQueueTail][0] = 'L';
    PcQueue[PcQueueTail][1] = 'O';
    PcQueue[PcQueueTail][2] = '5';
    PcQueue[PcQueueTail][3] = 'E';
    PcQueue[PcQueueTail][4] = '\r';
    PcQueue[PcQueueTail][5] = '\n';

    PcQueueCommit(6);
}


void PcSendReset(void)
{
    if(PcQueueCount >= PC_QUEUE_SIZE)
        return;

    PcQueue[PcQueueTail][0] = 'R';
    PcQueue[PcQueueTail][1] = 'E';
    PcQueue[PcQueueTail][2] = 'S';
    PcQueue[PcQueueTail][3] = 'E';
    PcQueue[PcQueueTail][4] = 'T';
    PcQueue[PcQueueTail][5] = '\r';
    PcQueue[PcQueueTail][6] = '\n';

    PcQueueCommit(7);
}


void PcSendProcess(void)
{
    unsigned char i;

    if(PcTxActive == 1)
    {
        if(GetUart1TxStatus() == enumUart1TxFree)
            PcTxActive = 0;
        else
            return;
    }

    if(PcQueueCount == 0)
        return;

    if(GetUart1TxStatus() != enumUart1TxFree)
        return;

    PcTxLen = PcQueueLen[PcQueueHead];

    for(i = 0; i < PcTxLen; i++)
        PcTxBuf[i] = PcQueue[PcQueueHead][i];

    if(Uart1Print(PcTxBuf, PcTxLen) == enumUart1TxOK)
    {
        PcQueueHead++;

        if(PcQueueHead >= PC_QUEUE_SIZE)
            PcQueueHead = 0;

        PcQueueCount--;

        PcTxActive = 1;
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
        if(GameState == GAME_IDLE)
        {
            GameStart();
        }
        else if(GameState == GAME_REACTION)
        {
            if(ReactionStarted == 0)
            {
                ReactionStarted = 1;

                LedPrint(0x00);

                SetBeep(1400,8);

                RestartReaction();
            }
            else
            {
                if(SignalStage < 2)
                {
                    GameFail();
                }
                else if(ReactionReady == 1)
                {
                    if(ReactionTime <= ReactionLimit)
                        ReactionStagePass();
                    else
                        GameFail();
                }
            }
        }
    }

    if(key2Act == enumKeyPress)
    {
        if(GameState == GAME_IDLE)
        {
            Difficulty++;

            if(Difficulty > HARD_MODE)
                Difficulty = EASY_MODE;

            ShowDifficulty();

            SetBeep(1000 + Difficulty * 300,5);

            PcSendDifficulty();
        }
    }
}


void myNav_callback(void)
{
    if(GameState != GAME_PASSWORD)
        return;

    if(GetAdcNavAct(enumAdcNavKey3) == enumKeyPress)
    {
        ShowHint();
        return;
    }

    if(HintShowTime > 0)
        return;

    if(GetAdcNavAct(enumAdcNavKeyUp) == enumKeyPress)
    {
        InputCode[InputPos]++;

        if(InputCode[InputPos] > 9)
            InputCode[InputPos] = 0;

        DisplayPassword();
        SetBeep(1200,3);
    }

    if(GetAdcNavAct(enumAdcNavKeyDown) == enumKeyPress)
    {
        if(InputCode[InputPos] == 0)
            InputCode[InputPos] = 9;
        else
            InputCode[InputPos]--;

        DisplayPassword();
        SetBeep(1000,3);
    }

    if(GetAdcNavAct(enumAdcNavKeyLeft) == enumKeyPress)
    {
        if(InputPos > 0)
            InputPos--;

        DisplayPassword();
        SetBeep(900,3);
    }

    if(GetAdcNavAct(enumAdcNavKeyRight) == enumKeyPress)
    {
        if(InputPos < 3)
            InputPos++;

        DisplayPassword();
        SetBeep(900,3);
    }

    if(GetAdcNavAct(enumAdcNavKeyCenter) == enumKeyPress)
        CheckPassword();
}


void myUart2_callback(void)
{
    unsigned char i;

    if(GameState == GAME_LINK)
    {
        for(i = 0; i < 4; i++)
            BombCode[i] = LinkRxBuf[i];

        LinkStagePass();
    }
}


void myHall_callback(void)
{
    if(GameState == GAME_HALL)
    {
        if(GetHallAct() == enumHallGetClose)
            HallStagePass();
    }
}


void my1mS_callback(void)
{
    RandomCounter++;

    if(RandomCounter >= 60000)
        RandomCounter = 0;

    if(GameState == GAME_REACTION &&
       ReactionStarted == 1)
    {
        if(SignalStage == 0)
        {
            if(ReactionWait < ReactionDelay)
                ReactionWait++;

            if(ReactionWait >= ReactionDelay)
                ReactionSignal();
        }
        else if(SignalStage == 1)
        {
            SignalDelayCounter++;

            if(SignalDelayCounter >= 300)
            {
                SignalStage = 2;
                ReactionReady = 1;
                ReactionTime = 0;

                SetBeep(2500,8);
            }
        }
        else if(SignalStage == 2)
        {
            if(ReactionTime < 9999)
                ReactionTime++;

            if(ReactionTime > ReactionLimit)
                GameFail();
        }
    }
}


void my10mS_callback(void)
{
    PcSendProcess();

    MusicProcess();
}


void my100mS_callback(void)
{
    if(GameState == GAME_IDLE &&
       ResetNotifyCount < 3)
    {
        ResetNotifyTick++;

        if(ResetNotifyTick >= 5)
        {
            ResetNotifyTick = 0;

            PcSendReset();

            ResetNotifyCount++;
        }
    }

    if(GameState == GAME_LIGHT)
        LightStageProcess();

    if(GameState == GAME_IDLE)
    {
        LedPrint(LedValue);

        LedValue <<= 1;

        if(LedValue == 0)
            LedValue = 0x01;
    }

    if(GameState == GAME_REACTION &&
       ReactionStarted == 0)
    {
        LedPrint(ReactionFlowLed);

        ReactionFlowLed <<= 1;

        if(ReactionFlowLed == 0)
            ReactionFlowLed = 0x01;
    }

    if(GameState == GAME_SUCCESS)
    {
        if(MusicMode == MUSIC_SUCCESS)
        {
            SuccessScrollStep++;

            if(SuccessScrollStep > 7)
                SuccessScrollStep = 0;

            SuccessScrollDisplay();
        }
        else
        {
            Seg7Print(6,6,6,6,6,6,6,6);
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

    if(GameState == GAME_FAILED &&
       FailEffect == 1)
    {
        FailEffectCount++;

        if(FailEffectCount == 1)
            LedPrint(0xFF);
        else if(FailEffectCount == 2)
            LedPrint(0x00);
        else if(FailEffectCount == 3)
            LedPrint(0xFF);
        else
        {
            LedPrint(0x00);
            FailEffect = 0;
        }
    }
}


void my1S_callback(void)
{
    if(GameState != GAME_IDLE &&
       GameState != GAME_SUCCESS &&
       GameState != GAME_FAILED)
    {
        if(GameTime > 0)
            GameTime--;

        if(GameTime == 0)
        {
            SyncTimeToB();
            PcSendTime();

            GameFail();

            return;
        }
    }

    if(GameState == GAME_LINK &&
       LinkHoldTime > 0)
    {
        LinkHoldTime--;

        if(LinkHoldTime == 0)
        {
            Uart2Print(&HideCommand,1);

            EnterHallStage();

            return;
        }
    }

    if(GameState == GAME_PASSWORD &&
       HintShowTime > 0)
    {
        HintShowTime--;

        if(HintShowTime == 0)
            DisplayPassword();
    }

    if(GameState != GAME_IDLE &&
       GameState != GAME_SUCCESS &&
       GameState != GAME_FAILED)
    {
        SyncTimeToB();

        PcSendTime();
    }
}


void ShowDifficulty(void)
{
    Seg7Print(
        Difficulty,
        12,12,12,12,12,12,12
    );
}


void SetReactionLimit(void)
{
    if(Difficulty == EASY_MODE)
        ReactionLimit = 3000;
    else if(Difficulty == NORMAL_MODE)
        ReactionLimit = 2500;
    else
        ReactionLimit = 1500;
}


void SyncTimeToB(void)
{
    TimePacket =
        0x80 | (GameTime & 0x7F);

    Uart2Print(
        &TimePacket,
        1
    );
}


void GameStart(void)
{
    GameTime = 60;

    LightKeepCounter = 0;

    HintIndex = 0;
    HintShowTime = 0;

    SetReactionLimit();

    LedPrint(0x00);

    SetBeep(1200,10);

    SyncTimeToB();

    PcSendStart();

    EnterLightStage();
}


void EnterLightStage(void)
{
    GameState = GAME_LIGHT;

    LightKeepCounter = 0;

    LedPrint(0x01);

    Seg7Print(
        1,12,12,12,
        12,12,12,12
    );
}


void LightStageProcess(void)
{
    struct_ADC adcData;

    adcData = GetADC();

    LightValue = adcData.Rop;

    DisplayLightValue(LightValue);

    if(LightValue < LIGHT_THRESHOLD)
    {
        if(LightKeepCounter < LIGHT_KEEP_COUNT)
            LightKeepCounter++;

        if(LightKeepCounter < 7)
            LedPrint(0x01);
        else if(LightKeepCounter < 14)
            LedPrint(0x03);
        else if(LightKeepCounter < 21)
            LedPrint(0x07);
        else if(LightKeepCounter < 28)
            LedPrint(0x0F);
        else if(LightKeepCounter < 35)
            LedPrint(0x1F);
        else if(LightKeepCounter < 42)
            LedPrint(0x3F);
        else if(LightKeepCounter < 49)
            LedPrint(0x7F);
        else
            LedPrint(0xFF);

        if(LightKeepCounter >= LIGHT_KEEP_COUNT)
            LightStagePass();
    }
    else
    {
        LightKeepCounter = 0;

        LedPrint(0x01);
    }
}


void LightStagePass(void)
{
    LedPrint(0xFF);

    SetBeep(1600,15);

    EnterLinkStage();
}


void DisplayLightValue(unsigned int value)
{
    unsigned char d0;
    unsigned char d1;
    unsigned char d2;
    unsigned char d3;

    d0 = value / 1000;
    d1 = value / 100 % 10;
    d2 = value / 10 % 10;
    d3 = value % 10;

    Seg7Print(
        1,12,12,12,
        d0,d1,d2,d3
    );
}


void EnterLinkStage(void)
{
    GameState = GAME_LINK;

    LinkHoldTime = 0;

    LedPrint(0x03);

    Seg7Print(
        2,12,12,12,
        12,12,12,12
    );

    PcSendStage(2);

    Uart2Print(
        &LinkRequest,
        1
    );
}


void LinkStagePass(void)
{
    LedPrint(0xFF);

    SetBeep(1800,15);

    Seg7Print(
        2,12,12,12,
        12,12,12,12
    );

    LinkHoldTime = 4;
}


void EnterHallStage(void)
{
    GameState = GAME_HALL;

    LedPrint(0x07);

    Seg7Print(
        3,12,12,12,
        12,12,12,12
    );

    PcSendStage(3);
}


void HallStagePass(void)
{
    LedPrint(0xFF);

    SetBeep(2000,20);

    EnterPasswordStage();
}


void EnterPasswordStage(void)
{
    unsigned char i;

    GameState = GAME_PASSWORD;

    InputPos = 0;

    HintIndex = 0;
    HintShowTime = 0;

    for(i = 0; i < 4; i++)
        InputCode[i] = 0;

    LedPrint(0x0F);

    DisplayPassword();

    PcSendStage(4);
}


void DisplayPassword(void)
{
    Seg7Print(
        4,
        InputPos + 1,
        12,
        12,
        InputCode[0],
        InputCode[1],
        InputCode[2],
        InputCode[3]
    );
}


void ShowHint(void)
{
    if(HintShowTime > 0)
        return;

    if(GameTime <= HINT_PENALTY)
    {
        GameTime = 0;

        SyncTimeToB();
        PcSendTime();

        GameFail();

        return;
    }

    GameTime -= HINT_PENALTY;

    SyncTimeToB();
    PcSendTime();

    SetBeep(700,10);

    if(HintIndex == 0)
    {
        Seg7Print(
            4,12,12,12,
            BombCode[0],
            12,12,12
        );
    }
    else if(HintIndex == 1)
    {
        Seg7Print(
            4,12,12,12,
            12,
            BombCode[1],
            12,12
        );
    }
    else if(HintIndex == 2)
    {
        Seg7Print(
            4,12,12,12,
            12,12,
            BombCode[2],
            12
        );
    }
    else
    {
        Seg7Print(
            4,12,12,12,
            12,12,12,
            BombCode[3]
        );
    }

    HintIndex++;

    if(HintIndex >= 4)
        HintIndex = 0;

    HintShowTime = 1;
}


void CheckPassword(void)
{
    unsigned char i;

    for(i = 0; i < 4; i++)
    {
        if(InputCode[i] != BombCode[i])
        {
            if(GameTime > PASSWORD_PENALTY)
                GameTime -= PASSWORD_PENALTY;
            else
                GameTime = 0;

            SyncTimeToB();
            PcSendTime();

            if(GameTime == 0)
            {
                GameFail();

                return;
            }

            LedPrint(0x81);

            SetBeep(500,20);

            DisplayPassword();

            return;
        }
    }

    PasswordStagePass();
}


void PasswordStagePass(void)
{
    LedPrint(0xFF);

    SetBeep(2200,20);

    EnterReactionStage();
}


void EnterReactionStage(void)
{
    GameState = GAME_REACTION;

    ReactionStarted = 0;
    ReactionFlowLed = 0x01;
    ReactionReady = 0;
    SignalStage = 0;
    SignalDelayCounter = 0;

    LedPrint(ReactionFlowLed);

    Seg7Print(
        5,12,12,12,
        12,12,12,12
    );

    PcSendStage(5);
}


void RestartReaction(void)
{
    ReactionReady = 0;
    ReactionWait = 0;
    ReactionTime = 0;
    SignalStage = 0;
    SignalDelayCounter = 0;

    ReactionDelay =
        2500 + (RandomCounter % 3501);

    LedPrint(0x00);

    Seg7Print(
        5,12,12,12,
        12,12,12,12
    );
}


void ReactionSignal(void)
{
    SignalStage = 1;

    SignalDelayCounter = 0;

    LedPrint(0xFF);
}


void ReactionStagePass(void)
{
    ReactionReady = 0;

    GameSuccess();
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


void StartSuccessMusic(void)
{
    MusicMode = MUSIC_SUCCESS;
    MusicIndex = 0;
    MusicGap = 0;
}


void StartFailMusic(void)
{
    MusicMode = MUSIC_FAIL;
    MusicIndex = 0;
    MusicGap = 0;
}


void MusicProcess(void)
{
    if(MusicMode == MUSIC_NONE)
        return;

    if(MusicGap > 0)
    {
        MusicGap--;
        return;
    }

    if(GetBeepStatus() != enumBeepFree)
        return;

    if(MusicMode == MUSIC_SUCCESS)
    {
        if(MusicIndex >= SUCCESS_LEN)
        {
            MusicMode = MUSIC_NONE;
            return;
        }

        SetBeep(
            SuccessFreq[MusicIndex],
            SuccessTime[MusicIndex]
        );

        MusicIndex++;

        MusicGap = 3;
    }
    else if(MusicMode == MUSIC_FAIL)
    {
        if(MusicIndex >= FAIL_LEN)
        {
            MusicMode = MUSIC_NONE;
            return;
        }

        SetBeep(
            FailFreq[MusicIndex],
            FailTime[MusicIndex]
        );

        MusicIndex++;

        MusicGap = 4;
    }
}


void GameSuccess(void)
{
    if(GameState == GAME_SUCCESS)
        return;

    GameState = GAME_SUCCESS;

    ReactionReady = 0;

    MusicMode = MUSIC_NONE;

    SuccessLedStep = 0;
    SuccessScrollStep = 0;

    Uart2Print(
        &SafeCommand,
        1
    );

    PcSendWin();

    SuccessScrollDisplay();

    StartSuccessMusic();
}


void GameFail(void)
{
    if(GameState == GAME_FAILED)
        return;

    GameState = GAME_FAILED;

    ReactionReady = 0;
    ReactionStarted = 0;
    SignalStage = 0;

    MusicMode = MUSIC_NONE;

    FailEffect = 1;
    FailEffectCount = 0;

    LedPrint(0xFF);

    Seg7Print(
        0,0,0,0,
        DISP_L,
        DISP_O,
        DISP_S,
        DISP_E
    );

    PcSendFail();

    StartFailMusic();
}


void main(void)
{
    DisplayerInit();

    SetDisplayerArea(0,7);

    KeyInit();

    AdcInit(ADCexpEXT);

    BeepInit();

    HallInit();

    Uart1Init(9600);

    Uart2Init(
        9600,
        Uart2Usedfor485
    );

    SetUart2Rxd(
        LinkRxBuf,
        4,
        0,
        0
    );

    SetEventCallBack(
        enumEventKey,
        myKey_callback
    );

    SetEventCallBack(
        enumEventNav,
        myNav_callback
    );

    SetEventCallBack(
        enumEventSys1mS,
        my1mS_callback
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
        enumEventSys1S,
        my1S_callback
    );

    SetEventCallBack(
        enumEventUart2Rxd,
        myUart2_callback
    );

    SetEventCallBack(
        enumEventHall,
        myHall_callback
    );

    MySTC_Init();

    LedPrint(0x00);

    ShowDifficulty();

    while(1)
    {
        MySTC_OS();
    }
}