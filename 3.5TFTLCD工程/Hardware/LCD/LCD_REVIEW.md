# LCD 待修改清单

> 规则：**每次 review 重写本文件，只保留“当前需要改的项”。**
> 范围：**仅 LCD 驱动层（`Hardware/LCD/`）。** 不涉及 LVGL 接入、`printf` 重定向、TIM2。
> 基准：2026-11-02 22:57 的实际工程。`LCD.c` 43180 B（11-02 21:34），`README.md` 19578 B（11-02 22:34），`cmake` 未变。
> 状态：仅列清单，**未改动任何文件**。

## 一览

| # | 项 | 类型 | 严重度 | 位置 |
|---|---|---|---|---|
| L-1 | `LCD_WritePixels` 右边缘裁剪会错行 | **功能缺陷** | 高 | `LCD.c:788-804` |
| L-2 | `LCD_T_ScanIT` 读失败时返回值与 `TP_Data.Count` 不一致 | 功能缺陷 | 低 | `LCD.c:1321` |
| L-3 | Debug 实际是 `-O0` | 配置 | 中 | `cmake/gcc-arm-none-eabi.cmake:35,37` |

---

## L-1　`LCD_WritePixels` 横向裁剪会错行（功能缺陷）

**位置**：`LCD.c:788-804`（当前实际代码）

```c
if (Data == 0 || Width == 0 || Height == 0) return;
if (X >= LCD_Param.Width || Y >= LCD_Param.Height) return;

if (X + Width  > LCD_Param.Width)  Width  = LCD_Param.Width  - X;
if (Y + Height > LCD_Param.Height) Height = LCD_Param.Height - Y;
if (Width == 0 || Height == 0) return;

total = (uint32_t)Width * Height;
LCD_Set_Window(X, Y, Width, Height);
LCD_Write_Reg(LCD_Param.WriteGRAM);
while (total--) LCD->LCD_RAM = *Data++;     /* 平铺连续写，没有按行定位 */
```

**问题**：裁剪宽度时只改了 `Width`，但源缓冲区仍按**原始宽度**行优先排列。连续灌数据会把“上一行被裁掉的部分”灌到下一行的开头，画面出现斜切/错行。

**举例**（屏宽 320，源图 40×2）：

```c
LCD_WritePixels(300, 0, 40, 2, data);   /* 期望显示源图左侧 20 列 */
```

裁剪后 `Width=20`、`total=40`，一次设窗 `(300,0,20,2)`，然后平铺写 `data[0..39]`：

| 窗口行 | 实际取到的源数据 | 应该是 |
|---|---|---|
| 第 0 行（y=0） | `data[0..19]` = 源第 0 行 0~19 列 | 正确 |
| 第 1 行（y=1） | `data[20..39]` = 源第 **0** 行 20~39 列（本该在屏外） | 应为源第 **1** 行 0~19 列 |

**纵向裁剪不受影响**：只裁 `Height` 时，取前 `W*H'` 个像素正好是前 `H'` 行，平铺写入是对的。所以这个缺陷只在**右边贴边且高度 > 1** 时暴露，左边、下边、整幅在屏内都正常——很容易漏测。

**参考**：同文件的 `LCD_DrawBitmap`（`LCD.c:850-864`）是**逐行算指针**再写的（`p = Data + (row - y0) * Width + (sx - x0)`），所以它贴边安全；`LCD_WritePixels` 缺的正是这一步。

**修法（二选一）**

方案 A —— 保留裁剪，改成逐行定位（与 `LCD_DrawBitmap` 行为一致，通用）：

```c
uint16_t srcW = Width;                       /* 保存原始宽度当步长 */
uint16_t row, col;
if (X + Width  > LCD_Param.Width)  Width  = LCD_Param.Width  - X;
if (Y + Height > LCD_Param.Height) Height = LCD_Param.Height - Y;
if (Width == 0 || Height == 0) return;

for (row = 0; row < Height; row++)
{
    const uint16_t *p = Data + (uint32_t)row * srcW;
    LCD_Set_Window(X, (uint16_t)(Y + row), Width, 1);
    LCD_Write_Reg(LCD_Param.WriteGRAM);
    for (col = 0; col < Width; col++) LCD->LCD_RAM = *p++;
}
```

代价：每行一次设窗，高度越大越多总线开销（一次设窗约 10 次寄存器写，相对每行 `Width` 个像素通常可忽略；`Width` 很小时才明显）。

方案 B —— 直接不裁剪，要求调用方保证矩形在屏内，越界就 `return`：

```c
if (X + Width > LCD_Param.Width || Y + Height > LCD_Param.Height) return;
```

一行设窗 + 平铺写入，最快，但把“必须自己保证不越界”的责任交给调用方——文档要同步改口径（`README.md:103` 目前称“贴边也安全”，与事实相反）。

**验证**：`LCD_WritePixels(300, 0, 40, 2, buf)`，`buf` 第 0 行填红、第 1 行填蓝；屏上 `y=0` 应为红、`y=1` 应为蓝。

**已用主机端模拟复现**（按当前代码的写入顺序逐像素还原 GRAM）：裁剪后窗口 `20×2`，`(300,0) = 1(红)` 正确，`(300,1) = 1(红)` **错误，应为 2(蓝)**；换成逐行定位的实现后 `(300,1) = 2`。所以这不是纸面推演。

---

## L-2　`LCD_T_ScanIT` 读失败时返回值与 `TP_Data.Count` 不一致

**位置**：`LCD.c:1321`

```c
if (!TP_IntFlag) return TP_Data.Count;        /* 无中断：直接返回上次状态 */
TP_IntFlag = 0;

if (LCD_T_Read(GT9XXX_GSTID_REG, &status, 1)) return 0;   /* 只返回 0，没清 Count */
```

**问题**：I2C 读失败（应答缺失/总线干扰）时函数返回 `0`，但 `TP_Data.Count` 保持上一次的值（可能是 1~5）。而 `README.md:167` 明确写“`Count` 为 0 表示已抬起”，`LCD.h:110` 也把 `Count` 定义为当前点数。于是：

- 用返回值判断的代码 → 认为已抬起；
- 直接读 `TP_Data.Count` 的代码 → 认为还在按着。

两种写法得出相反结论，属于可观测的状态不一致。

**修法**：失败路径统一处理

```c
if (LCD_T_Read(GT9XXX_GSTID_REG, &status, 1))
{
    TP_Data.Count = 0;      /* 读不到就当作抬起，避免 Count 停在旧值 */
    return 0;
}
```

（若希望“读失败保持上一次状态”，则应 `return TP_Data.Count;` 且文档写明；两种都可以，但必须与 `Count` 的语义一致。）

**验证**：拔掉触摸排线后循环调 `LCD_T_ScanIT()`，返回值与 `TP_Data.Count` 都应为 0。

---

## L-3　Debug 档实际是 `-O0`

**实际**：`cmake/gcc-arm-none-eabi.cmake:35` `set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")`；`:37` 同理。

**影响**：`LCD_Fill` / `LCD_WritePixels` / `LCD_DrawFont` 的内层循环承担几乎全部像素搬运，`-O0` 下指令数约为 `-Os` 的 5 倍。调试档下看到的“刷屏慢”会被误判成写时序问题（`README.md:206` 那条），而按那个方向去查 `ExtTiming.DataSetupTime` 是查不出来的。

**处理**：把两行改成 `-O1 -g3`

```cmake
set(CMAKE_C_FLAGS_DEBUG "-O1 -g3")
set(CMAKE_CXX_FLAGS_DEBUG "-O1 -g3")
```

`-O1` 保留完整调试信息，同时把内层循环的指令数降下来；Release 的 `-Os -g0` 不动。

**验证**：重新配置后 `grep "FLAGS = .*-O" build/<dir>/build.ninja` 应出现 `-O1 -g3`；`arm-none-eabi-size` 的 `text` 会明显下降（本工程此前 `-O0` → `-O1` 是 12076 → 8500 量级）。

---

## 本轮未做

- 未改任何代码、未改任何配置、未改 `README.md`。
- 未上板实测。L-1 已用主机端写入顺序模拟复现（见该条），L-2 为代码路径分析，两者都给了可复现的验证用例。
