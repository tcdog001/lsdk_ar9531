#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/string.h>
#include "upgrade.h"
#include "../common/autelan-app/src/partool/partoolapi.h"

/*zhangsiyu add watchdog function in u-boot 2014-8-22*/	
#include "ar7240_soc.h"
#include <asm/addrspace.h>
/*zhangsiyu add end*/


unsigned char partool_cmd_tmp[1024] ;

int
part_osenv_read(char *name, void *buf)
{
	char *partool_cmd[] = {"-partool","-part", "osenv", "-show", "name"};
	int rc;
	
	partool_cmd[4] = name;
	
	rc = partool_show_byname_api(5,partool_cmd,buf);

	partool_clean();

	return rc ;
}

int
part_osenv_write(char *name, char *value)
{
	char *partool_cmd[]= {"-partool","-part", "osenv", "-new"," name", "value"};
	int rc;

	partool_cmd[4] = name;
	partool_cmd[5] = value;
	
	rc = partool_new(6,partool_cmd);

    partool_clean();
	
	return rc;
}

int
upgrade_from_media()
{
    char *s;
    char cmd_buf[256];
    if ((s = getenv ("recovery_upgrade_disable")) == NULL) {
  
        if ((s = getenv ("disable_watchdog")) == NULL){
            //puts ("disable watchdog\n");
            ar7240_reg_wr(AR7240_WATCHDOG_TMR_CONTROL, 0);
        }   
         
        strcpy(cmd_buf, "tftpboot 0x80060000 openwrt-ar71xx-generic-ap147-16M-squashfs-sysupgrade.bin;erase 0x9f060000 +0xf10000;cp.b 0x80060000 0x9f060000 0xf10000");
        run_command(cmd_buf, 0);
        setenv ("recovery_upgrade", "no");

        if ((s = getenv ("disable_watchdog")) == NULL){
            puts ("feed the watchdog with 0xffff ffff\n");
            ar7240_reg_wr(AR7240_WATCHDOG_TMR_CONTROL, 3);
            ar7240_reg_wr(AR7240_WATCHDOG_TMR, 0xffffffff);
        }   

    }
  
    return 0;
}

int start_newsystem()
{
	int rc;
    char cmd_buf[256];

	rc = part_osenv_read("system.state", partool_cmd_tmp);
	if (rc == -22) {
        strcpy(cmd_buf, "partool -part osenv -empty");
        run_command(cmd_buf, 0);       
        return 1;
    } 
	if (strcmp(partool_cmd_tmp, "fail") == 0) {
		rc = part_osenv_read("system.failed_count", partool_cmd_tmp);
	    if (rc == -22) {
            strcpy(cmd_buf, "partool -part osenv -empty");
            run_command(cmd_buf, 0);       
            return 1;
        } 		
		if (strcmp(partool_cmd_tmp, "3") == 0) {
            //setenv ("recovery_upgrade", "yes");
		
            upgrade_from_media();
            /*  

			rc = part_osenv_write("system.failed_count", "0");
            if (rc != 0) {
                return 1;
            } 
*/
		} else if (strcmp(partool_cmd_tmp, "0") == 0 || strcmp(partool_cmd_tmp, "1") == 0 || strcmp(partool_cmd_tmp, "2") == 0){
			           
            partool_cmd_tmp[0] += 1;
            //setenv ("recovery_upgrade", "no");
            rc = part_osenv_write("system.failed_count", partool_cmd_tmp);
            if (rc != 0) {
                return 1;
            }             
			
		} else {
            //setenv ("recovery_upgrade", "no");
			printf("Invalid system.failed_count,now write 0 to system.failed_count\n");
			rc = part_osenv_write("system.failed_count", "0");
            if (rc != 0) {
                return 1;
            } 
		}
		
	} else if (strcmp(partool_cmd_tmp, "ok") == 0) {
        //setenv ("recovery_upgrade", "no");
		printf("system.state is ok\n");
	} else {
        //setenv ("recovery_upgrade", "no");
		printf("Invalid system.state,now write fail to system.state\n");	
	}
	
	rc = part_osenv_write("system.state", "fail");
	if (rc != 0) {
		return 1;
	} 
	
	return 0;
}
#if 0
int start_oldsystem()
{
    int rc;

    part_osenv_read("system.ha", partool_cmd_tmp);

    if(strcmp(partool_cmd_tmp, "single") != 0) {
		printf("invalid system.ha,now write single to system.ha\n");
		rc = part_osenv_write("system.ha", "single");
		
		if (rc != 0) {
			return 1;
			} 
		}
/*
check crc or check system.state
*/
    puts ("done\n");
	setenv ("bootcmd", "bootm 0x9f260000");
	setenv ("bootargs", "console=ttyS0,115200 root=31:06 rootfstype=jffs2 init=/sbin/init mtdparts=ath-nor0:256k(u-boot),64k(u-boot-env),64k(os-env),1024k(kernel-minios),1024k(rootfs-minios),1088k(kernel),12288k(rootfs),448k(config),64k(productinfo),64k(ART)");
    
    return 0;
}
#endif
int
check_cmd()
{
    char *s;
    char cmd_buf[256];

    if ((s = getenv ("switch_change")) == NULL) 
        return 0;
    else {
        setenv ("bootcmd", "0x9ff70000");
        strcpy(cmd_buf, "mw 0x18070000 0x80");
        run_command(cmd_buf, 0); 
      
        return 0;        
    }

}
void crc_flash(void)
{
    setenv ("bootcmd", "bootm 0x9f060000");

	start_newsystem();
    
    check_cmd();
	return 0;
}


