#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#include <stdint.h>

/* ============================================================
 * 字库编号，传给 LCD_ShowChar / LCD_ShowString 的 Size 参数
 * ============================================================ */
#define LCD_FONT_8X16       0       /* 小号：ASCII 8x16  */
#define LCD_FONT_16X32      1       /* 大号：ASCII 16x32 */
#define LCD_FONT_NUM        2       /* 字库总数 */

/* ============================================================
 * 汉字字模单元
 *   Index : 汉字本身，UTF-8 编码（最多 3 字节 + 结束符）
 *   Data  : 16x16 点阵，32 字节
 *           前 16 字节 = 上半 8 行，后 16 字节 = 下半 8 行
 *           每字节对应一列，bit0 在该列最上方
 * ============================================================ */
typedef struct
{
    char    Index[4];
    uint8_t Data[32];
} ChineseCell_t;

/* ============================================================
 * 字体描述表项
 *   Bytes = W * H / 8，即单字字模字节数
 * ============================================================ */
typedef struct
{
    uint16_t       W;
    uint16_t       H;
    uint8_t        Bytes;
    const uint8_t *Data;
} LCDFont_t;

/* ASCII 字模数据 */
extern const uint8_t LCD_ASCII_8x16[95][16];
extern const uint8_t LCD_ASCII_16x32[95][64];

/* 汉字字模数据（以空 Index 结尾，便于遍历） */
extern const ChineseCell_t LCD_Chinese_16x16[];

/* 字体描述表，下标即 LCD_FONT_xxx */
extern const LCDFont_t LCD_FontTable[LCD_FONT_NUM];

#endif

