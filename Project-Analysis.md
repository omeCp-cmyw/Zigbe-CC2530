# ZigBee CC2530 物联网智能家居系统 — 代码框架分析文档

## 一、项目概述

本项目是一个基于 **TI CC2530 芯片 + ZStack 协议栈** 的 **ZigBee 物联网（IoT）智能家居/环境监测系统**。

系统通过 ZigBee 无线网络采集多种传感器数据，经由协调器节点的 WiFi 模块将数据上报到 **中国移动 OneNET 云平台**（HTTP RESTful API），同时支持从协调器远程下发控制命令到各终端设备。

**项目类型：** 智能家居 / 智能环境监测 IoT 项目，属于典型的 ZigBee 无线传感器网络 + 云平台对接的综合应用，具备 **"感知层 → 网络层 → 应用层"** 完整物联网三层架构。

---

## 二、代码框架（分层架构）

```
┌──────────────────────────────────────────────────────────┐
│  应用层 (Application Layer)                               │
│  coordinator.c / router.c / enddevice.c / IOT.c          │
│  功能：传感器采集、数据解析、云平台对接、远程控制           │
├──────────────────────────────────────────────────────────┤
│  ZigBee 协议栈层 (ZStack)                                  │
│  AF(应用框架) / NWK(网络层) / APS(应用支持层) / ZDO / MAC │
│  功能：组网、路由、绑定、数据收发                          │
├──────────────────────────────────────────────────────────┤
│  OSAL 操作系统抽象层                                       │
│  OSAL.c / OSAL_Timers / OSAL_Memory / OSAL_PwrMgr        │
│  功能：任务调度、事件管理、定时器、内存管理                 │
├──────────────────────────────────────────────────────────┤
│  HAL 硬件抽象层                                            │
│  hal_lcd / hal_uart / hal_key / hal_led / hal_adc        │
│  功能：LCD显示、串口通信、按键、LED、ADC采集               │
├──────────────────────────────────────────────────────────┤
│  硬件层 (CC2530 SoC)                                      │
│  增强型8051内核 + 2.4GHz射频 + 14位ADC + Timer + UART    │
└──────────────────────────────────────────────────────────┘
```

### 核心源文件清单

| 文件 | 路径 | 说明 |
|------|------|------|
| `coordinator.c` | `Projects/zstack/Samples/GenericApp/Source/` | 协调器主程序 |
| `router.c` | `Projects/zstack/Samples/GenericApp/Source/` | 路由器程序 |
| `enddevice.c` | `Projects/zstack/Samples/GenericApp/Source/` | 终端设备程序（ENDNUM 1~5） |
| `IOT.c` | `Projects/zstack/Samples/GenericApp/Source/` | WiFi/云平台通信模块 |
| `OSAL_GenericApp.c` | `Projects/zstack/Samples/GenericApp/Source/` | OSAL任务注册与初始化 |
| `GenericApp.h` | `Projects/zstack/Samples/GenericApp/Source/` | 应用层公共头文件 |
| `IOT.h` | `Projects/zstack/Samples/GenericApp/Source/` | IoT通信配置头文件 |
| `delay.c` | `Projects/zstack/Samples/GenericApp/Source/` | 延时工具函数 |

### OSAL 任务调度链

在 `OSAL_GenericApp.c` 中注册的任务处理函数顺序如下：

```c
const pTaskEventHandlerFn tasksArr[] = {
    macEventLoop,           // MAC层事件循环
    nwk_event_loop,         // 网络层事件循环
    Hal_ProcessEvent,       // 硬件抽象层事件处理
    MT_ProcessEvent,        // 调试跟踪任务（可选）
    APS_event_loop,         // 应用支持层事件循环
    ZDApp_event_loop,       // ZDO应用层事件循环
    GenericApp_ProcessEvent // 用户应用层事件处理
};
```

---

## 三、设备节点拓扑

### 网络拓扑结构：星型/树型网络

```
                    ┌─────────────────┐
                    │   OneNET 云平台  │
                    │  (HTTP REST API) │
                    └────────┬────────┘
                             │ TCP (WiFi)
                    ┌────────┴────────┐
                    │    协调器 (1个)   │
                    │  Coordinator     │
                    │  ENDNUM = 3      │
                    │  WiFi模块+蜂鸣器  │
                    └───┬────┬────┬───┘
          ZigBee        │    │    │      ZigBee
        ┌───────────────┘    │    └───────────────┐
        │                    │                    │
  ┌─────┴─────┐      ┌──────┴──────┐      ┌──────┴──────┐
  │  路由器    │      │  终端设备    │      │  终端设备    │
  │  Router   │      │  EndDevice  │      │  EndDevice  │
  │  DHT11    │      │  ENDNUM=1~5 │      │  ENDNUM=1~5 │
  └───────────┘      └─────────────┘      └─────────────┘
```

### 设备节点详细清单

| 序号 | 设备角色 | ENDNUM | 源文件 | 主要功能 |
|------|----------|--------|--------|----------|
| 1 | **协调器** | 3 | `coordinator.c` | 建网、数据汇聚、WiFi连云、上报OneNET、下发控制命令、蜂鸣器 |
| 2 | **路由器** | — | `router.c` | DHT11温湿度采集，单播转发数据到协调器 |
| 3 | **终端1** | 1 | `enddevice.c` | DHT11温湿度 + MQ-2烟雾传感器（ADC采集） |
| 4 | **终端2** | 2 | `enddevice.c` | 火焰传感器（ADC采集）+ 空气质量AQI（ADC采集） |
| 5 | **终端3** | 3 | `enddevice.c` | 继电器开关控制 + HC-SR501人体红外感应 |
| 6 | **终端4** | 4 | `enddevice.c` | 蜂鸣器报警（PWM驱动，T1定时器） |
| 7 | **终端5** | 5 | `enddevice.c` | 步进电机控制（正转/反转，4相步进） |

> **总计：1个协调器 + 1个路由器 + 5个终端设备 = 7个节点**

---

## 四、各设备功能详解

### 4.1 协调器（Coordinator）

**源文件：** `coordinator.c` + `IOT.c`

#### 初始化流程
1. 配置T1定时器输出PWM（蜂鸣器驱动）
2. LCD显示 "IOT1892_YANWEI"
3. 复位WiFi模块（P0.6口低电平复位）
4. 初始化UART0（调试打印，115200波特率）
5. 初始化UART1（WiFi模块通信，115200波特率）
6. 设置广播地址模式（0xFFFF）
7. 注册端点描述符和应用框架
8. 注册ZDO消息回调（绑定响应、匹配描述符响应）

#### 事件处理
- **`ZDO_STATE_CHANGE`**：网络状态变化时启动定时发送事件（800ms周期）
- **`GENERICAPP_SEND_MSG_EVT`**：定时触发WiFi初始化 + 云平台连接 + 广播数据发送
- **`AF_INCOMING_MSG_CMD`**：接收终端上报数据并处理
- **`KEY_CHANGE`**：按键处理（SW2绑定、SW4匹配描述符）

#### 数据接收与解析
协调器通过簇ID `GENERICAPP_CLUSTERID(1)` 接收终端数据，根据第一个字节（设备ID）解析：

| 设备ID | 解析内容 | 数据格式 |
|--------|----------|----------|
| 1 | 温度、湿度、烟雾 | `Data[1-2]`=温度, `Data[3-4]`=湿度, `Data[5-6]`=MQ-2 |
| 2 | 火焰、空气质量 | `Data[1-2]`=火焰, `Data[3-4]`=AQI |
| 3 | 继电器状态、人体感应 | `Data[1]`=继电器, `Data[2]`=HC-SR501 |

#### 云平台数据上报
当WiFi连接状态为6（就绪）时，通过HTTP POST将数据上报到OneNET：

```
POST /devices/{devid}/datapoints?type=3 HTTP/1.1
api-key: {devkey}
Host: api.heclouds.com
Content-Length: {len}

{"temp":25,"humi":60,"MQ_2":30}
```

### 4.2 路由器（Router）

**源文件：** `router.c`

- 使用DHT11传感器采集温度和湿度
- 采用**单播**方式（目标地址0x0000，端点10）将数据发送到协调器
- 数据格式：`R[温度十位][温度个位][湿度十位][湿度个位]`
- 发送周期：1500ms

### 4.3 终端1（ENDNUM=1）— 温湿度 + 烟雾传感器

**源文件：** `enddevice.c`

- **DHT11**：采集温度（`wendu_shi/wendu_ge`）和湿度（`shidu_shi/shidu_ge`）
- **MQ-2 烟雾传感器**：通过ADC通道5（P0.5引脚）读取14位ADC值，转换为百分比浓度
- **LCD显示**：中文16x16字符显示温湿度、烟雾浓度
- **数据上报**：`[ID][温度十位][温度个位][湿度十位][湿度个位][烟雾十位][烟雾个位]`

### 4.4 终端2（ENDNUM=2）— 火焰 + 空气质量

**源文件：** `enddevice.c`

- **火焰传感器**：通过ADC通道4（P0.4引脚）读取火焰强度百分比
- **空气质量AQI**：通过ADC通道5（P0.5引脚）读取空气质量指标
- **LCD显示**：火焰值和AQI值
- **数据上报**：`[ID][火焰十位][火焰个位][AQI十位][AQI个位]`

### 4.5 终端3（ENDNUM=3）— 继电器 + 人体红外

**源文件：** `enddevice.c`

- **继电器控制**：P1.5引脚控制继电器开关（高/低电平）
- **HC-SR501 人体红外传感器**：P0.6引脚检测人体存在（高电平=有人）
- **远程控制**：接收协调器发来的 `1ON`（开）/ `1OF`（关）命令
- **数据上报**：`[ID][继电器状态][人体感应状态]`

### 4.6 终端4（ENDNUM=4）— 蜂鸣器报警

**源文件：** `enddevice.c`

- **PWM蜂鸣器**：P0.7引脚通过T1定时器输出PWM驱动蜂鸣器
- **远程控制**：接收协调器发来的 `yes`（响）/ `noo`（停）命令
- 用于异常气体/火焰检测时的声光报警

### 4.7 终端5（ENDNUM=5）— 步进电机

**源文件：** `enddevice.c`

- **4相步进电机**：P0.4(A/1N1)、P0.5(B/1N2)、P0.6(C/1N3)、P0.7(D/1N4)
- **正转序列**：`0x80 → 0x40 → 0x20 → 0x10`（D-C-B-A导通）
- **反转序列**：`0x10 → 0x20 → 0x40 → 0x80`（A-B-C-D导通）
- **远程控制**：接收协调器发来的 `shun`（顺时针）/ `ni`（逆时针）命令
- 每次执行1000步，转速可调（`ucSpeed`参数）

---

## 五、通信协议

### 5.1 ZigBee 通信参数

| 参数 | 值 | 说明 |
|------|-----|------|
| Endpoint | 10 | 应用端点号 |
| Profile ID | 0x0F04 | 自定义Profile |
| Device ID | 0x0001 | 设备标识 |
| Cluster ID 1 | 1 (GENERICAPP_CLUSTERID) | 数据收发主簇 |
| Cluster ID 2 | 2 (GENERICAPP_END) | 终端标识 |
| Cluster ID 3 | 3 (GENERICAPP_ROT) | 路由器标识 |
| Cluster ID 4 | 4 (SAMPLEAPP_Guangqiang) | 光强传感器 |
| 协调器发送周期 | 800ms | 广播控制命令 |
| 终端发送周期 | 1500ms + 随机偏移 | 上报传感器数据 |

### 5.2 数据帧格式

**终端上报数据（ASCII编码）：**
```
字节0: 设备ID（'1'~'5'）
字节1-N: 传感器数据（ASCII字符，每个传感器2位十进制数）
```

**协调器下发控制命令（ASCII编码）：**
```
继电器控制: "1ON" / "1OF"
蜂鸣器控制: "yes" / "noo"
电机控制:   "shun" / "ni"
```

### 5.3 WiFi/云平台通信

**WiFi模块初始化AT指令序列：**
```
AT                          → 测试连接
AT+CWMODE=3                 → 设置AP+STA模式
AT+CIPMODE=1                → 设置透传模式
AT+CWJAP="SSID","PASSWORD"  → 连接WiFi路由器
AT+CIPSTART="TCP","183.230.40.33",80  → TCP连接OneNET服务器
AT+CIPSEND                  → 开始发送数据
```

**云平台配置（IOT.h）：**
```c
#define devkey  "LwAugj24DRwby=ebA77npxvw9yo="  // OneNET产品API Key
#define devid   "746920376"                       // OneNET设备ID
#define LYSSID  "YanWei"                          // WiFi SSID
#define LYPASSWD "2233445566"                     // WiFi密码
```

---

## 六、硬件接口分配

### CC2530 GPIO 分配表

| 引脚 | 功能 | 设备 |
|------|------|------|
| P0.0 | UART0 RX | 调试串口 |
| P0.1 | UART0 TX | 调试串口 |
| P0.4 | 步进电机A(1N1) / ADC通道4 | 终端5 / 终端2(火焰) |
| P0.5 | 步进电机B(1N2) / ADC通道5 | 终端5 / 终端1,2(MQ-2/AQI) |
| P0.6 | 步进电机C(1N3) / HC-SR501 / WiFi Reset | 终端5 / 终端3 / 协调器 |
| P0.7 | 步进电机D(1N4) / 蜂鸣器PWM / T1定时器 | 终端5 / 终端4 / 协调器 |
| P1.0 | LED1 | 指示灯 |
| P1.5 | 继电器控制 | 终端3 |
| P1.6 | 火焰传感器 | 终端2 |

### 外设资源

| 外设 | 接口 | 用途 |
|------|------|------|
| UART0 | P0.0/P0.1, 115200bps | 调试信息打印 |
| UART1 | P1.6/P1.7, 115200bps | WiFi模块通信 |
| Timer1 | P0.7, PWM模式 | 蜂鸣器驱动 |
| ADC CH4 | P0.4, 14位分辨率 | 火焰传感器 |
| ADC CH5 | P0.5, 14位分辨率 | MQ-2/AQI传感器 |

---

## 七、系统工作流程

```
1. 系统上电
   ├── 协调器：建网 → WiFi初始化 → 等待终端加入
   ├── 路由器：加入网络 → 定时采集DHT11 → 单播上报
   └── 终端(1~5)：加入网络 → 初始化传感器/执行器 → 定时采集上报

2. 数据上报流程
   ├── 终端采集传感器数据（DHT11/ADC/GPIO）
   ├── 终端将数据打包为ASCII字符串
   ├── 终端通过ZigBee单播发送到协调器
   ├── 协调器接收并解析数据
   ├── 协调器LCD显示数据
   └── 协调器通过WiFi→HTTP POST→OneNET云平台

3. 远程控制流程
   ├── 串口0接收用户输入的控制命令
   ├── 协调器通过ZigBee广播发送命令
   ├── 终端接收命令并解析
   └── 终端执行动作（继电器/蜂鸣器/电机）
```

---

## 八、关键代码模块说明

### 8.1 OSAL任务初始化（OSAL_GenericApp.c）

```c
void osalInitTasks(void) {
    uint8 taskID = 0;
    tasksEvents = (uint16 *)osal_mem_alloc(sizeof(uint16) * tasksCnt);
    osal_memset(tasksEvents, 0, sizeof(uint16) * tasksCnt);
    macTaskInit(taskID++);      // MAC层
    nwk_init(taskID++);         // 网络层
    Hal_Init(taskID++);         // HAL层
    MT_TaskInit(taskID++);      // 调试层
    APS_Init(taskID++);         // APS层
    ZDApp_Init(taskID++);       // ZDO层
    GenericApp_Init(taskID);    // 应用层
}
```

### 8.2 协调器消息处理（coordinator.c - GenericApp_MessageMSGCB）

根据设备ID分别解析传感器数据：
- `id==1`：温度、湿度、MQ-2烟雾
- `id==2`：火焰、空气质量AQI
- `id==3`：继电器状态、人体红外

解析后封装为JSON格式通过HTTP POST上报OneNET云平台。

### 8.3 WiFi连接状态机（IOT.c）

```
状态0 → AT测试 → 状态1
状态1 → 设置WiFi模式 → 状态2
状态2 → 设置透传模式 → 状态3
状态3 → 连接路由器 → 状态4
状态4 → TCP连接OneNET → 状态5
状态5 → 开始发送数据 → 状态6（就绪）
```

### 8.4 ADC采集函数（enddevice.c）

- `GetADCMQ()`：ADC通道5，14位分辨率，读取MQ-2/AQI，范围0~8192，转百分比
- `getFrame()`：ADC通道4，14位分辨率，读取火焰传感器，范围0~8192，转百分比

### 8.5 步进电机驱动（enddevice.c）

```c
// 正转相序表
uchar funcForward[4] = {0x80, 0x40, 0x20, 0x10}; // D-C-B-A
// 反转相序表
uchar funcReverse[4] = {0x10, 0x20, 0x40, 0x80}; // A-B-C-D
```

---

## 九、系统特性总结

| 特性 | 说明 |
|------|------|
| 网络拓扑 | 星型/树型（1协调器 + 1路由器 + 5终端） |
| 通信方式 | ZigBee 2.4GHz + WiFi 2.4GHz |
| 传感器 | DHT11温湿度、MQ-2烟雾、火焰、空气质量、HC-SR501人体红外 |
| 执行器 | 继电器开关、蜂鸣器报警、步进电机 |
| 云平台 | 中国移动OneNET（HTTP REST API） |
| 显示 | LCD中文显示（16x16汉字 + 8x16 ASCII） |
| 供电 | 3.3V（CC2530典型工作电压） |
| 开发工具 | IAR Embedded Workbench for 8051 |
| 协议栈 | TI ZStack-CC2530 |
| 自动重启 | 协调器每接收3000次数据后软重启一次 |
