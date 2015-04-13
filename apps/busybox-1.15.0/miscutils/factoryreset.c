/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
 *
 */ 

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include "busybox.h"

static volatile int fd = 0;

static void siginthnd(int signo)
{
    printf("signal caught. closing fr device\n");
    if (fd != 0)
        close(fd);
    exit(0);
}

extern int factoryreset_main(int argc, char **argv)
{
	if (daemon(0, 1) < 0)
		bb_perror_msg_and_die("Failed forking factory reset daemon");

	signal(SIGHUP, siginthnd);
	signal(SIGINT, siginthnd);

	fd = open(argv[argc - 1], O_WRONLY);

        if (fd < 0)
		bb_perror_msg_and_die("Failed to open factory reset device");

        ioctl(fd, 0x89ABCDEF, 0);

	close(fd);

        printf("\nRestoring the factory default configuration ....\n");
        fflush(stdout);

        /* Restore the factory default settings */
        system("factoryrestore");
        sleep(1);

        reboot(RB_AUTOBOOT);
	return EXIT_SUCCESS;
}
