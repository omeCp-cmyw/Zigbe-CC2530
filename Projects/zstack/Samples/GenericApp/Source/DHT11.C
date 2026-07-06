/****************************************************************************
 * DHT11 温湿度传感器驱动
 * 平台：CC2530
 * 数据引脚：P0_7（通过宏可修改）
 * 协议：单总线（One-Wire）
 ****************************************************************************/
#include <ioCC2530.h>
#include "OnBoard.h"
#include "delay.h"
#include "DHT11.H"

#define uint  unsigned int
#define uchar unsigned char

/* 数据引脚定义（根据硬件实际连接修改） */
#define DHT11_PIN  P0_7

/* 超时计数上限（防止引脚卡死导致死循环） */
#define DHT11_TIMEOUT  200

/* 温湿度解析结果（外部通过DHT11.H访问） */
uchar shidu_shi = 0;   //!< 湿度十位
uchar shidu_ge  = 0;   //!< 湿度个位
uchar wendu_shi = 0;   //!< 温度十位
uchar wendu_ge  = 0;   //!< 温度个位

/****************************************************************************
 * @brief  从DHT11读取一个字节（高位在前）
 *         每位数据以低电平起始，高电平持续时间区分0/1：
 *         - 低电平约26~28us + 高电平约26~28us → 数据0
 *         - 低电平约26~28us + 高电平约70us    → 数据1
 * @return 读取到的字节数据
 ****************************************************************************/
static uchar DHT11_ReadByte(void)
{
    uchar i;
    uchar byte = 0;
    uchar timeout;

    for(i = 0; i < 8; i++)
    {
        /* 等待低电平结束（DHT11每位的起始低电平） */
        timeout = 0;
        while(!DHT11_PIN && timeout < DHT11_TIMEOUT)
            timeout++;
        if(timeout >= DHT11_TIMEOUT) return 0;  //!< 超时退出

        /* 延时约30us，跳过低电平阶段，进入高电平采样窗口 */
        Delay_10us();
        Delay_10us();
        Delay_10us();

        /* 采样：此时若为高电平则该位为1 */
        byte <<= 1;
        if(DHT11_PIN)
            byte |= 0x01;

        /* 等待高电平结束 */
        timeout = 0;
        while(DHT11_PIN && timeout < DHT11_TIMEOUT)
            timeout++;
        if(timeout >= DHT11_TIMEOUT) break;     //!< 超时退出
    }
    return byte;
}

/****************************************************************************
 * @brief  DHT11温湿度采集主函数
 *         调用后结果存入全局变量：wendu_shi/wendu_ge, shidu_shi/shidu_ge
 *         通信流程：
 *         1. 主机拉低>=18ms → 拉高等待20~40us
 *         2. DHT11应答：拉低80us → 拉高80us
 *         3. DHT11发送40位数据：湿度H + 湿度L + 温度H + 温度L + 校验和
 *         4. 校验和 = 前4字节之和的低8位
 ****************************************************************************/
void DHT11_TEST(void)
{
    uchar rhH, rhL, tempH, tempL, checksum;
    uchar sum;

    /* ---- 1. 主机发送起始信号 ---- */
    P0DIR |= 0x80;          //!< P0.7设为输出
    DHT11_PIN = 0;          //!< 拉低数据线
    Delay_ms(20);           //!< 保持低电平>=18ms（DHT11要求）
    DHT11_PIN = 1;          //!< 释放数据线

    /* ---- 2. 切换为输入，等待DHT11应答 ---- */
    P0DIR &= ~0x80;        //!< P0.7设为输入
    Delay_10us();           //!< 延时约40us（20~40us窗口）
    Delay_10us();
    Delay_10us();
    Delay_10us();

    /* ---- 3. 检测DHT11应答信号（80us低+80us高） ---- */
    if(DHT11_PIN != 0)      //!< 无应答，读取失败
    {
        wendu_shi = 0;
        wendu_ge  = 0;
        shidu_shi = 0;
        shidu_ge  = 0;
        P0DIR |= 0x80;     //!< 恢复为输出
        return;
    }

    /* 等待应答低电平结束（约80us） */
    {
        uchar timeout = 0;
        while(!DHT11_PIN && timeout < DHT11_TIMEOUT) timeout++;
    }
    /* 等待应答高电平结束（约80us） */
    {
        uchar timeout = 0;
        while(DHT11_PIN && timeout < DHT11_TIMEOUT) timeout++;
    }

    /* ---- 4. 依次读取5个字节（40位数据） ---- */
    rhH      = DHT11_ReadByte();  //!< 湿度整数部分
    rhL      = DHT11_ReadByte();  //!< 湿度小数部分
    tempH    = DHT11_ReadByte();  //!< 温度整数部分
    tempL    = DHT11_ReadByte();  //!< 温度小数部分
    checksum = DHT11_ReadByte();  //!< 校验和

    /* ---- 5. 释放总线 ---- */
    P0DIR |= 0x80;         //!< 恢复P0.7为输出
    DHT11_PIN = 1;         //!< 拉高数据线

    /* ---- 6. 校验数据 ---- */
    sum = rhH + rhL + tempH + tempL;
    if(sum != checksum)     //!< 校验失败，丢弃本次数据
    {
        return;
    }

    /* ---- 7. 解析结果 ---- */
    wendu_shi = tempH / 10;
    wendu_ge  = tempH % 10;
    shidu_shi = rhH / 10;
    shidu_ge  = rhH % 10;
}

/****************************************************************************
 * @brief  DHT11读取接口（兼容router.c中的调用）
 *         功能等同于DHT11_TEST()
 ****************************************************************************/
void DHT11(void)
{
    DHT11_TEST();
}
