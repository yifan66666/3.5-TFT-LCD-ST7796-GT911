# LCD 驱动层说明

ST7796（320×480，16 位 FSMC）+ GT911（软件 I2C）驱动，配套字库与文本层。
面向 STM32F103ZET6，HAL 库，CMake/CLion 工程。**无帧缓冲，所有绘制直接写 GRAM。**

## 文件构成

| 文件 | 作用 | 是否需要改 |
|---|---|---|
| `LCD.c` | 驱动实现：FSMC 读写、ST7796 初始化、几何层、文本层、GT911 触摸 | 一般不改 |
| `LCD.h` | 对外接口、颜色宏、引脚/FSMC 参数、GT911 寄存器定义 | 只有换屏或换片选才改 |
| `LCD_Font.c` | 字模数据：ASCII 8×16 / 16×32，汉字 16×16 | **加汉字改这里** |
| `LCD_Font.h` | 字模结构与字号宏 `LCD_FONT_8X16` / `LCD_FONT_16X32` | 加字号才改 |

## 应用范围

**适合**

- 静态界面：文字、按钮、状态数值、进度条、图标（位图）
- 局部刷新：只重画变化的那块矩形，典型耗时在毫秒以内
- 触摸交互：点击、长按、滑动、画板（配合 `Hardware/UI` 的事件层）
- 调试信息上屏：`LCD_printf` 替代不可用的 `printf`

**不适合**

- 全屏动画 / 视频：没有帧缓冲和 DMA。全屏写一次 12.8 ms，即总线极限约 78 fps；实测 `-O0` 下整屏重绘约 32 fps，`-Os` 约 45 fps，且 CPU 全程被占住
- 半透明、图层、抗锯齿：硬件只有 RGB565 直写，没有合成能力
- 复杂中文排版：字库是按需取模的，只有你手工加进去的字
- 多任务同时写屏：驱动没有加锁，也不可重入

## 快速上手

```c
LCD_Init(WHITE, BLACK);              /* 前景白、背景黑，内部会按背景色清屏 */
LCD_ShowString(10, 10, "Hello", LCD_FONT_8X16);
LCD_printf(10, 40, LCD_FONT_8X16, "cnt=%d", cnt);

while (1)
{
    uint8_t n = LCD_T_ScanIT();      /* 无触摸时只是一次变量判断，不碰 I2C */
    if (n) { /* TP_Data.X[0], TP_Data.Y[0] */ }
}
```

- **坐标系**：原点左上，X 向右 `0~319`，Y 向下 `0~479`（竖屏）。触摸坐标与之完全一致，不需要任何翻转。
- **颜色**：RGB565。`LCD.h` 里有 `WHITE/BLACK/RED/GREEN/BLUE/...`。
- **两种颜色状态**：`LCD_Param.TextColor`（前景）/ `LCD_Param.BackColor`（背景），用 `LCD_SetTextColor` / `LCD_SetBackColor` 修改。

## 接口分组

**底层（都是 `static`，外部不需要调用）**
`LCD_Write_Reg` `LCD_Write_Data` `LCD_Read_Data` `LCD_ST7796_RegInit`，以及触摸的软件 I2C 全套（`LCD_I2C_Start/Stop/SendByte/ReceiveByte/SendAck/ReceiveAck`）、`LCD_T_Read` `LCD_T_Write` `LCD_T_Reset` `LCD_TP_Init`。

**屏幕与颜色**
`LCD_Init` `LCD_Clear` `LCD_SetTextColor` `LCD_SetBackColor` `LCD_Set_Cursor` `LCD_Set_Window` `LCD_Fill`

> `LCD_Set_Cursor` 只写起始地址（0x2A/0x2B，不带结束地址），一般直接用 `LCD_Set_Window` 更安全。

**几何层（颜色取 `TextColor`；`LCD_Fill` 需显式传色，`LCD_DrawBitmap` 用位图自身颜色）**
`LCD_DrawPoint` `LCD_DrawLine` `LCD_DrawHLine` `LCD_DrawVLine` `LCD_FillRect` `LCD_DrawRect` `LCD_DrawCircle` `LCD_FillCircle` `LCD_DrawTriangle` `LCD_DrawBitmap`

**文本层**
`LCD_ShowChar` `LCD_ShowString`（UTF-8，ASCII 与汉字混排）`LCD_ShowNum` `LCD_printf`

**其他**
`LCD_ShowFPS`（每帧调一次，每秒更新）`LCD_T_ScanIT`（触摸，主循环调用）`TP_Data` `LCD_TP_State`

## 注意事项

### 显示

1. **没有帧缓冲**。任何绘制都是立即写屏，不存在"画完再一起显示"。想不闪，就只重画变化的区域，不要整屏重绘。
2. **绝大多数函数用当前前景色，只有 `LCD_Fill` 和 `LCD_DrawBitmap` 例外**：
   - 用 `TextColor` 的：`DrawPoint` `DrawLine` `DrawHLine` `DrawVLine` `FillRect` `DrawRect` `DrawCircle` `FillCircle` `DrawTriangle` `ShowChar` `ShowString` `ShowNum` `printf`
   - 需显式传色的：`LCD_Fill`（多一个 `color` 参数）、`LCD_DrawBitmap`（颜色来自位图数据）
   注意 `FillRect` 用的是**前景色**，想用别的颜色填充就先 `LCD_SetTextColor`，画完记得改回来。图画到一半改 `TextColor`，颜色会从下一个函数开始变，这是预期行为。
3. **越界处理不统一**：`LCD_DrawPoint` 直接丢弃越界点（`DrawLine` / `DrawCircle` / `DrawTriangle` 都建立在它之上，所以屏外部分自然不画）；`Fill` / `FillRect` / `FillCircle` / `DrawBitmap` 会先裁剪再写，不会越界。传负坐标时会被当作很大的 `uint16_t`，因此**不要传负数**（`DrawBitmap` 例外，它用 `int32_t` 计算，支持负坐标裁剪）。
4. **`LCD_Set_Window` 会归一化参数**：起点越界会被夹到边缘、宽高被夹到剩余空间、宽或高为 0 直接返回。手工调它之后紧跟 `LCD_Write_Reg(LCD_Param.WriteGRAM)` 才能连续灌数据，否则后续写入落到哪由上一次的窗口决定。
5. **`LCD_DrawBitmap` 数据格式**：RGB565、行优先、无透明色，字节序为小端（即直接 `uint16_t` 数组）。整幅图在屏内时走快速路径，超出屏幕时逐行裁剪，不会越界访问。
6. **`LCD_printf` 三个硬约束**：
   - 缓冲区是 `static char buf[128]`，**不占栈但不可重入**——不要在中断里和主循环里同时调用；
   - 会先把 `(X,Y)` 到屏幕右边整条用背景色擦掉再写字，所以数值变短不留残影，但在图形上写字会擦出一条黑带；
   - 超过 127 字节会被截断，不自动换行、不滚屏。
7. **`LCD_ShowFPS` 的数字包含它自己的绘制耗时**，是"这套循环的真实帧率"，不是纯刷屏理论值。第一次调用那一秒显示 `FPS:  0`，正常。
8. **`printf` 仍然不可用**。`syscalls.c` 里的 `__io_putchar` 只有 weak 声明，直接调 `printf` 会 HardFault。要么实现 `__io_putchar` 发串口，要么用 `LCD_printf`。

### 文本

9. **汉字库只有 `，。你好世界` 6 个字**，其余汉字会画成 16×16 空心方框。加字的流程：取模软件（阴码、列行式、顺向）→ 追加到 `LCD_Font.c` 的 `LCD_Chinese_16x16[]`，`Index` 填汉字本身。
10. **`main.c` 必须用 UTF-8 保存**。汉字靠 UTF-8 字节序列在字库里匹配，改成 GBK 会全部变成空心方框。
11. **汉字固定 16×16**，用 `LCD_FONT_16X32` 显示时会自动下移 8 像素垂直居中，不会掉到行底。想要更大的汉字需要新字库和新的字号宏。
12. **字号宏越界会被兜底**：`Font >= LCD_FONT_NUM` 时按 `LCD_FONT_8X16` 处理，不会崩。ASCII 范围外的字符按空格处理。

### 触摸

13. **`LCD_T_ScanIT` 必须在主循环里轮询调用**。它只在 `TP_IntFlag`（EXTI 回调置 1）为 1 时才读 I2C，所以静止时开销接近 0；触摸数据本身在中断里只置标志，不做 I2C。
14. **INT 引脚有两条硬要求**，违反任一条都会表现为"完全无触摸、`0x814E` 恒 0"：
    - 复位期间必须是**推挽输出拉高**（GT911 靠它锁存 I2C 地址 0x28），不能用内部上拉代替；
    - 工作期间必须是**悬浮输入 + 上升沿 EXTI**（`GPIO_NOPULL`）。一直上拉会让 GT911 工作异常。
    这两条由 `LCD_T_Reset()` 在运行时保证，CubeMX 里 PF10 应配 `GPIO_EXTI10 / External Interrupt Mode with Rising edge / No pull`。
15. **禁止软件复位**（`0x8040 = 0x02`）。它会重载 186 字节配置，且手册要求等待 >50 ms，代码只等 10 ms 会打断加载，配置区变空（表现为 `0x8048` 量程读到 0）。硬件复位已经足够。
16. **`TP_Data` 是全局共享的**，`Count` 为 0 表示已抬起。坐标越界会被钳到屏幕边缘，不会丢点。
17. **`LCD_TP_State`** 记录初始化结果，`0` = ID 匹配成功，非 0 = I2C 读 ID 失败或不是 GT911。

### 构建与配置

18. **FSMC 时序已优化**：`AddressSetupTime = 1`、`DataSetupTime = 3`（模式 A，6 HCLK = 83.3 ns/像素）。已存入 `.ioc`，CubeMX 重新生成不会丢。如果哪天觉得刷屏变慢了，先回 `Core/Src/fsmc.c` 核对这两个值。
19. **换片选或换地址线**要同时改 `LCD_FSMC_NEX` / `LCD_FSMC_AX` 和 CubeMX 里的 CS/RS 引脚。
20. **`LCD.c` / `LCD.h` 是 UTF-8 + LF**，CubeMX 生成的文件是 UTF-8 + CRLF，混用不影响编译，但别按 GBK 处理。
21. **Debug 默认是 `-O0`**，填充内层循环比 `-Os` 多出约 5 倍指令，刷屏帧率会明显偏低。要测真实帧率先切 Release 或把 `cmake/gcc-arm-none-eabi.cmake` 里的 `CMAKE_C_FLAGS_DEBUG` 改成 `-O1 -g3`。

## 常见现象对照

| 现象 | 优先检查 |
|---|---|
| 画面整体左右镜像 | `MADCTL(0x36)`，应为 `0x08`（MX=0 / MY=0 / BGR=1） |
| 触摸完全无响应 | PF10 是否 `IT_RISING + NOPULL`；`LCD_T_Reset` 是否被改回上拉；`LCD_TP_State` 是否非 0 |
| 触摸正常但读不到坐标 | 是否忘了主循环调 `LCD_T_ScanIT()`；`0x814E` 的 bit7 是否为 1 |
| 汉字显示成方框 | 字库是否包含该字；`main.c` 是否 UTF-8 |
| 数值刷新有残影 | 用 `LCD_printf`（自带擦行），不要用 `LCD_ShowNum` 反复刷新同一处 |
| 刷屏特别慢 | FSMC 时序是否被改回 15/255；是否还在 `-O0` |
| 调 `printf` 就死机 | `__io_putchar` 未实现，改用 `LCD_printf` |

## 重新生成 CubeMX 时重点核对

1. PF10：`GPIO_EXTI10` + 上升沿 + `No pull`，User Label 必须仍是 `LCD_T_INT`（否则 `LCD.c` 里的宏名对不上）。
2. NVIC：`EXTI line[15:10]` 已使能，优先级低于 TIM2。
3. FSMC：NE4 + A10 + 16 位 + 模式 A，`ADDSET=1` / `DATAST=3`。
4. 背光 PB0 默认高电平（`gpio.c` 里置高，没有控制 API，需要调光得自己写 PWM）。
