/*
 *
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

#ifndef __ASM_MACH_AR7240_KMALLOC_H
#define __ASM_MACH_AR7240_KMALLOC_H


#ifndef CONFIG_DMA_COHERENT
/*
 * Total overkill for most systems but need as a safe default.
 */
#ifdef CONFIG_KMALLOC_MINALIGN_64BYTE
#define ARCH_KMALLOC_MINALIGN	64
#endif
#endif

#endif /* __ASM_MACH_AR7240_KMALLOC_H */
