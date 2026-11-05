# LCD 驱动层说明

ST7796（320×480，16 位 FSMC）+ GT911（软件 I2C）驱动，配套字库与文本层。
面向 STM32F103ZET6，HAL 库，CMake/CLion 工程。**无帧缓冲，所有绘制直接写 GRAM。**

> **与本目录 `LCD_REVIEW.md` 的分工**：本文件描述驱动**应有的状态**（接口、约束、正确配置、现象对照），
> 是长期维护的"事实来源"；`LCD_REVIEW.md` 是**查错清单**，每次 review 重写、只留当前待改项。
> review 确认的问题修完后，落到本文件的对应条目里，不回写历史过程。

## 文件构成

| 文件 | 作用 | 是否需要改 |
|---|---|---|
| `LCD.c` | 驱动实现：FSMC 读写、ST7796 初始化、几何层、文本层、GT911 触摸 | 一般不改 |
| `LCD.h` | 对外接口、颜色宏、引脚/FSMC 参数、GT911 寄存器定义 | 只有换屏或换片选才改 |
| `LCD_Font.c` | 字模数据：ASCII 8×16 / 16×32，汉字 16×16 | **加汉字改这里** |
| `LCD_Font.h` | 字模结构与字号宏 `LCD_FONT_8X16` / `LCD_FONT_16X32` | 加字号才改 |

## 引脚表（换板或重新生成时照这张表核对）

| 信号 | 引脚 | 模式 / 复用 | CubeMX 里怎么配 | 备注 |
|---|---|---|---|---|
| LCD_CS | PG12 | FSMC_NE4 | `NorPsramChipSelect4_1` | 与 `LCD_FSMC_NEX = 4` 对应 |
| LCD_RS(DC) | PG0 | FSMC_A10 | `FSMC_A10` | 与 `LCD_FSMC_AX = 10` 对应 |
| LCD_WR | PD5 | FSMC_NWE | 默认 | |
| LCD_RD | PD4 | FSMC_NOE | 默认 | 只读 ID/状态时用 |
| LCD_D0 | PD14 | FSMC_D0 | 16 位 | |
| LCD_D1 | PD15 | FSMC_D1 | 16 位 | |
| LCD_D2 | PD0 | FSMC_D2 | 16 位 | |
| LCD_D3 | PD1 | FSMC_D3 | 16 位 | |
| LCD_D4 | PE7 | FSMC_D4 | 16 位 | |
| LCD_D5 | PE8 | FSMC_D5 | 16 位 | |
| LCD_D6 | PE9 | FSMC_D6 | 16 位 | |
| LCD_D7 | PE10 | FSMC_D7 | 16 位 | |
| LCD_D8 | PE11 | FSMC_D8 | 16 位 | |
| LCD_D9 | PE12 | FSMC_D9 | 16 位 | |
| LCD_D10 | PE13 | FSMC_D10 | 16 位 | |
| LCD_D11 | PE14 | FSMC_D11 | 16 位 | |
| LCD_D12 | PE15 | FSMC_D12 | 16 位 | |
| LCD_D13 | PD8 | FSMC_D13 | 16 位 | |
| LCD_D14 | PD9 | FSMC_D14 | 16 位 | |
| LCD_D15 | PD10 | FSMC_D15 | 16 位 | D0~D15 顺序错位会全屏花 |
| LCD_BL | PB0 | **TIM3_CH3** | `S_TIM3_CH3` → `PWM Generation3 CH3`，PSC=35 / ARR=99 | 20 kHz PWM，1 格 = 1%，`CCR` 即百分比；上电默认 100% |
| LCD_T_SCL | PB1 | GPIO 输出 | 推挽 / 上拉 / 高速 / 默认低 | 软件 I2C 时钟 |
| LCD_T_SDA | PF9 | GPIO 输出 | **开漏** / 上拉 / 高速 / 默认低 | 软件 I2C 数据，靠上拉拉高 |
| LCD_T_RST | PF11 | GPIO 输出 | 推挽 / 上拉 / 高速 / 默认低 | 只接 GT911 |
| LCD_T_INT | PF10 | GPIO_EXTI10 | 上升沿 / **No pull** / 开 EXTI 中断 | User Label 必须还是 `LCD_T_INT` |

补充：`ST7796` 这一侧**没有复位引脚接到 MCU**，只有 GT911 的 `LCD_T_RST`（PF11）。所以：
- 按 NRST 热复位后，屏可能停在上一次的花屏状态（没有 RST 就没有硬件复位，`0x11` 软退出睡眠也救不回来），**断电重上电**即可；
- 如果整机电源上电斜率过慢，ST7796 可能没完成内部上电复位，同样只能断电恢复；
- 若你的模块把 ST7796 的 RST 和排针共用了，可以一个 GPIO 同时复位两颗芯片，但 `LCD_T_Reset()` 里 GT911 那套 ①②③④ 时序必须原样保留。

## 应用范围

**适合**

- 静态界面：文字、按钮、状态数值、进度条、图标（位图）
- 局部刷新：只重画变化的那块矩形，典型耗时在毫秒以内
- 触摸交互：点击、长按、滑动、画板（`Hardware/UI` 里有一份事件层示例，默认未接入 `main.c`）
- 调试信息上屏：`LCD_printf` 替代不可用的 `printf`

**不适合**

- 全屏动画 / 视频：没有帧缓冲和 DMA。全屏写一次 12.8 ms，即总线极限约 78 fps；实测 `-O0` 下整屏重绘 32 fps，当前 Debug 默认的 `-O1` 更高，但 CPU 全程被占住
- 半透明、图层、抗锯齿：硬件只有 RGB565 直写，没有合成能力
- 复杂中文排版：字库是按需取模的，只有你手工加进去的字
- 多任务同时写屏：驱动没有加锁，也不可重入

## 快速上手

```c
uint32_t cnt = 0;

LCD_Init(WHITE, BLACK);              /* 前景白、背景黑，内部会按背景色清屏 + 背光置 100% */
LCD_ShowString(10, 10, "Hello", LCD_FONT_8X16);   /* 静态内容只画一次 */
LCD_BackLight(60);                   /* 嫌太亮就调，0~100（%），只改占空比不改频率 */

while (1)
{
    uint8_t n = LCD_T_ScanIT();      /* 无触摸时只是一次变量判断，不碰 I2C */
    if (n) { /* TP_Data.X[0], TP_Data.Y[0] */ }

    LCD_printf(10, 40, LCD_FONT_8X16, "cnt=%lu", (unsigned long)cnt++);
}
```

- **坐标系**：原点左上，X 向右、Y 向下。默认竖屏为 `0~319` / `0~479`，横屏为 `0~479` / `0~319`；尺寸一律用 `LCD_Param.Width` / `LCD_Param.Height` 取，不要写死。触摸坐标已经按当前方向变换好，与绘图坐标完全一致，不需要任何翻转。
- **颜色**：RGB565。`LCD.h` 里有 `WHITE/BLACK/RED/GREEN/BLUE/...`。
- **两种颜色状态**：`LCD_Param.TextColor`（前景）/ `LCD_Param.BackColor`（背景），用 `LCD_SetTextColor` / `LCD_SetBackColor` 修改。

## 接口分组

**底层（都是 `static`，外部不需要调用）**
显示：`LCD_Write_Reg` `LCD_Write_RegData` `LCD_Write_Data` `LCD_Read_Data` `LCD_ST7796_RegInit`。
触摸：软件 I2C 全套（`LCD_Write_SCL/SDA`、`LCD_Read_SDA`、`LCD_I2C_DelayInit/Delay`、`LCD_I2C_Start/Stop/SendByte/ReceiveByte/SendAck/ReceiveAck`）、`LCD_T_Read` `LCD_T_Write` `LCD_T_Reset` `LCD_TP_Init`。
背光：`LCD_BackLightInit`（启动 TIM3_CH3 并置 100%，由 `LCD_Init` 调用）。
内部辅助：`LCD_DrawFont` `LCD_FindChinese` `LCD_ShowChinese` `LCD_SpanClip` `LCD_DrawCirclePts` `LCD_TraceGet` `LCD_TracePut`。

**屏幕与颜色**
`LCD_Init` `LCD_Clear` `LCD_SetTextColor` `LCD_SetBackColor` `LCD_Set_Cursor` `LCD_Set_Window` `LCD_Fill`

**背光**
`LCD_BackLight(Duty)` —— 参数 `0~100` 表示占空比百分比，`0` = 全灭、`100` = 最亮（默认），超过 100 按 100 处理。
只改 `CCR` 不动 `ARR`/`PSC`，所以调亮度时频率恒定 20 kHz，不会出现闪频或"越暗越闪"。
因为 `ARR=99`（一个周期 100 格），函数体里就是一次 `CCR = Duty`，没有换算、没有中间变量。
PWM 的启动与上电默认值由 `LCD_Init()` 内部完成，**外部只需要这一个函数**。

> 前提：`main()` 里 `MX_TIM3_Init()` 必须在 `LCD_Init()` 之前调用（它负责开 TIM3 时钟、把 PB0 切成复用推挽）。

> `LCD_Set_Cursor` 只写起始地址（0x2A/0x2B，不带结束地址），结束坐标沿用上一次窗口的设置，单独调用很容易写错位置，直接用 `LCD_Set_Window` 更安全。
>
> **显示方向**只用 `LCD_SetRotation(0~3)` 切换，`LCD.h` 里的 `LCD_DEFAULT_ROT` 决定上电默认方向（默认竖屏）。`LCD_Param.Dir` 由它维护，**不要直接赋值**——只改 `Dir` 不会写 MADCTL，画面不会转；只改 `Width/Height` 会让窗口与裁剪错位；只改 MADCTL 而没换 W/H 会露黑边。

**几何层（颜色取 `TextColor`；`LCD_Fill` 需显式传色，`LCD_DrawBitmap` 用位图自身颜色）**
`LCD_DrawPoint` `LCD_DrawLine` `LCD_DrawHLine` `LCD_DrawVLine` `LCD_FillRect` `LCD_DrawRect` `LCD_DrawCircle` `LCD_FillCircle` `LCD_DrawTriangle` `LCD_DrawBitmap`

**批量灌像素**
`LCD_WritePixels(X, Y, W, H, Data)` —— 把一块行优先的 RGB565 数组灌进屏幕，直接把 `uint16_t` 数组写进去，不需要任何字节交换。`W` 是**源缓冲区的每行像素数**，裁剪后也不变。

> **宽度没被裁时**走最快路径：设一次窗口 + 连续写 `W*H` 个像素。
> **跨右边界时**宽被裁小，源数据仍按原始 `W` 分行存放，所以改为逐行重新定位指针、每行设一次窗口（与 `LCD_DrawBitmap` 行为一致），画面不会斜切。行数多时设窗次数等于可见行数，相对整块像素量可忽略。
> 行数被裁（跨下边界）不需要特殊处理，直接少写几行即可。
> 矩形越界或 `W`/`H` 为 0 时不写任何像素，也不需要调用方预先裁剪。

**文本层**
`LCD_ShowChar` `LCD_ShowString`（UTF-8，ASCII 与汉字混排）`LCD_ShowNum` `LCD_printf` `LCD_TextW`（算文本像素宽度，排版/居中用）

**其他**
`LCD_ShowFPS`（每帧调一次，每秒更新）`LCD_T_ScanIT`（触摸，主循环调用）`TP_Data` `LCD_TP_State` `LCD_Panel_State`

## 最常用的几个入口

| 用途 | 接口 |
|---|---|
| 初始化 | `LCD_Init(前景色, 背景色)`，之后 `LCD_Panel_State` / `LCD_TP_State` 都为 0 才算正常；背光同时被置到 100% |
| 静态文字 | `LCD_ShowString(x, y, "文本", 字号)`，只在初始化时调一次 |
| 变化的数值 | `LCD_printf(x, y, 字号, fmt, ...)`，同一位置反复覆盖不产生擦除痕迹 |
| 取触摸 | `LCD_T_ScanIT()` 返回点数，坐标在 `TP_Data.X[0]` / `TP_Data.Y[0]` |
| 调背光 | `LCD_BackLight(0~100)`，单位是占空比百分比 |

## 注意事项

### 显示

1. **没有帧缓冲**。任何绘制都是立即写屏，不存在"画完再一起显示"。想不闪，就只重画变化的区域，不要整屏重绘。
2. **绝大多数函数用当前前景色，只有 `LCD_Fill` / `LCD_DrawBitmap` / `LCD_WritePixels` 例外**：
   - 用 `TextColor` 的：`DrawPoint` `DrawLine` `DrawHLine` `DrawVLine` `FillRect` `DrawRect` `DrawCircle` `FillCircle` `DrawTriangle` `ShowChar` `ShowString` `ShowNum` `printf`
   - 需显式给色的：`LCD_Fill`（多一个 `color` 参数）、`LCD_DrawBitmap` 与 `LCD_WritePixels`（颜色来自你传进去的 `uint16_t` 数组）
   注意 `FillRect` 用的是**前景色**，想用别的颜色填充就先 `LCD_SetTextColor`，画完记得改回来。图画到一半改 `TextColor`，颜色会从下一个函数开始变，这是预期行为。
3. **越界处理不统一**：`LCD_DrawPoint` 直接丢弃越界点（`DrawLine` / `DrawCircle` / `DrawTriangle` 都建立在它之上，所以屏外部分自然不画）；`Fill` / `FillRect` / `FillCircle` / `DrawBitmap` / `WritePixels` 会先裁剪再写，不会越界。**所有函数的坐标形参都是 `uint16_t`，一律不接受负数**：`-1` 在进函数之前就变成 `65535`，`LCD_DrawBitmap` 也会因此被判成"起点已在屏幕外"而直接返回，一个像素都不画。要靠边裁剪请自己算好 X/Y 和宽高。
4. **`LCD_Set_Window` 会归一化参数**：起点越界会被夹到边缘、宽高被夹到剩余空间。**宽或高算出来是 0 时它直接返回，什么都不写——窗口会保持上一次的设置不变**；调用者若以为"窗口已经是我要的那块"接着灌数据，就会全部落进旧窗口。手工调它之后要紧跟 `LCD_Write_Reg(LCD_Param.WriteGRAM)` 才能连续灌数据。
5. **`LCD_DrawBitmap` 数据格式**：RGB565、行优先、无透明色，字节序为小端（即直接 `uint16_t` 数组）。整幅图在屏内时走快速路径，超出屏幕时逐行裁剪，不会越界访问。
6. **`LCD_printf` 四条硬约束**：
   - 缓冲区是 `static char buf[128]`，**不占栈但不可重入**——不要在中断里和主循环里同时调用；
   - **不再整行擦除**。`LCD_DrawFont` 写每个字符时会把格子底色一起写掉，所以只有"本次文本比上次短"时才擦多出来的尾巴，等宽刷新时一个像素都不擦（看不到"先黑一下再出字"）；
   - 这个优化依赖"同一锚点反复写"的记账（4 个槽位，按 `X/Y/Font` 匹配）。如果某块区域被 `LCD_Clear` 或图形函数改写过，又用更短的文本去覆盖，可能留下残余，此时先自己 `LCD_Fill` 一块背景；
   - 超过 127 字节会被截断，不自动换行、不滚屏。拿不准宽度用 `LCD_TextW(Str, Font)` 先算。
7. **`LCD_ShowFPS` 的数字包含它自己的绘制耗时**，是"这套循环的真实帧率"，不是纯刷屏理论值。第一次调用那一秒显示 `FPS:  0`，正常。数字本身也走 `LCD_printf`，刷新时同样会撕裂，介意就放到角落。
8. **`printf` 仍然不可用**。`syscalls.c` 里的 `__io_putchar` 只有 weak 声明，直接调 `printf` 会 HardFault。要么实现 `__io_putchar` 发串口，要么用 `LCD_printf`。

### 显示方向

- **切方向只改一行**：`LCD.h` 里的 `LCD_DEFAULT_ROT`（默认 `LCD_ROT_0` 竖屏）。可选 `LCD_ROT_0` 竖屏 320×480 / `LCD_ROT_90` 横屏 480×320 / `LCD_ROT_180` 竖屏倒置 / `LCD_ROT_270` 横屏反向。运行中也能调 `LCD_SetRotation()`，但会清屏（约 12.8 ms），界面需自己重画。
- **它一次做四件事**：写 MADCTL(0x36) → 交换 `Width/Height` → 切换触摸坐标变换 → 清空 `LCD_printf` 的 4 个擦除锚点并清屏。漏一件的现象：只写 MADCTL → 画面转了、触摸还在原位（点不准）；只换 W/H → 右边/下边留黑边；不清锚点 → 转屏后第一次 `printf` 按旧坐标擦错行。
- **MADCTL 固定取 `0x08 / 0x68 / 0xC8 / 0xA8`**（`LCD.c` 的 `MadctlTab[]`），BGR 位 `0x08` 必须保留，丢了红蓝互换。`0x68` 与 `0xA8` 是同一块屏的两个横屏朝向、互差 180°，哪个看着正就用哪个。
- **横屏下画面正、触摸却差 180°**：把 `LCD_T_ScanIT()` 里 `LCD_ROT_90` 与 `LCD_ROT_270` 两条坐标公式对调。纯软件，与配置无关。
- **GT911 的 `0x8048/0x804A` 不要动**：那是面板物理分辨率（320×480），触摸永远按原生方向上报，旋转属于显示侧，改它反而错位。
- **切屏后旧内容全丢**（GRAM 语义变了）。布局里别写死 320/480，用 `LCD_Param.Width/Height` 推算。

### 文本

9. **汉字库只有 `，。你好世界` 6 个字**，其余汉字会画成 16×16 空心方框。加字的流程：取模软件（阴码、列行式、顺向）→ 追加到 `LCD_Font.c` 的 `LCD_Chinese_16x16[]`，`Index` 填汉字本身。
10. **调用 `LCD_ShowString` 的源文件必须用 UTF-8 保存**。汉字靠 UTF-8 字节序列在字库里匹配，存成 GBK 会全部变成空心方框（`main.c` 和 `LCD.c` 里的字符串字面量都算）。
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
    > **I2C 失败会重试，不会把触摸锁死**：读 `0x814E` 或写清零标志连续失败 3 次时，函数**保留 `TP_IntFlag`、返回上次的 `TP_Data.Count`**，主循环下一轮接着试。这一点是必须的——GT911 要等 `0x814E` 被读走才拉低 INT，如果失败就放弃，`0x814E` 保持"有数据"、INT 停在电平高，而 EXTI 是上升沿触发，之后不会再有跳变，结果就是触摸一直无响应直到断电重启。
    > 读坐标点时中途失败只保留已成功的那几个点；**一个点都没读出来时保持上次的坐标和点数**，不会伪装成"抬起"。
17. **两个自检状态变量**，都在 `LCD_Init()` 里赋值，上电后可直接读：
    - `LCD_TP_State`：`0` = GT911 的 ID 读回 `911`，触摸链路通；非 0 = I2C 无应答或 ID 不对（拔掉触摸排线时恒为 1，可用来区分是屏坏还是芯片没起来）。
    - `LCD_Panel_State`：`0` = ST7796 ID 读回 `0x7796`；非 0 = 读 ID 失败，此时 `LCD_Param.ID` 一般是 `0x0000` / `0xFFFF` / `0x7797`，说明 FSMC 读时序不够，回 `Core/Src/fsmc.c` 核对读时序。驱动**不会**因为 ID 不对就停机，显示照常进行，方便先看到画面再排查。

### 构建与配置

18. **FSMC 是扩展模式，读、写各一套时序，必须分开核对**：
    - 写时序（进 `BWTR`，`.ioc` 键 `FSMC.ExtendedAddressSetupTime1` / `FSMC.ExtendedDataSetupTime1`）：
      `ADDSET = 1`、`DATAST = 3` → 6 HCLK = **83.3 ns**，满足 ST7796 `TWC ≥ 66 ns`，留 26% 余量。**刷屏快慢只由这一组决定**。
    - 读时序（进 `BTR`，`.ioc` 键 `FSMC.AddressSetupTime1` / `FSMC.DataSetupTime1`）：
      `ADDSET = 5`、`DATAST = 8` → 15 HCLK = **208 ns**，满足读周期 `TRC ≥ 160 ns` 与读 ID 的 `TCS ≥ 45 ns`。读时序偏慢不影响显示，但调快就会读 ID / 读状态出错（`LCD_Read_Data` 里以前那句补偿延时的 `lcd_opt_delay` 已删除，就是因为总线自身已经合规）。
    - 生成后对应 `HAL_SRAM_Init(&hsram1, &Timing, &ExtTiming)` 里的两个结构体：**`Timing` → BTR（读），`ExtTiming` → BWTR（写）**。变量名是 `Timing` / `ExtTiming`，不是 `ReadTiming` / `WriteTiming`，别按名字猜；两组填反就会出现"写变慢、读仍不合规"。
    - **最容易漏的一项：写组的 `Data setup time`**。CubeMX 只在 `.ioc` 里写你动过的键，如果没点过第二组的 Data setup time，生成出来就是默认值 **255**：写周期变成 (1+1)+(255+1) = 258 HCLK = **3.58 µs/像素**，整屏 `LCD_Clear` 要 **≈550 ms**（正常应为 12.8 ms）。看到"刷屏像在慢慢擦"就是这个原因，不是屏坏。
    - `fsmc.c` 里 `AddressHoldTime = 15`、`BusTurnAroundDuration = 15`、`CLKDivision = 16`、`DataLatency = 17` 四项：HAL **会把它们写进 `BTR` / `BWTR` 的对应位**（见 `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_fsmc.c` 的 `FSMC_NORSRAM_Init` 与 `FSMC_NORSRAM_Timing_Init`），但在"异步模式 A + 非复用总线"下 FSMC **不取用**这些位 —— `ADDHLD` 只在复用总线/D 模式生效，`BUSTURN` 只在复用模式生效，`CLKDIV`/`DATLAT` 只在同步模式生效。所以现在调它们不影响任何时序；但若哪天改成复用总线或同步突发，这四个残留值会**立刻变成真实时序**，那时必须重新核算。
19. **换片选或换地址线**要同时改 `LCD_FSMC_NEX` / `LCD_FSMC_AX` 和 CubeMX 里的 CS/RS 引脚。
20. **`LCD.c` / `LCD.h` 是 UTF-8 + LF**，CubeMX 生成的文件是 UTF-8 + CRLF，混用不影响编译，但别按 GBK 处理。
21. **Debug 已是 `-O1 -g3`**（`cmake/gcc-arm-none-eabi.cmake`）。早先的 `-O0` 下填充内层循环比 `-Os` 多出约 5 倍指令，刷屏帧率明显偏低，所以默认档已带优化；Release 是 `-Os -g0`，只在需要极致速度、不调试时用。

### 刷新痕迹

22. **"先擦黑再出字"来自 `LCD_printf` 的整行擦除**，不是屏幕故障。现在已改成只擦变短的尾巴，等宽刷新（如固定位数的计数器）稳态下没有任何擦除动作。
23. **"从下往上刷出来"是面板扫描与 CPU 写入不同步造成的撕裂**。无帧缓冲、直写 GRAM 就必然如此：扫描线以下立即显示新内容，以上要等下一帧。`LCD_DrawFont` 已改为一个字符只设一次窗口（原来每行设一次），8×16 单字写入约 11 µs、16×32 约 43 µs（按 83.3 ns/像素算），撕裂窗口被压到一行以内。
24. **要彻底消除撕裂只有两条路**：上帧缓冲（300 KB 放不进 64 KB SRAM，不可行），或读 `0x45`(GSCAN) 拿当前扫描行、把一次绘制塞进消隐区。后一条现在**具备前提了**——读时序已配成 208 ns（满足 `TRC ≥ 160 ns`），读通路在时序上合规；但 `0x45` 每次要独立的总线周期，逐行判断会吃掉刷新速度，只在写整屏大块时才划算。TE 信号（`0x35`）在这块屏上没引出到排针，用不了。日常界面用不上，别为此绕弯。
25. **静态内容只在初始化时画一次**，不要放进循环里反复刷新——这是成本最低、效果最好的一条。

### 背光

26. **背光默认 100%，PWM 由 `LCD_Init()` 内部启动**。PB0 在 CubeMX 里是 `TIM3_CH3` 复用推挽，**定时器没启动时该脚输出无效电平（背光全灭）**，所以 `LCD_Init()` 第一行就是 `HAL_TIM_PWM_Start` + `LCD_BackLight(100)`；否则整个初始化期间（触摸复位 + 配置约 220 ms）屏幕是黑的。外部只需要 `LCD_BackLight(0~100)` 这一个接口。
27. **`ARR` 必须凑成 99（100 格），`CCR` 才等于百分比**。`CCR = 100` 时 `CCR > ARR`，PWM1 模式下输出恒为有效电平，这才是真 100%；写 `CCR = 99` 会变成 99%。同理 `CCR = 0` 是全灭。`LCD_BackLight()` 内部就是 `CCR = Duty`，所以 **改 `ARR` 会让亮度整体差一个比例**（`ARR = 999` 时同样的 `Duty` 只有 1/10 亮度），改定时器参数时函数注释也要一起改。
28. **`CCR` 预装载已打开**（`__HAL_TIM_ENABLE_OCxPRELOAD`）。新占空比在下一个周期边界生效，避免在周期中途改值产生一个被拉长的长脉冲——低亮度慢速呼吸时，那一下会被看成"闪一下"。
29. **20 kHz 是给背光升压"消啸叫"的默认值**：人耳上限约 20 kHz、最敏感的是 2~5 kHz，1 kHz 的方波连谐波一起落在敏感区，MLCC 的压电效应和电感磁致伸缩就会出声。`PSC=35 / ARR=99` 正好 20 kHz，且周期是 100 格、`CCR` 直接是百分比，所以这个组合同时解决了"听不见"和"代码不用换算"。
    - 不要用 `PSC=71 / ARR=49` 求 20 kHz：那样一个周期只有 50 格，`Duty` 只能按 2% 跳，低亮度段会一格一格地蹦。
    - 也不必凑 1000 格（如 `PSC=2 / ARR=999` → 24 kHz）：接口只接受 `0~100` 的整数，多出来的 10 倍分辨率用不上，反而要写回换算。
    - 极少数屏 20 kHz 仍有声：`PSC=23` → 30 kHz、`PSC=17` → 40 kHz（都是 `ARR=99`）。再高就没意义了，每周期能量太小，升压来不及建立，低占空比会明显非线性。
    - 代价：1% 对应 0.5 µs 脉冲，个别升压芯片在 `Duty=1~2` 时启动不了，表现为"1%、2%、3% 一样暗"。真遇到就接受一个下限，或在 `LCD_BackLight()` 里把 `Duty` 抬到 3。
30. **占空比与观感不是线性的**。人眼对亮度近似对数响应，线性扫占空比会显得"前大半段几乎没变化、最后突然亮"。要做呼吸/渐亮，按余弦曲线给值（`duty = 50*(1-cos(2πi/N))`），别用线性斜坡。

## 常见现象对照

| 现象 | 优先检查 |
|---|---|
| 画面整体左右镜像 | `MADCTL(0x36)` 与方向不匹配：竖屏 `0x08`、横屏 `0x68` 或 `0xA8`、倒置 `0xC8`，见「显示方向」 |
| 横屏下右边/下边留黑边，内容只占左上角 | 只改了 MADCTL、没交换 `Width/Height`，一律用 `LCD_SetRotation()` 切换 |
| 画面方向对、但手指点的地方偏 90° 或 180° | 触摸变换与 MADCTL 不配套：把 `LCD_T_ScanIT()` 里 `LCD_ROT_90` / `LCD_ROT_270` 两条公式对调 |
| `LCD_Param.ID` 读到 `0000` / `FFFF` / `7797` | FSMC 读时序不足：`BTR` 应为 `ADDSET=5 / DATAST=8`；两组时序是否填反 |
| 触摸完全无响应 | PF10 是否 `IT_RISING + NOPULL`；`LCD_T_Reset` 是否被改回上拉；`LCD_TP_State` 是否非 0 |
| 触摸正常但读不到坐标 | 是否忘了主循环调 `LCD_T_ScanIT()`；`0x814E` 的 bit7 是否为 1 |
| Debug 能摸、Release 摸不动（或反之） | 软件 I2C 的 DWT 延时是否被删掉；`LCD_I2C_DelayInit()` 是否在 `LCD_TP_Init()` 之前调用 |
| 汉字显示成方框 | 字库是否包含该字；写这个字符串的源文件是否 UTF-8 |
| 数值刷新有残影 | 用 `LCD_printf`（只擦变短的尾巴），不要用 `LCD_ShowNum` 反复刷新同一处；被 `LCD_Clear` 改写过就先自己 `LCD_Fill` 一块底 |
| 刷新时先黑一下再出字 | 是否自己先 `LCD_Fill` 了一整行再写字；`LCD_printf` 已经不做整行擦除 |
| 触摸按住时坐标停住不动 | 主循环是否还在调 `LCD_T_ScanIT()`；I2C 失败后的自动重试依赖主循环轮转（见注意事项 16） |
| 背光完全不亮（屏幕全黑但 ID/触摸自检正常） | `MX_TIM3_Init()` 是否在 `LCD_Init()` 之前；PB0 是否还配成 GPIO 输出而不是 `S_TIM3_CH3`；`Duty` 是否为 0 |
| 调亮度时背光闪一下 | 改占空比未开 CCR 预装载（见注意事项 28） |
| 背光有啸叫 / 摄像头拍到横纹 | 确认 `PSC=35 / ARR=99`（20 kHz）；仍响见注意事项 29 |
| 文字区域有横线割裂、上下半新旧不一 | 无同步撕裂，见注意事项 22~24 |
| 刷屏特别慢（整屏要半秒左右，像在"慢慢擦"） | 写组 `DATAST` 还是默认 `255`：`ExtTiming.DataSetupTime` 应为 `3`；`.ioc` 里缺 `FSMC.ExtendedDataSetupTime1` 就是这个原因 |
| 刷屏略慢但不夸张 | 写时序被改成 4 以上，或 Debug 被改回 `-O0` |
| 调 `printf` 就死机 | `__io_putchar` 未实现，改用 `LCD_printf` |

## 重新生成 CubeMX 时重点核对

1. PF10：`GPIO_EXTI10` + 上升沿 + `No pull`，User Label 必须仍是 `LCD_T_INT`（否则 `LCD.c` 里的宏名对不上）。
2. NVIC：`EXTI line[15:10]` 已使能，优先级低于 TIM2。
3. FSMC：`SRAM` 类型 + NE4 + A10 + 16 位 + 模式 A + **扩展模式(Extended Mode)开启**；
   读组 `ADDSET=5 / DATAST=8`，写组 `ADDSET=1 / DATAST=3`。
   生成后逐行看 `fsmc.c`：`Timing.AddressSetupTime = 5` / `Timing.DataSetupTime = 8` /
   `ExtTiming.AddressSetupTime = 1` / `ExtTiming.DataSetupTime = 3`。四个数任何一个不对都要回 CubeMX，
   **尤其是 `ExtTiming.DataSetupTime`，漏改就是 255，整屏半秒**。
4. 背光 PB0：必须是 **`S_TIM3_CH3` → `PWM Generation3 CH3`**（不是普通 GPIO 输出）。
   TIM3：`Prescaler = 36-1`（35）、`Counter Period = 100-1`（99）→ 72 MHz / 36 / 100 = **20 kHz**；
   `PWM Mode 1`、`Polarity High`、`Pulse = 100`（=100%）。周期是 100 格，所以 `CCR` 就是百分比。
   生成后看 `tim.c`：`MX_TIM3_Init` 里有 `HAL_TIM_PWM_ConfigChannel(..., TIM_CHANNEL_3)`，
   `HAL_TIM_MspPostInit` 里 `LCD_BL_Pin` 是 `GPIO_MODE_AF_PP`。
   **注意 PB1 是触摸的 SCL，别把 TIM3_CH4 也打开**——那会和软件 I2C 抢引脚。
5. Project Manager → Minimum Stack Size 已设为 `0x1000`（4 KB），Heap 保持 `0x200`。**`.ioc` 与链接脚本两处必须同值**：CubeMX 重新生成会按 `.ioc` 重写 `STM32F103XX_FLASH.ld`，只改 `.ld` 的话下次生成就回到默认 `0x400`。
   > 这两项在 GCC 链接脚本里只体现为 `._user_heap_stack` 段加一句"RAM 够不够"的断言，**不是栈的上限**。栈顶固定是 `_estack = 0x20010000`（64 KB RAM 顶端），向下生长，可用空间 = 64 KB − 静态占用（当前 `data 472 + bss 5408`）≈ 58 KB。`_Min_Stack_Size` 只是保证"至少留出这么多空隙"，超了不会自动报警，要抓栈溢出得自己加水位检测（启动时把一块 RAM 涂成固定图案，周期性检查还剩多少没被覆盖）。
   > `_sstack`（`0x2000F000`）虽然被定义，但启动文件和库都没引用它，不要误以为栈被限制在那 4 KB 之内。
