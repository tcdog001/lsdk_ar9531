/*
 *  Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ioctl numbers for i2s driver */

#define NUM_DESC	    160
#define I2S_BUF_SIZE	24

#define I2S_VOLUME      _IOW('N', 0x20, int)
#define I2S_FREQ        _IOW('N', 0x21, int)
#define I2S_DSIZE       _IOW('N', 0x22, int)
#define I2S_MODE        _IOW('N', 0x23, int)
#define I2S_FINE        _IOW('N', 0x24, int)
#define I2S_COUNT       _IOWR('N', 0x25, int)
#define I2S_PAUSE       _IOWR('N', 0x26, int)
#define I2S_RESUME      _IOWR('N', 0x27, int)
#define I2S_MCLK        _IOW('N', 0x28, int)


