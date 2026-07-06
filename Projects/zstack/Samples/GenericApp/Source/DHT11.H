#ifndef __DHT11_H__
#define __DHT11_H__

#define uchar unsigned char

extern void DHT11_TEST(void);   //!< DHT11温湿度采集
extern void DHT11(void);        //!< DHT11读取接口（兼容别名）

//!< 温湿度解析结果（由DHT11_TEST/DHT11填充） */
extern uchar shidu_shi, shidu_ge, wendu_shi, wendu_ge;

#endif
