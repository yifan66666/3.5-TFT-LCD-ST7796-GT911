#ifndef INC_3_5TFTLCD_LCD_H
#define INC_3_5TFTLCD_LCD_H

#include "main.h"
#include "tim.h"
#include "LCD_Font.h"
/*********************************              引脚配置说明                ********************************
*   采用FSMC(显示) + 软件IIC(触摸)；
*
*   【显示】FSMC SRAM(不是 NorFlash) BANK1 NE4，16 位数据线 + 1 位地址线(A10)作 RS。
*     FSMC 已开扩展模式(Extended Mode)，读写各一套时序，互不影响：
*       读时序(写入 BTR)  AddressSetupTime=5 / DataSetupTime=8  = 15 HCLK = 208ns
*                         ST7796 要求 TRC>=160ns、读 ID 时 TCS>=45ns，满足
*       写时序(写入 BWTR) AddressSetupTime=1 / DataSetupTime=3  = 6 HCLK  = 83.3ns
*                         ST7796 要求 TWC>=66ns，留 26% 余量
*     刷屏快慢只由写时序决定；读时序调快会导致 ID/状态读错，不要动。
*     注意 HAL 在扩展模式下把第 2 个参数(ReadTiming)写进 BTR、第 3 个(WriteTiming)写进 BWTR，
*     CubeMX 里两组参数填反了就会出现"写变慢、读仍不合规"，生成后务必对照 fsmc.c 核对。
*
*   【GPIO】以下均为 CubeMX 里的配置：
*     LCD_BL     复用推挽输出 TIM3_CH3(PB0)，高速；PSC=35 / ARR=99 → 20kHz PWM，
*                一个周期正好 100 格，CCR 直接等于占空比百分比（0~100），上电默认 100%
*                （定时器未启动时该脚输出无效电平，所以 LCD_Init() 第一件事就是开 PWM）
*     LCD_T_SCL  推挽输出，上拉，高速，默认低电平（软件 I2C 时钟，驱动里先拉高再起始）
*     LCD_T_SDA  开漏输出，上拉，高速，默认低电平（软件 I2C 数据，靠上拉拉高）
*     LCD_T_RST  推挽输出，上拉，高速，默认低电平（只接 GT911，ST7796 无复位脚）
*     LCD_T_INT  外部中断上升沿触发，**无上下拉(NOPULL)**，必须开启 EXTI 中断
*                复位期间由 LCD_T_Reset() 临时切成推挽输出拉高，用来锁存 I2C 地址 0x28；
*                工作期间必须保持悬浮输入，否则 GT911 不再产生触摸报告。
*********************************************************************************************************/
/*********************************              LCD显示部分                *********************************/
typedef struct
{
    volatile uint16_t LCD_REG;
    volatile uint16_t LCD_RAM;
} LCD_TypeDef;

typedef struct
{
    uint16_t Width;     /* LCD 宽度 */
    uint16_t Height;    /* LCD 高度 */
    uint16_t ID;        /* LCD ID */
    uint8_t Dir;        /* 显示方向 0~3（LCD_ROT_x），由 LCD_SetRotation() 维护，不要直接赋值 */
    uint16_t WriteGRAM;   /* 开始写gram指令 */
    uint16_t SetXCmd;   /* 设置x坐标指令 */
    uint16_t SetYCmd;   /* 设置y坐标指令 */
    uint16_t TextColor;
    uint16_t BackColor;
} LCD_Param_TypeDef;

extern LCD_Param_TypeDef LCD_Param;

/* FSMC相关参数 定义
 * 注意: 我们默认是通过FSMC块1来连接LCD, 块1有4个片选: FSMC_NE1~4
 *
 * 修改LCD_FSMC_NEX, 对应的LCD_CS_GPIO相关设置也得改
 * 修改LCD_FSMC_AX , 对应的LCD_RS_GPIO相关设置也得改
 */
#define LCD_FSMC_NEX         4              /* 使用FSMC_NE4接LCD_CS,取值范围只能是: 1~4 */
#define LCD_FSMC_AX          10             /* 使用FSMC_A10接LCD_RS,取值范围是: 0 ~ 25 */

#define LCD_FSMC_BCRX        FSMC_Bank1->BTCR[(LCD_FSMC_NEX - 1) * 2]       /* BCR寄存器,根据LCD_FSMC_NEX自动计算 */
#define LCD_FSMC_BTRX        FSMC_Bank1->BTCR[(LCD_FSMC_NEX - 1) * 2 + 1]   /* BTR寄存器,根据LCD_FSMC_NEX自动计算 */
#define LCD_FSMC_BWTRX       FSMC_Bank1E->BWTR[(LCD_FSMC_NEX - 1) * 2]      /* BWTR寄存器,根据LCD_FSMC_NEX自动计算 */

#define LCD_BASE        (uint32_t)((0X60000000 + (0X4000000 * (LCD_FSMC_NEX - 1))) | (((1 << LCD_FSMC_AX) * 2) -2))
#define LCD             ((LCD_TypeDef *) LCD_BASE)

/*********************************              显示方向                ********************************
*   切换方向只改 LCD_DEFAULT_ROT 这一行，然后重新编译，其它代码一行都不用动。
*
*   LCD_SetRotation() 一次做完全部四件事，缺一件就会出问题：
*     ① 写 MADCTL(0x36) 决定行列扫描方向   —— 只改这里，画面方向变但坐标不跟着变
*     ② 交换 LCD_Param.Width / Height      —— 漏了，窗口/清屏/裁剪全按竖屏算，右边和下边留黑
*     ③ 切换触摸坐标变换                   —— 漏了，画面转 90° 而触点停留在原方向，点不准
*     ④ 清空 LCD_printf 的擦除锚点并清屏    —— 漏了，转屏后第一次 printf 会按旧坐标擦错行
*
*   MADCTL 取值由 LCD.c 的 MadctlTab[] 维护，BGR 位固定 0x08 不能丢，丢了红蓝互换。
*
*   若横屏下画面方向正确（文字正着），但触摸整体差 180°（点左上角反应在右下角），
*   把 LCD.c 里 LCD_T_ScanIT() 的 LCD_ROT_90 与 LCD_ROT_270 两条坐标公式对调即可，
*   只改软件、不用动这里的配置。
*********************************************************************************************************/
#define LCD_ROT_0           0       /* 竖屏      320x480（面板原生方向） */
#define LCD_ROT_90          1       /* 横屏      480x320 */
#define LCD_ROT_180         2       /* 竖屏倒置  320x480 */
#define LCD_ROT_270         3       /* 横屏      480x320（与 90 相差 180°） */

#define LCD_DEFAULT_ROT     LCD_ROT_0   /* ← 上电默认方向：改这里 */

#define LCD_PANEL_W         320     /* 面板物理分辨率(短边)，与方向无关，不要改 */
#define LCD_PANEL_H         480     /* 面板物理分辨率(长边)，与方向无关，不要改 */

/* 常用画笔颜色 */
#define WHITE           0xFFFF      /* 白色 */
#define BLACK           0x0000      /* 黑色 */
#define RED             0xF800      /* 红色 */
#define GREEN           0x07E0      /* 绿色 */
#define BLUE            0x001F      /* 蓝色 */
#define MAGENTA         0XF81F      /* 品红色/紫红色 = BLUE + RED */
#define YELLOW          0XFFE0      /* 黄色 = GREEN + RED */
#define CYAN            0X07FF      /* 青色 = GREEN + BLUE */

/* 非常用颜色 */
#define BROWN           0XBC40      /* 棕色 */
#define BRRED           0XFC07      /* 棕红色 */
#define GRAY            0X8430      /* 灰色 */
#define DARKBLUE        0X01CF      /* 深蓝色 */
#define LIGHTBLUE       0X7D7C      /* 浅蓝色 */
#define GRAYBLUE        0X5458      /* 灰蓝色 */
#define LIGHTGREEN      0X841F      /* 浅绿色 */
#define LGRAY           0XC618      /* 浅灰色(PANNEL),窗体背景色 */
#define LGRAYBLUE       0XA651      /* 浅灰蓝色(中间层颜色) */
#define LBBLUE          0X2B12      /* 浅棕蓝色(选择条目的反色) */

/* 背光 PWM 实例与通道，改接别的定时器/引脚时只动这两行 */
#define LCD_BL_TIM       (&htim3)
#define LCD_BL_TIM_CH    TIM_CHANNEL_3

/*********************************              LCD触摸部分                *********************************/

#define GT_CMD_WR   0x28
#define GT_CMD_RD   0x29

/* GT9XXX 部分寄存器定义  */
#define GT9XXX_CTRL_REG     0X8040      /* GT9XXX控制寄存器 */
#define GT9XXX_CFGS_REG     0X8047      /* GT9XXX配置起始地址寄存器 */
#define GT9XXX_XMAX_REG     0x8048      /* X输出最大值(低/高) */
#define GT9XXX_YMAX_REG     0x804A      /* Y输出最大值(低/高) */
#define GT9XXX_PID_REG      0x8140      /* Product ID，4字节ASCII */
#define GT9XXX_GSTID_REG    0x814E      /* 坐标状态：bit7=就绪, bit0~3=点数 */
#define GT9XXX_TP1_REG      0X8150      /* 第一个触摸点数据地址 */
#define GT9XXX_TP2_REG      0X8158      /* 第二个触摸点数据地址 */
#define GT9XXX_TP3_REG      0X8160      /* 第三个触摸点数据地址 */
#define GT9XXX_TP4_REG      0X8168      /* 第四个触摸点数据地址 */
#define GT9XXX_TP5_REG      0X8170      /* 第五个触摸点数据地址 */

#define GT_MAX_TOUCH   5

typedef struct
{
    uint8_t  Count;                  /* 当前触摸点数 */
    uint16_t X[GT_MAX_TOUCH];
    uint16_t Y[GT_MAX_TOUCH];
} TP_Data_TypeDef;

extern TP_Data_TypeDef TP_Data;
extern uint8_t LCD_TP_State;                            //触摸初始化结果 0=成功
extern uint8_t LCD_Panel_State;                         //面板 ID 自检结果 0=读到 0x7796

/*********************************              LCD函数                *********************************/
void LCD_Init(uint16_t TextColor, uint16_t BackColor);  //LCD初始化(内含背光PWM启动,默认100%)
void LCD_SetRotation(uint8_t Rot);                      //切换显示方向 0~3(重写MADCTL+交换W/H+清屏)
void LCD_Clear(uint16_t color);                         //LCD清屏
void LCD_BackLight(uint8_t Duty);                       //背光亮度 0~100(%),上电默认100
void LCD_SetTextColor(uint16_t color);                  //设置前景色
void LCD_SetBackColor(uint16_t color);                  //设置背景色
uint8_t LCD_T_ScanIT(void);                             //触摸扫描(中断驱动，主循环调用)
extern volatile uint8_t TP_IntFlag;                     //触摸中断标志，EXTI 回调置 1

void LCD_Set_Cursor(uint16_t X, uint16_t Y);                                            //设置光标
void LCD_Set_Window(uint16_t StartX, uint16_t StartY, uint16_t Width, uint16_t Height); //设置窗口
void LCD_Fill(uint16_t StartX, uint16_t StartY, uint16_t Width,
    uint16_t Height, uint16_t color);                                                   //填充窗口
void LCD_DrawPoint(uint16_t X, uint16_t Y);                                             //画点
void LCD_DrawLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1);                  //画线
void LCD_DrawHLine(uint16_t X, uint16_t Y, uint16_t Len);                               //水平线
void LCD_DrawVLine(uint16_t X, uint16_t Y, uint16_t Len);                               //垂直线
void LCD_FillRect(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height);             //实心矩形
void LCD_DrawRect(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height);             //矩形边框
void LCD_DrawCircle(uint16_t X, uint16_t Y, uint16_t R);                                //空心圆(圆周线条)
void LCD_FillCircle(uint16_t X, uint16_t Y, uint16_t R);                                //实心圆
void LCD_DrawTriangle(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1,
                      uint16_t X2, uint16_t Y2);                                        //空心三角形
void LCD_DrawBitmap(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height,
                    const uint16_t *Data);                                              //显示RGB565位图
void LCD_WritePixels(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height,
                     const uint16_t *Data);                                             //批量灌像素(自动裁剪,源行长=Width)

uint16_t LCD_ShowChar(uint16_t X, uint16_t Y, char Ch, uint8_t Font);                              //显示一个ASCII字符
uint16_t LCD_ShowString(uint16_t X, uint16_t Y, const char *Str, uint8_t Font);                    //显示字符串(UTF-8)
uint16_t LCD_ShowNum(uint16_t X, uint16_t Y, uint32_t Num, uint8_t Len, uint8_t Font);             //显示无符号十进制数
uint16_t LCD_TextW(const char *Str, uint8_t Font);                                                 //计算文本像素宽度
uint16_t LCD_printf(uint16_t X, uint16_t Y, uint8_t Font, const char *Fmt, ...);                   //格式化输出到屏幕

uint16_t LCD_ShowFPS(uint16_t X, uint16_t Y, uint8_t Font);                                        //实时帧率，每帧调一次，每秒更新

#endif //INC_3_5TFTLCD_LCD_H
