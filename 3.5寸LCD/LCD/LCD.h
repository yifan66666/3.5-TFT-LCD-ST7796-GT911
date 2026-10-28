#ifndef INC_3_5TFTLCD_LCD_H
#define INC_3_5TFTLCD_LCD_H

#include "main.h"
#include "LCD_Font.h"
/*********************************              引脚配置说明                ********************************
*   采用FSMC(显示)+软件IIC(触摸);
*   FSMC NorFlash BANK1 NE4 16位(默认)数据 1位(A10)地址线
*   Address setup time in HCLK clock cycles     1   Data setup time in HCLK clock cycles    3(这俩参数用于控制时序执行速度,目前是最快的)
*   LCD_BL配置为：推挽输出，无上下拉，高速，默认高电平；
*   LCD_T_SCL配置为：推挽输出，上拉，告诉，默认低电平；
*   LCD_T_SDA配置为：开漏输出，上拉，告诉，默认低电平；
*   LCD_T_RST配置为：推挽输出，上拉，告诉，默认低电平；
*   LCD_T_INT配置为：外部中断高电平触发，无上下拉，注意开启GPIO外部中断
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
    uint8_t Dir;        /* 横屏还是竖屏控制：0，竖屏；1，横屏。 */
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

/*********************************              LCD函数                *********************************/
void LCD_Init(uint16_t TextColor, uint16_t BackColor);  //LCD初始化
void LCD_Clear(uint16_t color);                         //LCD清屏
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

uint16_t LCD_ShowChar(uint16_t X, uint16_t Y, char Ch, uint8_t Font);                              //显示一个ASCII字符
uint16_t LCD_ShowString(uint16_t X, uint16_t Y, const char *Str, uint8_t Font);                    //显示字符串(UTF-8)
uint16_t LCD_ShowNum(uint16_t X, uint16_t Y, uint32_t Num, uint8_t Len, uint8_t Font);             //显示无符号十进制数
uint16_t LCD_printf(uint16_t X, uint16_t Y, uint8_t Font, const char *Fmt, ...);                   //格式化输出到屏幕

uint16_t LCD_ShowFPS(uint16_t X, uint16_t Y, uint8_t Font);                                        //实时帧率，每帧调一次，每秒更新

#endif //INC_3_5TFTLCD_LCD_H
