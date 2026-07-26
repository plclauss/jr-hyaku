/**
 * @file oled_cmds.h
 * @author Paul Clauss (plclauss@gmail.com)
 * @brief A set of commands used by the application to configure/interact w/ the
 * OLED. These commands are effectively "standardized" for SSD1351 modules; see
 * the documentation: https://newhavendisplay.com/content/app_notes/SSD1351.pdf.
 * @date 2025-11-23
 */

#ifndef __OLED_CMDS_H__
#define __OLED_CMDS_H__

#define SSD1351_CMD_DISP_OFF (0xAE)
#define SSD1351_CMD_CMDLOCK (0xFD)
#define SSD1351_CMD_SET_DISP_OFFSET (0xA2)
#define SSD1351_CMD_SET_DISP_GPIOS (0xB5)
#define SSD1351_CMD_SET_REMAP (0xA0)
#define SSD1351_CMD_DISP_ON (0xAF)
#define OLED_CMD_SET_COL_ADDR (0x15)
#define OLED_CMD_SET_ROW_ADDR (0x75)
#define SSD1351_CMD_WRITE_RAM (0x5C)

#endif  // __OLED_CMDS_H__
