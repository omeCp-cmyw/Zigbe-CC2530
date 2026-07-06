/******************************************************************************
  Filename:       enddevice.c（终端设备）
  Revised:        $Date: 2012-03-07 01:04:58 -0800 (Wed, 07 Mar 2012) $
  Revision:       $Revision: 29656 $

  Description:    Generic Application (no Profile).


  Copyright 2004-2012 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License"). You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product. Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED 揂S IS?WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
******************************************************************************/

/*********************************************************************
  This application isn't intended to do anything useful, it is
  intended to be a simple example of an application's structure.

  This application sends "Hello World" to another "Generic"
  application every 5 seconds.  The application will also
  receives "Hello World" packets.

  The "Hello World" messages are sent/received as MSG type message.

  This applications doesn't have a profile, so it handles everything
  directly - itself.

  Key control:
    SW1:
    SW2:  initiates end device binding
    SW3:
    SW4:  initiates a match description request
*********************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "GenericApp.h"
#include "DebugTrace.h"
#if !defined( WIN32 )
  #include "OnBoard.h"
#endif

/* HAL */
#include "delay.h"
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "MT_UART.h"
#include "MT_APP.h"
#include "MT.h"
#include "DHT11.h"
#include "stdio.h"
#include "string.h"
#include "IOT.h"
#include "hal_adc.h"
#include <stdio.h>
#include <string.h>

extern void LCD_P8x16Str(unsigned char x, unsigned char y,unsigned char ch[]);
extern void HalLcdDisplayPercentBar( char *title, uint8 value );
extern void LCD_P16x16Ch(unsigned char x, unsigned char y, unsigned char N);
void controlMotor(unsigned char* v_MessageData);//!< 控制电机

unsigned char getFrame(void);
unsigned int openLed(unsigned char* v_MessageData);//!< 控制led灯
/* RTOS */
#if defined( IAR_ARMCM3_LM )
#include "RTOS_App.h"
#endif  

/*********************************************************************
 * MACROS
 */

#define TIMER_START() T1CTL|=0X03   //!< 启动定时器
#define TIMER_STOP() T1CTL&=~0X03   //!< 停止定时器

void Init_Beep(void);
void InitTimer(void);
void startPwm(void);

void controlBeer(unsigned char* v_MsgData);

#define BEEP_PIN     P0_7  //!< 定义P1.7口为蜂鸣器的输出端



/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
// This list should be filled with Application specific Cluster IDs.
const cId_t GenericApp_ClusterList[GENERICAPP_MAX_CLUSTERS] =
{
  GENERICAPP_CLUSTERID
};

const SimpleDescriptionFormat_t GenericApp_SimpleDesc =
{
  GENERICAPP_ENDPOINT,              //  int Endpoint;
  GENERICAPP_PROFID,                //  uint16 AppProfId[2];
  GENERICAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  GENERICAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  GENERICAPP_FLAGS,                 //  int   AppFlags:4;
  GENERICAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)GenericApp_ClusterList,  //  byte *pAppInClusterList;
  GENERICAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)GenericApp_ClusterList   //  byte *pAppInClusterList;
};

// This is the Endpoint/Interface description.  It is defined here, but
// filled-in in GenericApp_Init().  Another way to go would be to fill
// in the structure here and make it a "const" (in code space).  The
// way it's defined in this sample app it is define in RAM.
endPointDesc_t GenericApp_epDesc;

#define RELAY P1_5       //!< 定义P0.6定义为输出口(继电器控制开关)

/*********************************************************************
 * EXTERNAL VARIABLES
 */


/*********************************************************************
 * EXTERNAL FUNCTIONS
 */
#define FRAME P1_6
/*********************************************************************
 * LOCAL VARIABLES
 */
byte GenericApp_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // GenericApp_Init() is called.
devStates_t GenericApp_NwkState;


byte GenericApp_TransID;  // This is the unique message ID (counter)

afAddrType_t GenericApp_DstAddr;

#define DATA_PIN P0_6            //定义P0.6口为传感器的输入端

//byte sensorID = '1';

//!< 定义步进电机连接端口
#define A P0_4 //!< 连接 1N1
#define B P0_5 //!< 连接 1N2
#define C P0_6 //!< 连接 1N3
#define D P0_7 //!< 连接 1N4
uchar funcForward[4] = {0x80,0x40,0x20,0x10};//!< 正转 电机导通相序 D-C-B-A
uchar funcReverse[4] = {0x10,0x20,0x40,0x80};//!< 反转 电机导通相序 A-B-C-D

uchar ucSpeed;

void MotorDoProcessedData(uchar v_ucdata); //!< 点击转动处理过程
//!< 顺时针转动
void MotorDoProcessedShun(uchar Speed);
//!< 逆时针转动
void MotorDoProcessedNi(uchar Speed);
//停止转动
void MotorDoProcessedStop(void);

void InitIO(void);//!< IO引脚初始化工作
/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
static void GenericApp_HandleKeys( byte shift, byte keys );
static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
static void GenericApp_SendTheMessage( void );
void UartInitPort0(void );
static void rxCB(uint8 port,uint8 event);
unsigned char GetADCMQ(void);
unsigned char GetADCVol(void);
int controlRelay(unsigned char* v_MessageData);

int HCSR501(void);

#if defined( IAR_ARMCM3_LM )
static void GenericApp_ProcessRtosMessage( void );
#endif

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */


void UartInitPort0(void )
{

          halUARTCfg_t uartConfig;//顶一个串口结构体
          uartConfig.configured             =TRUE;//串口配置为真
          uartConfig.baudRate               =HAL_UART_BR_115200;//波特率为115200
          uartConfig.flowControl            =FALSE;//流控制为假
          uartConfig.callBackFunc       =    rxCB;
          HalUARTOpen(HAL_UART_PORT_0,&uartConfig);// 打开串口0
}

static void rxCB(uint8 port,uint8 event)
{
  unsigned char j=0,uartbuf[128]={0x00};
  while (Hal_UART_RxBufLen(0)) //检测串口数据是否接收完成
  {
    HalUARTRead (0,&uartbuf[j], 1);  //把数据接收放到buf中
    j++;                           //记录字符数
  } 
}
/*********************************************************************
 * @fn      GenericApp_Init
 *
 * @brief   Initialization function for the Generic App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void GenericApp_Init( uint8 task_id )
{
 
  GenericApp_TaskID = task_id;
  GenericApp_NwkState = DEV_INIT;
  GenericApp_TransID = 0;
  P1DIR|=0X03;//!<  设置P10 P11端口输出
  if(ENDNUM==4)
  {
  startPwm();             //!< 配置T1输出PWM
  TIMER_STOP();          //!< 默认关闭蜂鸣器
  }
  if(ENDNUM == 5)
  {
    InitIO();
    //!< 改变这个参数可以调整电机转速，数字越小，转速越大,力矩越小
    ucSpeed = 2; 
  }
  if(ENDNUM == 3)
  {
    P1DIR |= 0x20;   //!<  设置P1_5为输出口
    P0SEL &= ~0x40;
    P0DIR &= ~0x40; 
  }
    
  
  UartInitPort0();//初始化下串口，方便数据打印
  GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;//表明单播功能
  GenericApp_DstAddr.endPoint = GENERICAPP_ENDPOINT;//发到协调器的对应的端点位置，此端点位置需要和协调器的端点位置保持一致
  GenericApp_DstAddr.addr.shortAddr = 0x0000;//发送给网络端的协调器

  // Fill out the endpoint description.
  GenericApp_epDesc.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_epDesc.task_id = &GenericApp_TaskID;
  GenericApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&GenericApp_SimpleDesc;
  GenericApp_epDesc.latencyReq = noLatencyReqs;

  // Register the endpoint description with the AF
  afRegister( &GenericApp_epDesc );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( GenericApp_TaskID );

  // Update the display
#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "GenericApp", HAL_LCD_LINE_1 );
#endif

  ZDO_RegisterForZDOMsg( GenericApp_TaskID, End_Device_Bind_rsp );
  ZDO_RegisterForZDOMsg( GenericApp_TaskID, Match_Desc_rsp );

#if defined( IAR_ARMCM3_LM )
  // Register this task with RTOS task initiator
  RTOS_RegisterApp( task_id, GENERICAPP_RTOS_MSG_EVT );
#endif
}

/*********************************************************************
 * @fn      GenericApp_ProcessEvent
 *
 * @brief   Generic Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  none
 */
uint16 GenericApp_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  afDataConfirm_t *afDataConfirm;

  // Data Confirmation message fields
  byte sentEP;
  ZStatus_t sentStatus;
  byte sentTransID;       // This should match the value sent
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZDO_CB_MSG:
          GenericApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          GenericApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case AF_DATA_CONFIRM_CMD:
          // This message is received as a confirmation of a data packet sent.
          // The status is of ZStatus_t type [defined in ZComDef.h]
          // The message fields are defined in AF.h
          afDataConfirm = (afDataConfirm_t *)MSGpkt;
          sentEP = afDataConfirm->endpoint;
          sentStatus = afDataConfirm->hdr.status;
          sentTransID = afDataConfirm->transID;
          (void)sentEP;
          (void)sentTransID;

          // Action taken when confirmation is received.
          if ( sentStatus != ZSuccess )
          {
            // The data wasn't delivered -- Do something
          }
          break;

        case AF_INCOMING_MSG_CMD:
          GenericApp_MessageMSGCB( MSGpkt );
          break;

        case ZDO_STATE_CHANGE://!<  网络状态改变触发
          GenericApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if ( (GenericApp_NwkState == DEV_ZB_COORD)
              || (GenericApp_NwkState == DEV_ROUTER)
              || (GenericApp_NwkState == DEV_END_DEVICE) )
          {
            //!<  设置定时器等待事件
            osal_start_timerEx( GenericApp_TaskID,
                                GENERICAPP_SEND_MSG_EVT,
                                GENERICAPP_SEND_MSG_TIMEOUT );
          }
          break;

        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );

      // Next
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( GenericApp_TaskID );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  // Send a message out - This event is generated by a timer
  //  (setup in GenericApp_Init()).
  if ( events & GENERICAPP_SEND_MSG_EVT )  //!< 发送事件触发
  {
    // Send "the" message
    GenericApp_SendTheMessage();//数据采集发送函数
    // Setup to send message again
    //!<  这里是间隔多久调用一次事件发送函数，GENERICAPP_SEND_MSG_TIMEOUT就是的，我们可以设定时间设置2s一次发数据
    osal_start_timerEx( GenericApp_TaskID,
                        GENERICAPP_SEND_MSG_EVT,
                        GENERICAPP_SEND_MSG_TIMEOUT+(osal_rand()&0x00FF));
    // return unprocessed events
    return (events ^ GENERICAPP_SEND_MSG_EVT);
  }

  
#if defined( IAR_ARMCM3_LM )
  // Receive a message from the RTOS queue
  if ( events & GENERICAPP_RTOS_MSG_EVT )
  {
    // Process message from RTOS queue
    GenericApp_ProcessRtosMessage();

    // return unprocessed events
    return (events ^ GENERICAPP_RTOS_MSG_EVT);
  }
#endif

  // Discard unknown events
  return 0;
}

/*********************************************************************
 * Event Generation Functions
 */

/*********************************************************************
 * @fn      GenericApp_ProcessZDOMsgs()
 *
 * @brief   Process response messages
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg )
{
  switch ( inMsg->clusterID )
  {
    case End_Device_Bind_rsp://请求是发给我们的 Coordinator
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )//請求绑定
      {
        // Light LED
        HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );//绑定成功
      }
#if defined( BLINK_LEDS )
      else
      {
        // Flash LED to show failure
        HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );//绑定失败
      }
#endif
      break;

    case Match_Desc_rsp:
      {
        ZDO_ActiveEndpointRsp_t *pRsp = ZDO_ParseEPListRsp( inMsg );
        if ( pRsp )
        {
          if ( pRsp->status == ZSuccess && pRsp->cnt )
          {
            GenericApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
            GenericApp_DstAddr.addr.shortAddr = pRsp->nwkAddr;
            // Take the first endpoint, Can be changed to search through endpoints
            GenericApp_DstAddr.endPoint = pRsp->epList[0];

            // Light LED
            HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
            
          }
          osal_mem_free( pRsp );
        }
      }
      break;
  }
}

/*********************************************************************
 * @fn      GenericApp_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_4
 *                 HAL_KEY_SW_3
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
static void GenericApp_HandleKeys( uint8 shift, uint8 keys )
{
  zAddrType_t dstAddr;

  // Shift is used to make each button/switch dual purpose.
  if ( shift )
  {
    if ( keys & HAL_KEY_SW_1 )
    {
    }
    if ( keys & HAL_KEY_SW_2 )
    {
    }
    if ( keys & HAL_KEY_SW_3 )
    {
    }
    if ( keys & HAL_KEY_SW_4 )
    {
    }
  }
  else
  {
    if ( keys & HAL_KEY_SW_1 )
    {
      // Since SW1 isn't used for anything else in this application...
#if defined( SWITCH1_BIND )
      // we can use SW1 to simulate SW2 for devices that only have one switch,
      keys |= HAL_KEY_SW_2;
#elif defined( SWITCH1_MATCH )
      // or use SW1 to simulate SW4 for devices that only have one switch
      keys |= HAL_KEY_SW_4;
#endif
    }

    if ( keys & HAL_KEY_SW_2 )
    {
      HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );

      // Initiate an End Device Bind Request for the mandatory endpoint
      dstAddr.addrMode = Addr16Bit;
      dstAddr.addr.shortAddr = 0x0000; // Coordinator
      ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(),
                            GenericApp_epDesc.endPoint,
                            GENERICAPP_PROFID,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                            FALSE );
    }

    if ( keys & HAL_KEY_SW_3 )
    {
    }

    if ( keys & HAL_KEY_SW_4 )
    {
      HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
      // Initiate a Match Description Request (Service Discovery)
      dstAddr.addrMode = AddrBroadcast;
      dstAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
      ZDP_MatchDescReq( &dstAddr, NWK_BROADCAST_SHORTADDR,
                        GENERICAPP_PROFID,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        GENERICAPP_MAX_CLUSTERS, (cId_t *)GenericApp_ClusterList,
                        FALSE );
    }
  }
}

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      GenericApp_MessageMSGCB
 *
 * @brief   Data message processor callback.  This function processes
 *          any incoming data - probably from other devices.  So, based
 *          on cluster ID, perform the intended action.
 *
 * @param   none
 *
 * @return  none
 */

int iRelayState = 0;

static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
 // unsigned char cGetDate[10];//获取协调器发来的命令
  switch ( pkt->clusterId )
  {
    case GENERICAPP_CLUSTERID:
      HalUARTWrite(0, pkt->cmd.Data, pkt->cmd.DataLength);  //!< 输出接收到的数据,通过串口发送
      //osal_memcpy(cGetDate,pkt->cmd.Data, pkt->cmd.DataLength);
      if(ENDNUM == 3)
      {
        Delay_ms(5);
        switch(controlRelay(pkt->cmd.Data))//!< 判断协调器是否对终端的继电器进行操作
      {
        case 0:
          RELAY  = 1;  //!< 继电器关闭状态
          iRelayState = 1;
          break;
        case 1:
          RELAY  = 0;  //!< 继电器打开状态
          iRelayState = 0;
          break;
        default:
          break;
      }
      break;
      }
      
  }
  if(ENDNUM == 4)
  {
    controlBeer(pkt->cmd.Data);
  }
  if(ENDNUM == 5)
  {
    controlMotor(pkt->cmd.Data);
  }

}

void controlBeer(unsigned char* v_MsgData)
{
  if(v_MsgData == NULL)
  {
    return;
  }
  if(v_MsgData[0] == 'y' && v_MsgData[1] == 'e' &&v_MsgData[2] == 's')
  {
    
    TIMER_START();        //!< 检测到异常气体/火焰时蜂鸣器响
    BEEP_PIN = 1;
    Delay_ms(500);
    TIMER_STOP();         //!< 气体正常/无火焰不响
    Delay_ms(500);
  }
  if(v_MsgData[0] == 'n' && v_MsgData[1] == 'o' &&v_MsgData[2] == 'o')
  {
    TIMER_STOP();         //!< 气体正常/无火焰不响
  }
}

int iMotorFlag = 1;

//!< 电机的判断
void controlMotor(unsigned char* v_MessageData)
{
  int i = 0;
  Delay_ms(50);   //!< 等待系统稳定
  if(v_MessageData[0] == 's' && v_MessageData[1] == 'h'&& v_MessageData[2] == 'u' && v_MessageData[3] == 'n')
  {
    //!< 执行顺转
    if(iMotorFlag == 1)
    {
      HalUARTWrite(0,"456SHUN\r\n",10);
      for(i=0;i<1000;i++)
      {
        MotorDoProcessedShun(ucSpeed); //!< 顺时针转动
      } 
      iMotorFlag = 0;
      MotorDoProcessedStop();        //!< 停止转动
      HalUARTWrite(0,"789SHUN\r\n",10);
     }
  }
  if(v_MessageData[0] == 'n' && v_MessageData[1] == 'i')
  {
    if(iMotorFlag == 0)
    {
      for(i=0;i<1000;i++)
      {
        MotorDoProcessedNi(5); //!< 顺时针转动
      } 
      iMotorFlag = 1;
      MotorDoProcessedStop();        //!< 停止转动 
    }
      
  }
}

/*********************************************************************
 * @fn      GenericApp_SendTheMessage
 *
 * @brief   Control relay switch .
 *
 * @param   unsigned char *
 *
 * @return  0 or 1
 *********************************************************************/
int controlRelay(unsigned char* v_MessageData)//!< 判断协调器的命令
{
  if(v_MessageData == NULL)  //!<  传进来的数据是否为空
  {
    return 0;
  }
  if(ENDNUM == 3)
  {
    if(v_MessageData[0] == '1')
    {
      return (openLed( v_MessageData));
    }
  }
  return 2;
}
//!< 控制led灯的开关
unsigned int openLed(unsigned char* v_MessageData)
{
    
    if((v_MessageData[1]== 'O' && v_MessageData[2]== 'N')||(v_MessageData[1]== 'o' && v_MessageData[2]== 'n')) //!< on打开
    {
      
      return 1;
    }
    else if((v_MessageData[1]== 'O' && v_MessageData[2]== 'F')||(v_MessageData[1]== 'o' && v_MessageData[2]== 'f'))//!< of关闭
    {
       return 0;
    }
    return 0;
}

/*********************************************************************
 * @fn      GenericApp_SendTheMessage
 *
 * @brief   Send "the" message.
 *
 * @param   none
 *
 * @return  none
 */

static void GenericApp_SendTheMessage( void )
{
 unsigned char i = 0;               //!<  用于循环
 unsigned char idstr[2] = {0};      //!<  存放设备id号
 unsigned char temp[4] = {0};       //!<  存放温度值
 unsigned char humi[4] = {0};       //!<  存放湿度值
 unsigned char szMsg[20] = {0};       //!<  封装数据发给协调器
 unsigned char strTemp[22] = {0};   //!<  串口打印出数据
 unsigned char volTemp[20] = {0};   //!<  显示采集的ADC
 unsigned char szSend[20] = {0};
 
 memset(idstr,0,2);
 memset(temp,0,4);
 memset(humi,0,4);
 memset(strTemp,0,22);
 memset(szMsg,0,20);
 memset(szSend,0,20);
  
 DHT11_TEST();//!<  温湿度值初始化
 
 if(ENDNUM == 1)
 {
   idstr[0] = ENDNUM + 0x30;
   idstr[1] = 0x00;
   temp[0]=wendu_shi+0x30;
   temp[1]=wendu_ge+0x30; 
   temp[2] = 0x00;
   humi[0]=shidu_shi+0x30;
   humi[1]=shidu_ge+0x30;
   humi[2]= 0x00;//把温湿度数据进行发送
    
   osal_memcpy(strTemp,"id:", 3); 
   osal_memcpy(&strTemp[3],idstr, 1);
   osal_memcpy(&strTemp[4],"temp:", 5); 
   osal_memcpy(&strTemp[9],temp, 2);
   osal_memcpy(&strTemp[11],"humi:", 5);
   osal_memcpy(&strTemp[16],humi, 2);
   osal_memcpy(&strTemp[18],"\n", 1);
   
   HalUARTWrite(0, "CSTX=>", 6);
   HalUARTWrite(0, strTemp, 19);
   HalUARTWrite(0, "\r\n",2);
     //输出到LCD显示
    for(i=0; i<3; i++)
    {   
      if(i==0)
      {
        LCD_P16x16Ch(i*16,4,i*16);
        LCD_P16x16Ch(i*16,6,(i+3)*16);
      }
      else
      {
        LCD_P16x16Ch(i*16,4,i*16);
        LCD_P16x16Ch(i*16,6,i*16);        
      }
    } 
    LCD_P8x16Str(44, 4, temp);
    LCD_P8x16Str(44, 6, humi);
    LCD_P8x16Str(88, 2, "ZB");
    LCD_P8x16Str(106, 2,idstr);
    
    
     szMsg[0] = ENDNUM+ 0x30;
     szMsg[1] = wendu_shi+ 0x30;
     szMsg[2] = wendu_ge+ 0x30;
     szMsg[3] = shidu_shi+ 0x30;  
     szMsg[4] = shidu_ge+ 0x30;
     szMsg[5] = GetADCMQ()/10+ 0x30;
     szMsg[6]= GetADCMQ()%10+ 0x30;            //!< 得到烟雾的指标
     szMsg[7]= 0;
     
      
      HalUARTWrite(0, szMsg, 20);
     memset(volTemp,0,20); 
     
     LCD_P16x16Ch(16*4+3,4,14*16);
     LCD_P16x16Ch(5*16+3,4,15*16);
     
     sprintf((char *)volTemp,":%02d",GetADCMQ());
     LCD_P8x16Str(6*16, 4, volTemp);
    }
    if(ENDNUM == 2)
    {
         //显示采集的ADC
     LCD_P8x16Str(16*6-5, 2, "TWO");
     
     memset(szMsg, 0,20);
     szMsg[0] = ENDNUM+ 0x30;
     szMsg[1] = (100-getFrame())/10+ 0x30;
     szMsg[2] = (100-getFrame())%10+ 0x30;
     szMsg[3] = GetADCMQ()/10+ 0x30;
     szMsg[4] = GetADCMQ()%10+ 0x30;
     szMsg[5] = 0;
      memset(volTemp,0,20); 
      
      LCD_P16x16Ch(0,4,4*16);
      LCD_P16x16Ch(16,4,5*16);
      LCD_P16x16Ch(32,4,2*16);
      //LCD_P8x16Str(44, 4, volTemp);
     sprintf((char *)volTemp,"%02d",(unsigned int)(100-getFrame()));
     LCD_P8x16Str(44,4, volTemp);
     memset(volTemp,0,20);
     
     sprintf((char *)volTemp,"A Q I: %02d",(unsigned int)(GetADCMQ()));
     LCD_P8x16Str(0,6, volTemp);
     HalUARTWrite(0, szMsg, 20);
    }
    if(ENDNUM == 3)
    {
      memset(szMsg, 0,20);
     szMsg[0] = ENDNUM+ 0x30;
     szMsg[1] = iRelayState + 0x30;
     szMsg[2] = HCSR501() + 0x30;
     szMsg[3] = 0;
     
    LCD_P16x16Ch(0,4,6*16);
    LCD_P16x16Ch(16,4,7*16);
    LCD_P16x16Ch(16*2,4,8*16);
    LCD_P16x16Ch(16*3,4,9*16);
    LCD_P16x16Ch(16*4,4,2*16);
    
    LCD_P16x16Ch(0,6,10*16);
    LCD_P16x16Ch(16,6,11*16);
    LCD_P16x16Ch(16*2,6,12*16);
    LCD_P16x16Ch(16*3,6,13*16);
    LCD_P16x16Ch(16*4,6,2*16);
   if(iRelayState == 1)
   {
      LCD_P8x16Str(16*5,4, "ON");
   }
   if(iRelayState == 0)
   {
      LCD_P8x16Str(16*5,4, "OFF");
   }
    
     HalUARTWrite(0, szMsg, 20);
    }
    if ( AF_DataRequest( &GenericApp_DstAddr, &GenericApp_epDesc,
                         GENERICAPP_CLUSTERID,
                         (byte)osal_strlen((char *)szMsg ) ,
                         (byte *)&szMsg,
                         &GenericApp_TransID,
                         AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
    {
      // Successfully requested to be sent.
    }
    else
    {
      // Error occurred in request to send.
    }
}

//!< 红外传感器的获取
int HCSR501(void)
{
  int iState = 0;
  MicroWait(1000);
  if(DATA_PIN == 1)
  {
    if(DATA_PIN == 1)
    {
      iState = 1;
      //HalLcdWriteString( "insecurity", HAL_LCD_LINE_3 ); //LCD显示
      LCD_P8x16Str(16*5,6, "YES");
    }
  }
  else
  {
    iState = 0;
    LCD_P8x16Str(16*5,6, "NO");
  }
  return iState;
}

//!< 获取火焰传感器的状态
unsigned char percentVOL=0;//百分比的整数值

unsigned char getFrame(void)
{
  uint16 adc=0;
  float vol=0.0; //adc采样电压  
  //unsigned char volTemp[20];
  //读MQ2浓度 0-8192
  adc= HalAdcRead(HAL_ADC_CHANNEL_4, HAL_ADC_RESOLUTION_14);

  //最大采样值8192(因为最高位是符号位)
  //2的13次方=8192
  if(adc>=8192)
  {
    return 0;
  }

  //转化为百分比 8189/8192=0.9996
  vol=(float)((float)adc)/8192.0;
     
  //取百分比两位数字
  percentVOL=(int)(vol*100);
  if(percentVOL<10)
    percentVOL=1;
  if(percentVOL>99)
     percentVOL=99;
  //sprintf((char *)volTemp,"ADC:%d",percentVOL);
  //LCD_P8x16Str(80, 6, volTemp);
  
  return percentVOL;
}

/**********************************************************************/
//!<  通道5获取ADC值   //!<  使用的Px_5引脚
unsigned char percentADCMQ=0;//!<  百分比的整数值
unsigned char GetADCMQ(void)
{
  uint16 adc=0;
  float vol=0.0; //!<  adc采样电压  
  //!<  读MQ2浓度 得到0-8192
  adc= HalAdcRead(HAL_ADC_CHANNEL_5, HAL_ADC_RESOLUTION_14);

  //!<  最大采样值8192(因为最高位是符号位)
  //2的13次方=8192
  if(adc>=8192)
  {
    return 0;
  }
  //转!<  化为百分比 8189/8192=0.9996
  vol=(float)((float)adc)/8192.0;
  //!<  取百分比两位数字
  percentADCMQ=(int)(vol*100);
  if(percentADCMQ<10)
    percentADCMQ=1;
  if(percentADCMQ>99)
     percentADCMQ=99;
  return percentADCMQ;
}

void MotorDoProcessedData(uchar v_ucdata) //!< 点击转动处理过程
{
  A = 1&(v_ucdata>>4);
  B = 1&(v_ucdata>>5);
  C = 1&(v_ucdata>>6);
  D = 1&(v_ucdata>>7);
}

//!< 顺时针转动
void MotorDoProcessedShun(uchar Speed)
{
  uchar i;
  for(i=0;i<4;i++)
  {
    MotorDoProcessedData(funcForward[i]);
    Delay_ms(Speed);//!< 转速调节
  }
}
//!< 逆时针转动
void MotorDoProcessedNi(uchar Speed)
{
  uchar i;
  for(i=0;i<4;i++)
  {
    MotorDoProcessedData(funcReverse[i]);
    Delay_ms(Speed);//!< 转速调节
  }
}
//停止转动
void MotorDoProcessedStop(void)
{
  MotorDoProcessedData(0x00);
}

void InitIO(void)//!< IO引脚初始化工作
{
	//!< P04 05 06 07定义为普通IO
	P0SEL &= 0x0F;  
	//!< P04 05 06 07定义为输出
	P0DIR |= 0xF0;  
}

void Init_Beep(void)
{
    P0SEL |= 0x80;          //!< 设置P0.7口为外设
    P0DIR &= 0x80;          //!< 设置P0.7为输出IO口
    PERCFG |= 0x40;         //!< 设置定时器1 的I / O 位置   1： 备用位置2
}

//!< 将基准值放入T1CC0 寄存器, 将被比较值放入T1CC3寄存器
//!< 当T1CC3中的值与T1CC0中的值相等时，则T1CC3 设置or清除
void InitTimer(void)
{
    T1CC0L = 0xff;         //!< PWM duty cycle  周期
    T1CC0H = 0x0;

    T1CC3L = 0x30;        //!< PWM signal period 占空比
    T1CC3H = 0x00;

    //!< 等于T1CC0中的数值时候，输出高电平 1； 等于T1CC3中的数值时候，输出低电平 0
    //!< 其实整个占空比就为50%  为了蜂鸣器输出连续的响声修改了占空比
    T1CCTL3 = 0x34;
    T1CTL |= 0x0f;         //!< divide with 128 and to do i up-down mode
}

void startPwm(void)
{
    Init_Beep();
    InitTimer();
}

/**********************************************************************/

#if defined( IAR_ARMCM3_LM )
/*********************************************************************
 * @fn      GenericApp_ProcessRtosMessage
 *
 * @brief   Receive message from RTOS queue, send response back.
 *
 * @param   none
 *
 * @return  none
 */
static void GenericApp_ProcessRtosMessage( void )
{
  osalQueue_t inMsg;

  if ( osal_queue_receive( OsalQueue, &inMsg, 0 ) == pdPASS )
  {
    uint8 cmndId = inMsg.cmnd;
    uint32 counter = osal_build_uint32( inMsg.cbuf, 4 );

    switch ( cmndId )
    {
      case CMD_INCR:
        counter += 1;  /* Increment the incoming counter */
                       /* Intentionally fall through next case */

      case CMD_ECHO:
      {
        userQueue_t outMsg;

        outMsg.resp = RSP_CODE | cmndId;  /* Response ID */
        osal_buffer_uint32( outMsg.rbuf, counter );    /* Increment counter */
        osal_queue_send( UserQueue1, &outMsg, 0 );  /* Send back to UserTask */
        break;
      }
      
      default:
        break;  /* Ignore unknown command */    
    }
  }
}
#endif

/*********************************************************************
 */
