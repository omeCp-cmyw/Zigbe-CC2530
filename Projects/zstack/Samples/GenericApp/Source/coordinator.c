/******************************************************************************
  Filename:       GenericApp.c
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
#include "delay.h"
#include "GenericApp.h"
#include "DebugTrace.h"

#if !defined( WIN32 )
  #include "OnBoard.h"
#endif

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_uart.h"
#include "IOT.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* RTOS */
#if defined( IAR_ARMCM3_LM )
#include "RTOS_App.h"
#endif  
extern stWiFi WiFiStatus;
//extern unsigned char isBUSY; //是否进入手动控制


extern void LCD_P8x16Str(unsigned char x, unsigned char y,unsigned char ch[]);
extern void HalLcdDisplayPercentBar( char *title, uint8 value );
extern void LCD_P16x16Ch(unsigned char x, unsigned char y, unsigned char N);
/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */
/**********************
beep蜂鸣器
***********************/
#define BEEP_PIN     P0_7  //!< 定义P0.7口为蜂鸣器的输出端

#define TIMER_START() T1CTL|=0X03   //!< 启动定时器
#define TIMER_STOP() T1CTL&=~0X03   //!< 停止定时器
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

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
byte GenericApp_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // GenericApp_Init() is called.
devStates_t GenericApp_NwkState;


byte GenericApp_TransID;  // This is the unique message ID (counter)

afAddrType_t GenericApp_DstAddr;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void GenericApp_ProcessZDOMsgs( zdoIncomingMsg_t *inMsg );
static void GenericApp_HandleKeys( byte shift, byte keys );
static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
static void GenericApp_SendTheMessage( void );
void ReSetWiFi(void);
//!< 蜂鸣器

void Init_Beep(void);
void InitTimer(void);
void startPwm(void);
void judgeData(unsigned char v_uData);

#if defined( IAR_ARMCM3_LM )
static void GenericApp_ProcessRtosMessage( void );
#endif


#define WIFIRESET       P0_6          // P0.6口控制WiFi的reset
/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */


/****************************************************************************
* 名    称: ReSetWiFi()
* 功    能: 低电平 WiFi模块
* 入口参数: 无
* 出口参数: 无
****************************************************************************/
void ReSetWiFi(void)
{
  P0DIR |= 0x40;                  //P0.6定义为输出
   
  WIFIRESET = 0;                  //低电平复位---------------------
  Delay_ms(1000);
  WIFIRESET = 1;                  //高电平工作------------
  Delay_ms(2000);
}


/*********************************************************************
 * PUBLIC FUNCTIONS
 */

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
  //unsigned char tmp[10];
  GenericApp_TaskID = task_id;
  GenericApp_NwkState = DEV_INIT;
  GenericApp_TransID = 0;
  
  startPwm();             //!< 配置T1输出PWM
  TIMER_STOP();          //!< 默认关闭蜂鸣器
    // Update the display
#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "IOT1892_YANWEI", HAL_LCD_LINE_1 );
#endif
  
  ReSetWiFi();  //!< 重启WiFi模块
  /*LED2 = 1; //关灯
  LED1 = 1;*/
  UartInitPort0();                     //!< 初始化串口0打印调试信息
  UartInitPort1(SerialCallBack);       //!< 初始化串口1对接4G或者NBIOT模块或者WiFi模块
  WiFiStatus.Status=0;                 //!< WiFi模块中操作的标志位
  //HalUARTWrite(1, "AT+RST\r\n", 8);
  
  //!< 设置为广播模式
  GenericApp_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
  GenericApp_DstAddr.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_DstAddr.addr.shortAddr = 0xFFFF;
  
  // Fill out the endpoint description.
  GenericApp_epDesc.endPoint = GENERICAPP_ENDPOINT;
  GenericApp_epDesc.task_id = &GenericApp_TaskID;
  GenericApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&GenericApp_SimpleDesc;
  GenericApp_epDesc.latencyReq = noLatencyReqs;

  // Register the endpoint description with the AF
  afRegister( &GenericApp_epDesc );//!< 这里是注册自身模块的端点与应用层任务进行联系

  // Register for all key events - This app will handle all key events
  RegisterForKeys( GenericApp_TaskID );



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

uint16 GenericApp_ProcessEvent( uint8 task_id, uint16 events )//!< 任务时间处理函数
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

        case AF_INCOMING_MSG_CMD://!< 有数据接收则执行GenericApp_MessageMSGCB()
          GenericApp_MessageMSGCB( MSGpkt );//!< 有数据包进来，会进入这个事件
          break;

        case ZDO_STATE_CHANGE://!< 网络状态的的改变事件
          GenericApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if ( (GenericApp_NwkState == DEV_ZB_COORD) //!< 响应回终端等
              || (GenericApp_NwkState == DEV_ROUTER)
              || (GenericApp_NwkState == DEV_END_DEVICE) )
          {
            // Start sending "the" message in a regular interval.
            osal_start_timerEx( GenericApp_TaskID,
                                GENERICAPP_SEND_MSG_EVT,
                                GENERICAPP_SEND_MSG_TIMEOUT_1);
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
  if ( events & GENERICAPP_SEND_MSG_EVT )
  {
    
    IOT_WiFiInit();                //!< WiFi模块的配置初始化
    IotConntect();            //IOT MQTT初始化 链接阿里云的初始化
    //isBUSY= !isBUSY;
    GenericApp_SendTheMessage();//广播数据发送
    
    osal_start_timerEx( GenericApp_TaskID,
                        GENERICAPP_SEND_MSG_EVT,
                        GENERICAPP_SEND_MSG_TIMEOUT_1 );

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
    case End_Device_Bind_rsp:
      if ( ZDO_ParseBindRsp( inMsg ) == ZSuccess )
      {
        // Light LED
        HalLedSet( HAL_LED_4, HAL_LED_MODE_ON );
      }
#if defined( BLINK_LEDS )
      else
      {
        // Flash LED to show failure
        HalLedSet ( HAL_LED_4, HAL_LED_MODE_FLASH );
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
      ZDP_EndDeviceBindReq( &dstAddr, NLME_GetShortAddr(),//获取短地址
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
int flagCount=0;//用来统计收到次数
unsigned int  id=66,temp=0,humi=0,MQ_2 = 0,flame=0,uiAQI = 0,iRelay = 0,iSec = 0; //分别从终端发送过来的传感器数据
static void GenericApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
     char RevData[256];
     char strTemp[256];
     switch ( pkt->clusterId )//这个就是簇，必须要保证发来的簇与要判断的簇保持一致才可以
     { 

      case GENERICAPP_CLUSTERID:  //!< 收到数据进行解析
        if(pkt->cmd.Data == NULL) //!< 判断是否有数据
        {
          return;
        }
        id=pkt->cmd.Data[0]-48;
        memset(strTemp,0,256);
        //HalUARTWrite(0,pkt->cmd.Data,pkt->cmd.DataLength);
        LCD_P8x16Str(0, 4, "Get EndD Data ..");
        if(pkt->cmd.DataLength < 1)
          return ;
        //LCD_P8x16Str(0, 4, "Send NET Data ..");
        flagCount++;             //!< 统计接收到的次数
        if(flagCount>3000)       //!< 差不多每天重启一次
        {
          flagCount=0;
          Onboard_soft_reset();
        }
        if(id == 1)
        {
          temp=((pkt->cmd.Data[1]-48)*10 + (pkt->cmd.Data[2]-48));    //!<  温度
          humi=((pkt->cmd.Data[3]-48)*10 + (pkt->cmd.Data[4]-48));    //!<  湿度
          MQ_2=((pkt->cmd.Data[5]-48)*10 + (pkt->cmd.Data[6]-48));    //!<  烟雾传感器
        }
        if(id == 2)
        {
          flame=((pkt->cmd.Data[1]-48)*10 + (pkt->cmd.Data[2]-48));
          uiAQI = ((pkt->cmd.Data[3]-48)*10 + (pkt->cmd.Data[4]-48));
        }
        if(id == 3)
        {
          iRelay = ((pkt->cmd.Data[1]-48));
          iSec = ((pkt->cmd.Data[2]-48));
        }
        //flame=pkt->cmd.Data[4]; //!<  火焰传感器
        
        
         
        memset(RevData,0,256);      //统计组成字符串液晶屏显示
        sprintf(RevData,"ZB%d->T:%d,H:%d",id,temp,humi);
        LCD_P8x16Str(0, 6, (unsigned char *)RevData);
          
         if(WiFiStatus.Status==6)
        { 
            /* ============================================================
             * OneNET物模型属性上报（新版API）
             * 请求URL: POST /fuse/http/device/thing/property/post?topic=...
             * 认证方式: token（MD5签名）
             * JSON格式: {"id":"..","version":"1.0","params":{...}}
             * ============================================================ */

            //!< 1. 先构建JSON请求体（物模型格式）
            memset(strTemp,0,256);
            if(id == 1)
            {
              sprintf(strTemp,
                "{\"id\":\"%d\",\"version\":\"1.0\",\"params\":{"
                "\"%s\":{\"value\":%d},"
                "\"%s\":{\"value\":%d},"
                "\"%s\":{\"value\":%d}"
                "}}",
                flagCount,
                PROP_TEMP,(unsigned int)temp,
                PROP_HUMI,(unsigned int)humi,
                PROP_MQ2,(unsigned int)MQ_2);
            }
            if(id == 2)
            {
              sprintf(strTemp,
                "{\"id\":\"%d\",\"version\":\"1.0\",\"params\":{"
                "\"%s\":{\"value\":%d},"
                "\"%s\":{\"value\":%d}"
                "}}",
                flagCount,
                PROP_FLAME,flame,
                PROP_AQI,uiAQI);
            }
            if(id == 3)
            {
              sprintf(strTemp,
                "{\"id\":\"%d\",\"version\":\"1.0\",\"params\":{"
                "\"%s\":{\"value\":%d},"
                "\"%s\":{\"value\":%d}"
                "}}",
                flagCount,
                PROP_RELAY,iRelay,
                PROP_HCSR501,iSec);
            }

            //!< 2. 发送HTTP请求行（物模型属性上报URL）
            memset(RevData,0,256);
            sprintf(RevData,
              "POST %s?topic=$sys/%s/%s/thing/property/post&protocol=http HTTP/1.1\r\n",
              ONENET_PATH, PRODUCT_ID, DEVICE_NAME);
            HalUARTWrite(1,(unsigned char *)RevData,osal_strlen(RevData));

            //!< 3. 发送HTTP请求头（token认证 + Content-Type）
            memset(RevData,0,256);
            sprintf(RevData,
              "Host: open.iot.10086.cn\r\n"
              "Content-Type: application/json\r\n"
              "token: %s\r\n"
              "Content-Length: %d\r\n\r\n",
              ONENET_TOKEN, osal_strlen(strTemp));
            HalUARTWrite(1,(unsigned char *)RevData,osal_strlen(RevData));

            //!< 4. 发送JSON请求体
            HalUARTWrite(1,(unsigned char *)strTemp,osal_strlen(strTemp));

            //!< 5. 调试信息输出到串口0
            memset(RevData,0,256);
            sprintf(RevData,"\n[END%d] %s\n",id,strTemp);
            HalUARTWrite(0,(unsigned char *)RevData,osal_strlen(RevData));
            LCD_P8x16Str(0, 4, "Send NET Data ..");
         }
      break;
     }
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
static void GenericApp_SendTheMessage( void )//!< 发命令给终端们，对其进行控制
{
  if(strlen((char *)uchBuf) != 0)//!< 判断是否有发送命令
  {
    if(AF_DataRequest( &GenericApp_DstAddr, &GenericApp_epDesc,
                             GENERICAPP_CLUSTERID,
                              10,
                             (unsigned char *)uchBuf,
                             &GenericApp_TransID,
                             AF_DISCV_ROUTE, AF_DEFAULT_RADIUS)== afStatus_SUCCESS )
    {
      osal_memset(uchBuf,0,strlen((char *)uchBuf));//!< 发完命令将数组清空，以便下一次使用
    }
  }
}

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
