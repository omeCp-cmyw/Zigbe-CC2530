#include <ioCC2530.h>
#include "OnBoard.h"
#include "IOT.h"
#include "delay.h"
#include "ZComDef.h"
#include "ZGlobals.h"
#include "OSAL.h"
#include "MT.h"
#include "MT_SYS.h"
#include "DebugTrace.h"

/* Hal */
#include "hal_lcd.h"
#include "hal_mcu.h"
#include "hal_timer.h"
#include "hal_key.h"
#include "hal_led.h"
#include "hal_uart.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
/* Allow access macRandomByte() */
#include "mac_radio_defs.h"
unsigned char uartbuf2[200]={0x00};
unsigned char uchBuf[10] = {0x00};
stWiFi WiFiStatus;


void UartInitPort0(void )
{

          halUARTCfg_t uartConfig;//顶一个串口结构体
          uartConfig.configured             =TRUE;//串口配置为真
          uartConfig.baudRate               =HAL_UART_BR_115200;//波特率为115200
          uartConfig.flowControl            =FALSE;//流控制为假
          uartConfig.callBackFunc       =    rxCB;
          HalUARTOpen(HAL_UART_PORT_0,&uartConfig);// 打开串口0
}
//下面这个回调函数是我加的，从串口0读取n个字符，放进uartbuf
static void rxCB(uint8 port,uint8 event)
{
  unsigned char index=0;
  unsigned char uartbuf[128];

  memset(uartbuf, 0, sizeof(uartbuf));
  while (Hal_UART_RxBufLen(0) && index < sizeof(uartbuf)) //!< 接收数据，防止缓冲区溢出
  {
    HalUARTRead(0, &uartbuf[index], 1);
    index++;
  }
  if(index > 0 && index < sizeof(uartbuf))
  {
    unsigned char copyLen = (index < sizeof(uchBuf)) ? index : sizeof(uchBuf) - 1; //!< 防止溢出
    osal_memcpy(uchBuf, uartbuf, copyLen);
    uchBuf[copyLen] = '\0';
  }
}

void UartInitPort1( halUARTCBack_t SerialCallBack )//配置USART1接到WiFi模块
{
  halUARTCfg_t uartConfig;

  // configure UART
  uartConfig.configured           = TRUE;
  uartConfig.configured             =TRUE;//串口配置为真
  uartConfig.baudRate               =HAL_UART_BR_115200;//波特率为115200
  uartConfig.flowControl            =FALSE;//流控制为假
  uartConfig.callBackFunc         = SerialCallBack;
  // start UART
  // Note: Assumes no issue opening UART port.
  HalUARTOpen( HAL_UART_PORT_1, &uartConfig );

}


//串口接收函数 大部分的时候都在这里面做 模块的反馈信息
//下面这个回调函数是我加的，从串口1读取n个字符，放进uartbuf
 void SerialCallBack(uint8 port,uint8 event)
{
  unsigned char unflag=0, j=0;
  while (Hal_UART_RxBufLen(1) && j < sizeof(uartbuf2) - 1) //接收数据，防止缓冲区溢出
  {
    HalUARTRead(1, &uartbuf2[j], 1);
    j++;
    unflag = 1;
  }
  uartbuf2[j] = '\0'; //确保字符串终止

  if(unflag)
  {
    IOT_RecdataPut();          //!< 对模块接收到的数据进行处理
    HalUARTWrite(0, uartbuf2, j); //!< 将接收到的数据打印出来
    WiFiStatus.Reclen = j;     //!< 获取接收到的数据长度
  }
 }
//unsigned char isBUSY=0; //是否切换到手动模式       
void IOT_WiFiInit(void)//初始化WiFi
{ 
  switch(WiFiStatus.Status)
  {
    case 0:
      HalUARTWrite(1,"AT\r\n",osal_strlen("AT\r\n"));//配置发AT命令
      HalUARTWrite(0,"\r\n**CSGSM Connect To MODULE...\r\n",32); //串口提示正在连接模块
    break;
     case 1:
      HalUARTWrite(1,"AT+CWMODE=3\r\n",osal_strlen("AT+CWMODE=3\r\n"));//设置模式，客户端模式
      HalUARTWrite(0,"Set WiFi ModuleAP+STA..\r\n",25);
    break;
      case 2:
      HalUARTWrite(1,"AT+CIPMODE=1\r\n",osal_strlen("AT+CIPMODE=1\r\n"));//配置发AT命令 设置透传模式CS
      HalUARTWrite(0,"Set WiFi Module transparent\r\n",29); //串口提示正在连接模块
    break;
  }
}

/*
LYSSID：路由器的账号
LYPASSWD：路由器密码
*/
void IotConntect(void)//初始化WiFi模块
{
  unsigned char SendATData[200];
    
  switch(WiFiStatus.Status)
  {
     case 3:
      memset(SendATData,0,200);
      sprintf((char *)SendATData,"AT+CWJAP=\"%s\",\"%s\"\r\n",LYSSID,LYPASSWD);//链接到路由器
      HalUARTWrite(1,SendATData,osal_strlen((char *)SendATData));
      HalUARTWrite(0,SendATData,osal_strlen((char *)SendATData));
      HalUARTWrite(0,"Connect To WiFi Route...\r\n",26);
      break;
    case 4:
      //设置登录到OneNet服务器（使用IOT.h中定义的ONENET_HOST/ONENET_PORT宏）
      memset(SendATData,0,200);
      sprintf((char *)SendATData,"AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",ONENET_HOST,ONENET_PORT);
      HalUARTWrite(1,SendATData,osal_strlen((char *)SendATData));
      break;
    case 5:
      HalUARTWrite(1, "AT+CIPSEND\r\n", 12); //!< 进入透传发送模式
      break;  
  }
}


void IOT_RecdataPut(void)//对WiFi回的数据内容进行解析
{ 
  char *strx;
 switch(WiFiStatus.Status){
    case 0:
      strx=strstr((const char*)uartbuf2,(const char*)"OK");//返回OK，表明通讯正常c s t x
       if(strx)
       {
         LCD_P8x16Str(0, 4, "CONNET MODULE OK");
         WiFiStatus.Status=1;
       }
      break;
     case 1:
      strx=strstr((const char*)uartbuf2,(const char*)"OK");//设置下模式3
       if(strx){
         LCD_P8x16Str(0, 4, "SET MODE 3  [OK]");
         WiFiStatus.Status=2;
       }
       break;  
     case 2:
      strx=strstr((const char*)uartbuf2,(const char*)"OK");//设置模块为 透传模式 发送json到oennet
       if(strx){
         LCD_P8x16Str(0, 4, "TRANSPARENT [OK]");
         WiFiStatus.Status=3;
       } 
      break;  
    case 3://判断登录服务器情况
        strx=strstr((const char*)uartbuf2,(const char*)"OK");//CSTX链接到路由器反馈，要上外网必须让WiFi去链接到路由器
       if(strx){
         LCD_P8x16Str(0, 4, "CONNECT Route OK");
         WiFiStatus.Status=4;//此时需要发布登录
       } 
      break;   
     case 4://查看登录情况返回
       strx=strstr((const char*)uartbuf2,(const char*)"OK");//ONENET登录成功，可以发布订阅数据了
       if(strx){
          LCD_P8x16Str(0, 4, "CONNECT CLOUD OK");
          WiFiStatus.Status=5;//此时就可以发数据了
       } 
      break;
     case 5://查看发布数据情况
     strx=strstr((const char*)uartbuf2,(const char*)"OK");//发布数据成功c s t x
     if(strx){
        LCD_P8x16Str(0, 4, "SEND CLOUD DATA.");
        WiFiStatus.Status=6;//此时就可以发数据了
     }else{
        LCD_P8x16Str(0, 4, "SEND CLOUD DATA*");
        WiFiStatus.Status=6;//此时就可以发数据了
     }
    break;
  }   
}