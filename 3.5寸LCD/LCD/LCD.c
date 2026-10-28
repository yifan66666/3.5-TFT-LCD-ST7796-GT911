#include "LCD.h"
#include <stdarg.h>
#include <stdio.h>

/*******************************
 *      LCD显示底层函数
 *      显示芯片 ：ST7796
 *      创建于2026-10-23
 *******************************/
LCD_Param_TypeDef LCD_Param;
/**
 * @brief       写入ST7796指定寄存器
 * @param       Reg : 起始寄存器地址
 * @retval      无
 */
static void LCD_Write_Reg(volatile uint16_t Reg)
{
    LCD ->LCD_REG = Reg;
}
/**
 * @brief       向ST7796写数据
 * @param       data : 要写入的数据
 * @retval      无
 */
static void LCD_Write_Data(volatile uint16_t data)
{
    LCD ->LCD_RAM = data;
}
/**
 * @brief       向ST7796指定寄存器写数据
 * @param       Reg : 起始寄存器地址
 * @param       data: 要写入的数据
 * @retval      无
 */
static void LCD_Write_RegData(uint16_t Reg , uint16_t data)
{
    LCD->LCD_REG = Reg;
    LCD->LCD_RAM = data;
}

static void lcd_opt_delay(uint32_t i)
{
    volatile uint32_t n = i;
    while (n--);
}
/**
 * @brief       从ST7796读数据
 * @param
 * @retval      读出的数据
 */
static uint16_t LCD_Read_Data(void)
{
    volatile uint16_t data = 0;
    lcd_opt_delay(2);
    data = LCD->LCD_RAM;
    return data;
}
/**
 * @brief       ST7796寄存器初始化
 * @param
 * @retval      无
 */
static void LCD_ST7796_RegInit(void)
{
    LCD_Write_Reg(0x11);              /* SLPOUT 退出睡眠 */
    HAL_Delay(120);

    LCD_Write_RegData(0x36, 0x08);    /* MADCTL：竖屏，MX=0、MY=0、BGR=1 */
    LCD_Write_RegData(0x3A, 0x55);    /* COLMOD：16bit/pixel (RGB565) */

    LCD_Write_RegData(0xF0, 0xC3);    /* 解锁扩展命令页 */
    LCD_Write_RegData(0xF0, 0x96);

    LCD_Write_RegData(0xB4, 0x01);    /* 反转控制 */

    LCD_Write_Reg(0xB6);              /* 显示功能控制 */
    LCD_Write_Data(0x0A);
    LCD_Write_Data(0xA2);

    LCD_Write_RegData(0xB7, 0xC6);    /* 进入模式 */

    LCD_Write_Reg(0xB9);              /* 扩展命令使能 */
    LCD_Write_Data(0x02);
    LCD_Write_Data(0xE0);

    LCD_Write_Reg(0xC0);              /* 电源控制1 */
    LCD_Write_Data(0x80);
    LCD_Write_Data(0x16);

    LCD_Write_RegData(0xC1, 0x19);    /* 电源控制2 */
    LCD_Write_RegData(0xC2, 0xA7);    /* 电源控制3 */
    LCD_Write_RegData(0xC5, 0x16);    /* VCOM */

    LCD_Write_Reg(0xE8);              /* 输出控制 */
    LCD_Write_Data(0x40); LCD_Write_Data(0x8A);
    LCD_Write_Data(0x00); LCD_Write_Data(0x00);
    LCD_Write_Data(0x29); LCD_Write_Data(0x19);
    LCD_Write_Data(0xA5); LCD_Write_Data(0x33);

    LCD_Write_Reg(0xE0);              /* Gamma 正极性 */
    LCD_Write_Data(0xF0); LCD_Write_Data(0x07); LCD_Write_Data(0x0D);
    LCD_Write_Data(0x04); LCD_Write_Data(0x05); LCD_Write_Data(0x14);
    LCD_Write_Data(0x36); LCD_Write_Data(0x54); LCD_Write_Data(0x4C);
    LCD_Write_Data(0x38); LCD_Write_Data(0x13); LCD_Write_Data(0x14);
    LCD_Write_Data(0x2E); LCD_Write_Data(0x34);

    LCD_Write_Reg(0xE1);              /* Gamma 负极性 */
    LCD_Write_Data(0xF0); LCD_Write_Data(0x10); LCD_Write_Data(0x14);
    LCD_Write_Data(0x0E); LCD_Write_Data(0x0C); LCD_Write_Data(0x08);
    LCD_Write_Data(0x35); LCD_Write_Data(0x44); LCD_Write_Data(0x4C);
    LCD_Write_Data(0x26); LCD_Write_Data(0x10); LCD_Write_Data(0x12);
    LCD_Write_Data(0x2C); LCD_Write_Data(0x32);

    LCD_Write_RegData(0xF0, 0x3C);    /* 锁定回 Page0 */
    LCD_Write_RegData(0xF0, 0x69);

    HAL_Delay(120);

    LCD_Write_Reg(0x21);              /* INVON 显示反相 */
    LCD_Write_Reg(0x29);              /* DISPON 开显示 */
}

/*******************************
 *      LCD触摸底层函数
 *      触摸芯片 ：GT911
 *      创建于2026-10-23
 *******************************/

/**
 * @brief       IIC写SCL状态
 * @param       state : SCL指定状态
 * @retval      无
 */
static void LCD_Write_SCL(uint8_t state)
{
    HAL_GPIO_WritePin(LCD_T_SCL_GPIO_Port, LCD_T_SCL_Pin, state);
}
/**
 * @brief       IIC写SDA状态
 * @param       state : SDA指定状态
 * @retval      无
 */
static void LCD_Write_SDA(uint8_t state)
{
    HAL_GPIO_WritePin(LCD_T_SDA_GPIO_Port, LCD_T_SDA_Pin, state);
}
/**
 * @brief       IIC读SDA状态
 * @param
 * @retval      SDA当前状态
 */
static uint8_t LCD_Read_SDA(void)
{
    uint8_t state = 0;
    state = HAL_GPIO_ReadPin(LCD_T_SDA_GPIO_Port, LCD_T_SDA_Pin);
    return state;
}
/**
 * @brief       IIC起始信号
 * @param
 * @retval      无
 */
static void LCD_I2C_Start(void)
{
    LCD_Write_SDA(1);
    LCD_Write_SCL(1);
    LCD_Write_SDA(0);
    LCD_Write_SCL(0);
}
/**
 * @brief       IIC终止信号
 * @param
 * @retval      无
 */
static void LCD_I2C_Stop(void)
{
    LCD_Write_SDA(0);
    LCD_Write_SCL(1);
    LCD_Write_SDA(1);
}
/**
 * @brief       IIC发送一个字节(8Bit)
 * @param       Byte ：一个字节(8Bit)
 * @retval      无
 */
static void LCD_I2C_SendByte(uint8_t Byte)
{
    uint8_t i = 0;
    for(i = 0;i < 8;i ++)
    {
        LCD_Write_SDA(!!(Byte & (0x80 >> i)));
        LCD_Write_SCL(1);
        LCD_Write_SCL(0);
    }
}
/**
 * @brief       IIC接收一个字节(8Bit)
 * @param
 * @retval      接收一个字节(8Bit)
 */
static uint8_t LCD_I2C_ReceiveByte(void)
{
    uint8_t i = 0,Byte = 0x00;
    LCD_Write_SDA(1);
    for (i = 0;i < 8;i ++)
    {
        LCD_Write_SCL(1);
        if (LCD_Read_SDA()){Byte |= (0x80 >> i);}
        LCD_Write_SCL(0);
    }
    return Byte;
}
/**
 * @brief       IIC发送一个应答位
 * @param       AckBit : 0 -> Ack;1 -> NoAck
 * @retval      无
 */
static void LCD_I2C_SendAck(uint8_t AckBit)
{
    LCD_Write_SDA(AckBit);
    LCD_Write_SCL(1);
    LCD_Write_SCL(0);
}
/**
 * @brief       IIC发送一个应答位
 * @param
 * @retval      AckBit : 0 -> Ack;1 -> NoAck
 */
static uint8_t LCD_I2C_ReceiveAck(void)
{
    uint8_t AckBit = 0x00;
    LCD_Write_SDA(1);
    LCD_Write_SCL(1);
    AckBit = LCD_Read_SDA();
    LCD_Write_SCL(0);
    return AckBit;
}

/**
 * @brief       向GT911写入一次数据
 * @param       Reg : 起始寄存器地址
 * @param       Buf : 数据缓缓存区
 * @param       Len : 写数据长度
 * @retval      0, 成功; 1, 失败;
 */
static uint8_t LCD_T_Write(uint16_t Reg, uint8_t *Buf, uint16_t Len)
{
    uint16_t i = 0;

    LCD_I2C_Start();
    LCD_I2C_SendByte(GT_CMD_WR);
    if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}
    LCD_I2C_SendByte(Reg >> 8);
    if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}
    LCD_I2C_SendByte(Reg & 0xFF);
    if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}

    for (i = 0; i < Len; i++)
    {
        LCD_I2C_SendByte(Buf[i]);
        if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}
    }
    LCD_I2C_Stop();
    return 0;
}

/**
 * @brief       从GT911读一次数据
 * @param       Reg : 起始寄存器地址
 * @param       Buf : 数据缓缓存区
 * @param       Len : 读数据长度
 * @retval      0, 成功; 1, 失败;
 */
static uint8_t LCD_T_Read(uint16_t Reg, uint8_t *Buf, uint16_t Len)
{
    uint16_t i = 0;

    LCD_I2C_Start();
    LCD_I2C_SendByte(GT_CMD_WR);
    if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}
    LCD_I2C_SendByte(Reg >> 8);
    if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}
    LCD_I2C_SendByte(Reg & 0xFF);
    if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}

    LCD_I2C_Start();
    LCD_I2C_SendByte(GT_CMD_RD);
    if (LCD_I2C_ReceiveAck()){LCD_I2C_Stop();return 1;}
    for (i = 0; i < Len; i++)
    {
        Buf[i] = LCD_I2C_ReceiveByte();
        LCD_I2C_SendAck(i < Len - 1 ? 0 : 1);
    }
    LCD_I2C_Stop();
    return 0;
}

static void LCD_T_Reset(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    LCD_Write_SCL(1);  LCD_Write_SDA(1);

    /* 严格按 GT91x 编程指南 4.2 节"设定地址为 0x28/0x29 的时序"：
       ① Reset 输出低(>100us)  ② INT 输出高  ③ Reset 输出高(>5ms)  ④ INT 转悬浮输入态
       INT 这里必须用推挽输出，不能用内部上拉代替：上拉只有 40k，遇到外部下拉就拉不高，
       地址会被选成 0x5D（0xBA/0xBB），整套读写全部失效。 */
    GPIO_InitStruct.Pin   = LCD_T_INT_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_T_INT_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(LCD_T_RST_GPIO_Port, LCD_T_RST_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_T_INT_GPIO_Port, LCD_T_INT_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);                                   /* ① Reset 低电平 >100us，这里给 10ms */

    HAL_GPIO_WritePin(LCD_T_INT_GPIO_Port, LCD_T_INT_Pin, GPIO_PIN_SET);
    HAL_Delay(1);                                    /* ② 先把 INT 拉高，再释放 Reset */

    HAL_GPIO_WritePin(LCD_T_RST_GPIO_Port, LCD_T_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);                                   /* ③ Reset 高电平 >5ms，这里给 10ms */

    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LCD_T_INT_GPIO_Port, &GPIO_InitStruct);

    __HAL_GPIO_EXTI_CLEAR_IT(LCD_T_INT_Pin);         /* 清掉切换模式瞬间可能锁存的假边沿 */

    /* 4.3 节：INT 转悬浮后要延时 50ms 以上，芯片才允许接收配置/读寄存器 */
    HAL_Delay(100);
}

/**
 * @brief       触摸初始化
 * @param
 * @retval      1 -> 失败;0 -> 成功
 */
uint8_t LCD_TP_State = 0xFF;        /* 触摸初始化结果，0=ID匹配成功 */

static uint8_t LCD_TP_Init(void)
{
    uint8_t ID[5];

    LCD_T_Reset();

    if (LCD_T_Read(0x8140, ID, 4)) return 1;     /* Product ID */
    ID[4] = 0;

    if (ID[0] != '9' || ID[1] != '1' || ID[2] != '1') return 1;   /* 不是 GT911 */

    /* 这里原本有软件复位：cmd=0x02 写 0x8040，等 10ms，再写 0x00。
       0x8040=0x02 会让 GT911 从 Flash 重新加载整块配置区（0x8047~0x80FE），
       实测等 10ms 就发下一条命令会把加载打断，配置区留在全 0：
       0x8048/0x804A 量程读回 0，芯片从此不再产生任何触摸报告（INT 不动、0x814E 恒 0）。
       硬件复位已经在 LCD_T_Reset 里做过（RST 低 10ms + 高后等 100ms），足够让配置就位，
       所以这段软件复位整段删除。若以后确实要软复位，必须等够 100ms 以上再写 0x8040=0x00。 */

    /* 预读一次：若此刻 INT 已经是高电平，把它读掉让 GT911 拉低，
       否则下一次触摸不会产生上升沿，表现为"第一次摸没反应" */
    TP_IntFlag = 1;
    LCD_T_ScanIT();

    return 0;
}

TP_Data_TypeDef TP_Data;

volatile uint8_t TP_IntFlag = 0;      /* EXTI 回调置 1，由 LCD_T_ScanIT 消费 */

/*******************************
 *      LCD功能函数
 *      创建于2026-10-23
 *******************************/

/**
 * @brief       设置前景色
 * @param       color : RGB565 color
 * @retval      无
 */
void LCD_SetTextColor(uint16_t color)
{
    LCD_Param.TextColor = color;
}

/**
 * @brief       设置背景色
 * @param       color : RGB565 color
 * @retval      无
 */
void LCD_SetBackColor(uint16_t color)
{
    LCD_Param.BackColor = color;
}

/**
 * @brief       清屏并设置底色
 * @param       color : RGB565 color
 * @retval      无
 */
void LCD_Clear(uint16_t color)
{
    uint32_t total = (uint32_t)LCD_Param.Width * LCD_Param.Height;

    LCD_Set_Window(0, 0, LCD_Param.Width, LCD_Param.Height);
    LCD_Write_Reg(LCD_Param.WriteGRAM);     /* 0x2C，之后连续写 RAM */
    while (total--) LCD->LCD_RAM = color;
}

/**
 * @brief       设置光标
 * @param       X , Y :起始X，Y坐标
 * @retval      无
 */
void LCD_Set_Cursor(uint16_t X, uint16_t Y)
{
    LCD_Write_Reg(LCD_Param.SetXCmd);
    LCD_Write_Data(X >> 8);
    LCD_Write_Data(X & 0XFF);
    LCD_Write_Reg(LCD_Param.SetYCmd);
    LCD_Write_Data(Y >> 8);
    LCD_Write_Data(Y & 0XFF);
}
/**
 * @brief       选中N * N的窗口
 * @param       StartX,StartY,Width,Height : 起始X坐标,起始Y坐标,宽度,长度
 * @retval      无
 */
void LCD_Set_Window(uint16_t StartX, uint16_t StartY, uint16_t Width, uint16_t Height)
{
    uint16_t EndX, EndY;

    if (StartX >= LCD_Param.Width)  StartX = LCD_Param.Width - 1;
    if (StartY >= LCD_Param.Height) StartY = LCD_Param.Height - 1;
    if (Width  > LCD_Param.Width  - StartX) Width  = LCD_Param.Width  - StartX;
    if (Height > LCD_Param.Height - StartY) Height = LCD_Param.Height - StartY;

    if (Width == 0 || Height == 0) return;

    EndX = StartX + Width  - 1;
    EndY = StartY + Height - 1;

    LCD_Write_Reg(LCD_Param.SetXCmd);       /* CASET */
    LCD_Write_Data(StartX >> 8);  LCD_Write_Data(StartX & 0xFF);
    LCD_Write_Data(EndX >> 8);  LCD_Write_Data(EndX & 0xFF);

    LCD_Write_Reg(LCD_Param.SetYCmd);       /* RASET */
    LCD_Write_Data(StartY >> 8);  LCD_Write_Data(StartY & 0xFF);
    LCD_Write_Data(EndY >> 8);  LCD_Write_Data(EndY & 0xFF);
}

/**
 * @brief       填充N * N的窗口
 * @param       StartX,StartY,Width,Height,color : 起始X坐标,起始Y坐标,宽度,长度,RGB565色
 * @retval      无
 */
void LCD_Fill(uint16_t StartX, uint16_t StartY, uint16_t Width, uint16_t Height, uint16_t color)
{
    uint32_t total;
    if (Width == 0 || Height == 0) return;

    if (StartX >= LCD_Param.Width || StartY >= LCD_Param.Height) return;
    if (StartX + Width  > LCD_Param.Width)  Width  = LCD_Param.Width  - StartX;
    if (StartY + Height > LCD_Param.Height) Height = LCD_Param.Height - StartY;

    total = (uint32_t)Width * Height;
    LCD_Set_Window(StartX, StartY, Width, Height);
    LCD_Write_Reg(LCD_Param.WriteGRAM);
    while (total--) LCD->LCD_RAM = color;
}

/**
 * @brief       画一个点，颜色取当前前景色
 * @param       X, Y : 屏幕坐标
 * @retval      无
 */
void LCD_DrawPoint(uint16_t X, uint16_t Y)
{
    if (X >= LCD_Param.Width || Y >= LCD_Param.Height) return;   /* 越界点直接丢弃 */

    LCD_Set_Window(X, Y, 1, 1);
    LCD_Write_Reg(LCD_Param.WriteGRAM);
    LCD->LCD_RAM = LCD_Param.TextColor;
}

/**
 * @brief       画线 (Bresenham integer algorithm)
 * @param       X0, Y0 : start point
 * @param       X1, Y1 : end point
 * @retval      none
 */
void LCD_DrawLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1)
{
    int16_t x  = (int16_t)X0;
    int16_t y  = (int16_t)Y0;
    int16_t tx = (int16_t)X1;
    int16_t ty = (int16_t)Y1;
    int16_t dx, dy, sx, sy, err, e2;

    if (X0 >= LCD_Param.Width  && X1 >= LCD_Param.Width)  return;
    if (Y0 >= LCD_Param.Height && Y1 >= LCD_Param.Height) return;

    if (Y0 == Y1)
    {
        uint16_t len = (uint16_t)(((X1 > X0) ? (X1 - X0) : (X0 - X1)) + 1);
        LCD_DrawHLine((X0 < X1) ? X0 : X1, Y0, len);
        return;
    }
    if (X0 == X1)
    {
        uint16_t len = (uint16_t)(((Y1 > Y0) ? (Y1 - Y0) : (Y0 - Y1)) + 1);
        LCD_DrawVLine(X0, (Y0 < Y1) ? Y0 : Y1, len);
        return;
    }


    dx = (tx > x) ? (tx - x) : (x - tx);
    dy = (ty > y) ? (ty - y) : (y - ty);
    sx = (x < tx) ? 1 : -1;
    sy = (y < ty) ? 1 : -1;

    dy  = -dy;
    err = dx + dy;

    for (;;)
    {
        LCD_DrawPoint((uint16_t)x, (uint16_t)y);

        if (x == tx && y == ty) break;

        e2 = err << 1;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

/**
 * @brief       画水平线
 * @param       X, Y ,Len: 起始坐标,线段长度(整数)
 * @retval      none
 */
void LCD_DrawHLine(uint16_t X, uint16_t Y, uint16_t Len)
{
    LCD_Fill(X, Y, Len, 1, LCD_Param.TextColor);
}

/**
 * @brief       画垂直线
 * @param       X, Y ,Len: 起始坐标,线段长度(整数)
 * @retval      none
 */
void LCD_DrawVLine(uint16_t X, uint16_t Y, uint16_t Len)
{
    LCD_Fill(X, Y, 1, Len, LCD_Param.TextColor);
}

/**
 * @brief       画实心矩形
 * @param       X, Y : 左上角坐标
 * @param       Width, Height : 尺寸
 * @retval      none
 */
void LCD_FillRect(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height)
{
    LCD_Fill(X, Y, Width, Height, LCD_Param.TextColor);
}

/**
 * @brief       画矩形边框
 * @param       X, Y : 左上角坐标
 * @param       Width, Height : 尺寸
 * @retval      none
 */
void LCD_DrawRect(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height)
{
    if (Width == 0 || Height == 0) return;
    if (X >= LCD_Param.Width || Y >= LCD_Param.Height) return;

    if ((uint32_t)X + Width  > LCD_Param.Width)  Width  = LCD_Param.Width  - X;
    if ((uint32_t)Y + Height > LCD_Param.Height) Height = LCD_Param.Height - Y;
    if (Width == 0 || Height == 0) return;

    LCD_DrawHLine(X, Y, Width);
    LCD_DrawHLine(X, (uint16_t)(Y + Height - 1), Width);

    if (Height > 2)
    {
        LCD_DrawVLine(X, (uint16_t)(Y + 1), (uint16_t)(Height - 2));
        if (Width > 1)
            LCD_DrawVLine((uint16_t)(X + Width - 1), (uint16_t)(Y + 1), (uint16_t)(Height - 2));
    }
}
/**
 * @brief       plot the 8 symmetric points of one circle step
 * @param       cx, cy : circle center
 * @param       dx, dy : offset from center
 * @retval      none
 */
static void LCD_DrawCirclePts(int16_t cx, int16_t cy, int16_t dx, int16_t dy)
{
    LCD_DrawPoint((uint16_t)(cx + dx), (uint16_t)(cy + dy));
    LCD_DrawPoint((uint16_t)(cx + dx), (uint16_t)(cy - dy));
    LCD_DrawPoint((uint16_t)(cx - dx), (uint16_t)(cy + dy));
    LCD_DrawPoint((uint16_t)(cx - dx), (uint16_t)(cy - dy));
    LCD_DrawPoint((uint16_t)(cx + dy), (uint16_t)(cy + dx));
    LCD_DrawPoint((uint16_t)(cx + dy), (uint16_t)(cy - dx));
    LCD_DrawPoint((uint16_t)(cx - dy), (uint16_t)(cy + dx));
    LCD_DrawPoint((uint16_t)(cx - dy), (uint16_t)(cy - dx));
}

/**
 * @brief       画空心圆
 * @param       X, Y : 中心坐标
 * @param       R : 半径
 * @retval      none
 */
void LCD_DrawCircle(uint16_t X, uint16_t Y, uint16_t R)
{
    int16_t cx = (int16_t)X;
    int16_t cy = (int16_t)Y;
    int16_t x  = 0;
    int16_t y;
    int32_t d;

    if (R > 1024) return;
    if (R == 0) { LCD_DrawPoint(X, Y); return; }

    y = (int16_t)R;
    d = 3 - 2 * y;

    while (x <= y)
    {
        LCD_DrawCirclePts(cx, cy, x, y);

        if (d < 0)
        {
            d += 4 * x + 6;
        }
        else
        {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/**
 * @brief       fill a horizontal span, clipped to the panel
 * @param       X0, X1 : span end points (order does not matter),  Y : row
 * @retval      none
 */
static void LCD_SpanClip(int16_t X0, int16_t X1, int16_t Y)
{
    if (Y < 0 || Y >= (int16_t)LCD_Param.Height) return;

    if (X0 > X1) { int16_t t = X0; X0 = X1; X1 = t; }

    if (X1 < 0 || X0 >= (int16_t)LCD_Param.Width) return;
    if (X0 < 0) X0 = 0;
    if (X1 >= (int16_t)LCD_Param.Width) X1 = (int16_t)LCD_Param.Width - 1;

    LCD_Fill((uint16_t)X0, (uint16_t)Y, (uint16_t)(X1 - X0 + 1), 1, LCD_Param.TextColor);
}

/**
 * @brief       画实心圆
 * @param       X, Y : 中心坐标
 * @param       R : 半径
 * @retval      none
 */
void LCD_FillCircle(uint16_t X, uint16_t Y, uint16_t R)
{
    int16_t cx = (int16_t)X;
    int16_t cy = (int16_t)Y;
    int16_t x  = 0;
    int16_t y;
    int32_t d;

    if (R > 1024) return;
    if (R == 0) { LCD_DrawPoint(X, Y); return; }

    y = (int16_t)R;
    d = 3 - 2 * y;

    while (x <= y)
    {
        LCD_SpanClip((int16_t)(cx - y), (int16_t)(cx + y), (int16_t)(cy + x));   /* wide row  */
        if (x != 0) LCD_SpanClip((int16_t)(cx - y), (int16_t)(cx + y), (int16_t)(cy - x));
        LCD_SpanClip((int16_t)(cx - x), (int16_t)(cx + x), (int16_t)(cy + y));   /* short row */
        if (x != y) LCD_SpanClip((int16_t)(cx - x), (int16_t)(cx + x), (int16_t)(cy - y));

        if (d < 0)
        {
            d += 4 * x + 6;
        }
        else
        {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/**
 * @brief       画三角形边框
 * @param       X0,Y0 / X1,Y1 / X2,Y2 : 三个顶点坐标
 * @retval      无
 */
void LCD_DrawTriangle(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1,
                      uint16_t X2, uint16_t Y2)
{
    LCD_DrawLine(X0, Y0, X1, Y1);
    LCD_DrawLine(X1, Y1, X2, Y2);
    LCD_DrawLine(X2, Y2, X0, Y0);
}

/**
 * @brief       显示一幅 RGB565 位图
 * @param       X, Y   : 左上角坐标（允许为负，会自动裁剪）
 * @param       Width  : 图片宽度(像素)
 * @param       Height : 图片高度(像素)
 * @param       Data   : RGB565 数据，按行存放，行内从左到右
 * @retval      无
 */
void LCD_DrawBitmap(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height,
                    const uint16_t *Data)
{
    int32_t x0, y0, x1, y1;      /* 图片在屏幕坐标系下的四条边界，允许为负 */
    int32_t sx, sy, ex, ey;      /* 裁剪之后真正可见的区域 */
    int32_t row, i;
    const uint16_t *p;
    uint32_t total;

    if (Data == 0 || Width == 0 || Height == 0) return;

    x0 = (int32_t)X;
    y0 = (int32_t)Y;
    x1 = (int32_t)X + Width  - 1;
    y1 = (int32_t)Y + Height - 1;

    /* 与屏幕完全没有交集，直接返回 */
    if (x1 < 0 || y1 < 0) return;
    if (x0 >= (int32_t)LCD_Param.Width || y0 >= (int32_t)LCD_Param.Height) return;

    /* 情况一：整幅图都在屏幕内 -- 设一次窗口，连续灌数据，这是最快的路径 */
    if (x0 >= 0 && y0 >= 0 &&
        x1 < (int32_t)LCD_Param.Width && y1 < (int32_t)LCD_Param.Height)
    {
        total = (uint32_t)Width * Height;
        LCD_Set_Window(X, Y, Width, Height);
        LCD_Write_Reg(LCD_Param.WriteGRAM);
        p = Data;
        while (total--) LCD->LCD_RAM = *p++;
        return;
    }

    /* 情况二：部分超出屏幕 -- 逐行裁剪，每行只写可见的那一段 */
    sx = (x0 < 0) ? 0 : x0;
    sy = (y0 < 0) ? 0 : y0;
    ex = (x1 >= (int32_t)LCD_Param.Width)  ? (int32_t)LCD_Param.Width  - 1 : x1;
    ey = (y1 >= (int32_t)LCD_Param.Height) ? (int32_t)LCD_Param.Height - 1 : y1;

    for (row = sy; row <= ey; row++)
    {
        /* 指针定位到本行可见段的第一个像素 */
        p = Data + (row - y0) * (int32_t)Width + (sx - x0);

        LCD_Set_Window((uint16_t)sx, (uint16_t)row, (uint16_t)(ex - sx + 1), 1);
        LCD_Write_Reg(LCD_Param.WriteGRAM);
        for (i = sx; i <= ex; i++) LCD->LCD_RAM = *p++;
    }
}

/*******************************
 *      LCD文本层函数
 *      创建于2026-10-26
 *******************************/

/**
 * @brief       把一块字模画到屏幕
 * @param       X, Y  : 左上角坐标
 * @param       W, H  : 字模宽度、高度
 * @param       Data  : 字模数据，列优先存放
 *                      byte = page * W + col，bit = row % 8，bit0 在该列最上面
 * @retval      无
 * @note        一行只设一次窗口、连续灌 W 个像素。亮点写前景色、底色写背景色，
 *              所以同一个位置反复刷新不会留下残影
 */
static void LCD_DrawFont(uint16_t X, uint16_t Y, uint16_t W, uint16_t H,
                         const uint8_t *Data)
{
    uint16_t line[32];                          /* 一行的像素缓冲，最大 32 像素宽 */
    uint16_t fg = LCD_Param.TextColor;
    uint16_t bg = LCD_Param.BackColor;
    uint16_t cw, ch, r, c;
    uint8_t  page, bit;

    if (Data == 0 || W == 0 || H == 0 || W > 32) return;
    if (X >= LCD_Param.Width || Y >= LCD_Param.Height) return;

    /* 右边和下边超出屏幕的部分先裁掉，保证窗口宽度和实际写入的像素个数一致 */
    cw = (uint16_t)(LCD_Param.Width  - X);  if (cw > W) cw = W;
    ch = (uint16_t)(LCD_Param.Height - Y);  if (ch > H) ch = H;

    for (r = 0; r < ch; r++)
    {
        page = (uint8_t)(r >> 3);
        bit  = (uint8_t)(1u << (r & 7));

        for (c = 0; c < cw; c++)
            line[c] = (Data[page * W + c] & bit) ? fg : bg;

        LCD_Set_Window(X, (uint16_t)(Y + r), cw, 1);
        LCD_Write_Reg(LCD_Param.WriteGRAM);
        for (c = 0; c < cw; c++) LCD->LCD_RAM = line[c];
    }
}

/**
 * @brief       显示一个 ASCII 字符
 * @param       X, Y : 左上角坐标
 * @param       Ch   : 要显示的字符 (0x20 ~ 0x7E)
 * @param       Font : LCD_FONT_8X16 / LCD_FONT_16X32
 * @retval      下一个字符的 X 坐标
 */
uint16_t LCD_ShowChar(uint16_t X, uint16_t Y, char Ch, uint8_t Font)
{
    const LCDFont_t *f;
    uint8_t u = (uint8_t)Ch;

    if (Font >= LCD_FONT_NUM) Font = LCD_FONT_8X16;
    f = &LCD_FontTable[Font];

    if (u < 0x20 || u > 0x7E) u = 0x20;         /* 不可显示的字符按空格处理 */
    u = (uint8_t)(u - 0x20);

    LCD_DrawFont(X, Y, f->W, f->H, f->Data + (uint32_t)u * f->Bytes);
    return (uint16_t)(X + f->W);
}

/**
 * @brief       在汉字库里查一个字模
 * @param       Key : UTF-8 三字节汉字
 * @retval      字模指针；字库里没有这个字时返回 0
 */
static const uint8_t *LCD_FindChinese(const char *Key)
{
    const ChineseCell_t *p = LCD_Chinese_16x16;

    while (p->Index[0])
    {
        if (p->Index[0] == Key[0] && p->Index[1] == Key[1] && p->Index[2] == Key[2])
            return p->Data;
        p++;
    }
    return 0;
}

/**
 * @brief       显示一个 16x16/32汉字
 * @param       X, Y  : 左上角坐标
 * @param       Data  : 汉字字模，为 0 表示缺字，画一个空心方框占位
 * @param       FontH : 当前 ASCII 字号的高度，用来把汉字在行内垂直居中
 * @retval      下一个字符的 X 坐标
 */
static uint16_t LCD_ShowChinese(uint16_t X, uint16_t Y, const uint8_t *Data,
                                uint16_t FontH)
{
    uint16_t y = Y;

    if (FontH > 16) y = (uint16_t)(Y + (FontH - 16) / 2);   /* 大号字时居中，避免汉字掉到行底 */

    if (Data != 0)
    {
        LCD_DrawFont(X, y, 16, 16, Data);
    }
    else
    {
        LCD_Fill(X, y, 16, 16, LCD_Param.BackColor);
        LCD_DrawRect(X, y, 16, 16);                         /* 缺字：画个空心方框 */
    }
    return (uint16_t)(X + 16);
}

/**
 * @brief       显示一个字符串（UTF-8 编码，ASCII 与汉字可以混排）
 * @param       X, Y : 左上角坐标
 * @param       Str  : 要显示的字符串，以 '\0' 结尾
 * @param       Font : LCD_FONT_8X16 / LCD_FONT_16X32
 * @retval      下一个字符的 X 坐标
 * @note        本函数不自动换行、也不解析 '\n'，需要换行用返回值自己处理 Y
 */
uint16_t LCD_ShowString(uint16_t X, uint16_t Y, const char *Str, uint8_t Font)
{
    const LCDFont_t *f;
    uint16_t x = X;
    uint8_t  ch;

    if (Str == 0) return X;
    if (Font >= LCD_FONT_NUM) Font = LCD_FONT_8X16;
    f = &LCD_FontTable[Font];

    while (*Str)
    {
        ch = (uint8_t)*Str;

        if (ch < 0x80)                                          /* 单字节：ASCII */
        {
            x = LCD_ShowChar(x, Y, (char)ch, Font);
            Str++;
        }
        else if ((ch & 0xE0) == 0xC0 && Str[1] != 0)            /* 两字节 UTF-8：没有字模，画空框 */
        {
            x = LCD_ShowChinese(x, Y, 0, f->H);
            Str += 2;
        }
        else if ((ch & 0xF0) == 0xE0 && Str[1] != 0 && Str[2] != 0)   /* 三字节 UTF-8：查汉字库 */
        {
            x = LCD_ShowChinese(x, Y, LCD_FindChinese(Str), f->H);
            Str += 3;
        }
        else                                                    /* 四字节或残缺字节：跳过 */
        {
            Str++;
        }
    }
    return x;
}

/**
 * @brief       显示一个无符号十进制数
 * @param       X, Y : 左上角坐标
 * @param       Num  : 要显示的数字 (0 ~ 4294967295)
 * @param       Len  : 显示几位。不足时左边补空格；超出时只显示低 Len 位
 * @param       Font : 字号
 * @retval      下一个字符的 X 坐标
 */
uint16_t LCD_ShowNum(uint16_t X, uint16_t Y, uint32_t Num, uint8_t Len,
                     uint8_t Font)
{
    char     buf[11];
    uint8_t  digits = 0, i;
    uint32_t n = Num;

    if (Len == 0 || Len > 10) Len = 10;         /* uint32 最多 10 位十进制 */

    do { digits++; n /= 10; } while (n != 0);   /* 先数出真实位数 */
    if (digits > Len) digits = Len;             /* 太长就只保留低位 */

    for (i = 0; i < Len; i++) buf[i] = ' ';     /* 左边补空格 */
    buf[Len] = '\0';

    n = Num;
    for (i = Len; i > 0 && digits > 0; i--)     /* 从右往左填低位 */
    {
        buf[i - 1] = (char)('0' + (n % 10));
        n /= 10;
        digits--;
    }

    return LCD_ShowString(X, Y, buf, Font);
}

/**
 * @brief       格式化输出到屏幕，用法参考printf
 * @param       X, Y : 左上角起点
 * @param       Font : LCD_FONT_8X16 / LCD_FONT_16X32
 * @param       Fmt  : 格式字符串，支持 %d %u %x %s %c %f 等
 * @param       ...  : 与格式串对应的参数
 * @retval      下一个字符的 X 坐标
 * @note        1) 先把 (X,Y) 到屏幕右边这一整条用背景色擦掉，再写字，
 *                 所以数值变短（比如 100 变成 9）不会留下上一次的残影；
 *              2) 缓冲区是 static 的 128 字节，超长会被截断；不重入，
 *                 不要在中断里和主循环里同时调用；
 *              3) 不自动换行、不滚屏，要换行用返回值自己算 Y
 */
uint16_t LCD_printf(uint16_t X, uint16_t Y, uint8_t Font, const char *Fmt, ...)
{
    static char buf[128];
    va_list ap;

    if (Fmt == 0) return X;
    if (Font >= LCD_FONT_NUM) Font = LCD_FONT_8X16;

    va_start(ap, Fmt);
    vsnprintf(buf, sizeof(buf), Fmt, ap);
    va_end(ap);

    /* 先擦掉整条，数字变短时不会残留高位 */
    LCD_Fill(X, Y, (uint16_t)(LCD_Param.Width - X), LCD_FontTable[Font].H,
             LCD_Param.BackColor);

    return LCD_ShowString(X, Y, buf, Font);
}

/*******************************
 *      LCD帧率显示
 *      创建于2026-10-26
 *******************************/
/**
 * @brief       实时帧率显示，每 1 秒更新一次数字
 * @param       X, Y : 显示位置左上角
 * @param       Font : LCD_FONT_8X16 / LCD_FONT_16X32
 * @retval      下一个字符的 X 坐标
 * @note        放在主循环末尾，每画完一帧调用一次即可，不需要任何初始化。
 *              函数内部自己数帧：满 1 秒结算一次，把结果画在 (X,Y)。
 *              计时用 SysTick（HAL_GetTick），1ms 分辨率对 1 秒窗口的理论误差
 *              只有 0.1%，不用额外开定时器、也不用中断。
 */
uint16_t LCD_ShowFPS(uint16_t X, uint16_t Y, uint8_t Font)
{
    static uint8_t  Inited = 0;         /* 第一次调用时记录窗口起点 */
    static uint32_t Count  = 0;         /* 当前窗口内已画完的帧数 */
    static uint32_t Tick0  = 0;         /* 当前窗口的起点(ms) */
    static uint32_t Fps    = 0;         /* 最近一次结算出的帧率 */

    uint32_t now = HAL_GetTick ();

    Count++;                            /* 本次调用代表画完了一帧 */

    if (Inited == 0)
    {
        Inited = 1;
        Tick0  = now;
    }
    else if ((uint32_t)(now - Tick0) >= 1000u)             /* 满 1 秒结算 */
    {
        Fps   = Count * 1000u / (now - Tick0);             /* 用真实间隔算，不会累积误差 */
        Count = 0;
        Tick0 = now;
    }

    return LCD_printf(X, Y, Font, "FPS:%3lu", (unsigned long)Fps);
}

/**
 * @brief       LCD初始化
 * @param
 * @retval      无
 */
void LCD_Init(uint16_t TextColor, uint16_t BackColor)
{
    LCD_Write_Reg(0XD3);
    LCD_Param.ID = LCD_Read_Data();  /* dummy read */
    LCD_Param.ID = LCD_Read_Data();  /* 读到0X00 */
    LCD_Param.ID = LCD_Read_Data();  /* 读取0X77 */
    LCD_Param.ID <<= 8;
    LCD_Param.ID |= LCD_Read_Data(); /* 读取0X96 */
    // printf("LCD ID: %04X\r\n", LCD_Param.ID);
    // if (LCD_Param.ID != 0x7796) { printf("ID error!\r\n"); return; }

    LCD_ST7796_RegInit();

    LCD_Param.Dir       = 0;      /* 竖屏 */
    LCD_Param.Width     = 320;
    LCD_Param.Height    = 480;
    LCD_Param.WriteGRAM = 0x2C;
    LCD_Param.SetXCmd   = 0x2A;
    LCD_Param.SetYCmd   = 0x2B;
    LCD_Param.TextColor = TextColor;
    LCD_Param.BackColor = BackColor;

    LCD_TP_Init();                      /* 0=ID 匹配成功，1=I2C 读 ID 失败或不是 GT911 */

    LCD_Clear(BackColor);
}
/********************************触摸中断*********************************/

/**
 * @brief       EXTI 中断回调
 * @param       GPIO_Pin : 触发中断的引脚
 * @retval      无
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LCD_T_INT_Pin) TP_IntFlag = 1;
}

/**
 * @brief       触摸扫描(中断驱动)，主循环每轮调用一次
 * @param       无
 * @retval      当前触摸点数, 0=无触摸
 */
uint8_t LCD_T_ScanIT(void)
{
    uint8_t status, buf[4], zero = 0, i, cnt;

    if (!TP_IntFlag) return TP_Data.Count;        /* 无中断：直接返回上次状态 */
    TP_IntFlag = 0;                               /* 先清标志，扫描期间来的新边沿不会丢 */

    if (LCD_T_Read(GT9XXX_GSTID_REG, &status, 1)) return 0;

    if (!(status & 0x80)) return TP_Data.Count;   /* bit7=0, 无新数据 */

    cnt = status & 0x0F;
    LCD_T_Write(GT9XXX_GSTID_REG, &zero, 1);      /* 清就绪标志，GT911 才会拉低 INT */

    if (cnt == 0) { TP_Data.Count = 0; return 0; }/* 抬起 */
    if (cnt > GT_MAX_TOUCH) cnt = GT_MAX_TOUCH;

    for (i = 0; i < cnt; i++)
    {
        if (LCD_T_Read(GT9XXX_TP1_REG + 8 * i, buf, 4)) break;   /* 读失败：停止，已读到的点仍然有效 */

        /* 竖屏(Dir=0)直接用原始值, 不翻转 */
        TP_Data.X[i] = ((uint16_t)buf[1] << 8) | buf[0];
        TP_Data.Y[i] = ((uint16_t)buf[3] << 8) | buf[2];

        /* 边界毛刺钳到屏幕内，不再丢弃整点 */
        if (TP_Data.X[i] >= LCD_Param.Width)  TP_Data.X[i] = LCD_Param.Width  - 1;
        if (TP_Data.Y[i] >= LCD_Param.Height) TP_Data.Y[i] = LCD_Param.Height - 1;
    }

    if (i < cnt) cnt = i;                         /* 中途读失败时，后面的点不算数 */
    TP_Data.Count = cnt;
    return cnt;
}
