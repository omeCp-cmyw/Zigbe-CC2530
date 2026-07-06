#ifndef __IOT_H__
#define __IOT_H__

#define uchar unsigned char

/*********************************
* 下列参数根据实际修改
*  ENDNUM       - 设备号
*  PRODUCT_ID   - OneNET产品ID
*  DEVICE_NAME  - OneNET设备名称
*  ACCESS_KEY   - 用于生成token签名
*  ONENET_TOKEN - 预生成的token(从平台获取，建议设置较长过期时间)
*  LYSSID       - 路由器SSID
*  LYPASSWD     - 路由器密码
*********************************/

#define  ENDNUM 3 //定义设备号 

/* OneNET物模型新版API配置 */
#define  PRODUCT_ID    "gZSY9iX0tP"                //!< OneNET产品ID
#define  DEVICE_NAME   "temp"                       //!< OneNET设备名称
#define  ACCESS_KEY    "o0trd0lLvyPhqUPFRdmRHbllH1tj82nph9E97yFXz2s="  //!< 访问密钥

/* Token - 从OneNET平台生成，格式：version=2018-10-31&res=...&et=...&method=md5&sign=...
 * 注意：token有过期时间(et字段)，请在平台设置较长的过期时间
 * 如果token过期，需要重新从平台生成并替换此处 */
#define  ONENET_TOKEN  "version=2018-10-31&res=products%2FgZSY9iX0tP%2Fdevices%2Ftemp&et=1780918555&method=md5&sign=4KqqNF59z77rYCafkNpS2Q%3D%3D"

/* WiFi路由器配置 */
#define  LYSSID   "YanWei"                      //!< 修改你路由器的SSId
#define  LYPASSWD "2233445566"                       //!< 修改你路由器的密码

/* OneNET物模型API端点(HTTP，适合CC2530等嵌入式设备) */
#define  ONENET_HOST   "open.iot.10086.cn"              //!< API服务器IP
#define  ONENET_PORT   80                            //!< API服务器端口
#define  ONENET_PATH   "/fuse/http/device/thing/property/post"  //!< 物模型属性上报路径

/* 物模型属性标识符 - 需在OneNET平台物模型中定义对应属性 */
#define  PROP_TEMP      "temp"                       //!< 温度属性
#define  PROP_HUMI      "humi"                       //!< 湿度属性
#define  PROP_MQ2       "MQ_2"                       //!< 烟雾传感器属性
#define  PROP_FLAME     "flame"                      //!< 火焰传感器属性
#define  PROP_AQI       "indoorAqi"                  //!< 空气质量属性
#define  PROP_RELAY     "Relay"                      //!< 继电器状态属性
#define  PROP_HCSR501   "HC-SR501"                   //!< 人体红外属性

////////函数声明/////////////

void UartInitPort0(void);
void UartInitPort1(halUARTCBack_t SerialCallBack);
void SerialCallBack(uint8 port, uint8 event);
void IOT_WiFiInit(void);
void IOT_RecdataPut(void);
void IotConntect(void);

/* OLED显示函数（实现在hal_lcd.c） */
extern void LCD_P8x16Str(unsigned char x, unsigned char y, unsigned char ch[]);
extern void LCD_P16x16Ch(unsigned char x, unsigned char y, unsigned char N);
extern void HalLcdDisplayPercentBar(char *title, uint8 value);

typedef struct                        //!< 操作WiFi的结构体
{
    unsigned char Status;             //!< 流程步骤号
    unsigned char Recflag;            //!< 收到数据标志
    unsigned char Reclen;             //!< 收到的数据长度
    unsigned char Recbuffer[20];      //!< 接收反馈回的信息
} stWiFi;
extern stWiFi WiFiStatus;
extern unsigned char uchBuf[10];

#endif