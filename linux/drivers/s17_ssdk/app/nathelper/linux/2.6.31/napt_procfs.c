/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 *  napt_procfs.c -  create files in /proc
 *
 */
#include <linux/autoconf.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>    /* for copy_from_user */

#define ATHRS17_MAC_LEN         13      // 12+1
#define ATHRS17_IP_LEN     9       // 8+1
#define ATHRS17_CHAR_MAX_LEN ATHRS17_MAC_LEN

#define NF_PROCFS_DIR                        "qca_switch"

#define NF_ATHRS17_HNAT_NAME                 "nf_athrs17_hnat"
#define NF_ATHRS17_HNAT_WAN_TYPE_NAME        "nf_athrs17_hnat_wan_type"
#define NF_ATHRS17_HNAT_PPP_ID_NAME          "nf_athrs17_hnat_ppp_id"
#define NF_ATHRS17_HNAT_UDP_THRESH_NAME      "nf_athrs17_hnat_udp_thresh"
#define NF_ATHRS17_HNAT_WAN_IP_NAME          "nf_athrs17_hnat_wan_ip"
#define NF_ATHRS17_HNAT_PPP_PEER_IP_NAME     "nf_athrs17_hnat_ppp_peer_ip"
#define NF_ATHRS17_HNAT_PPP_PEER_MAC_NAME    "nf_athrs17_hnat_ppp_peer_mac"
#define NF_ATHRS17_HNAT_WAN_MAC_NAME         "nf_athrs17_hnat_wan_mac"

/* for PPPoE */
int nf_athrs17_hnat = 0;
int nf_athrs17_hnat_wan_type = 0;
int nf_athrs17_hnat_ppp_id = 0;
int nf_athrs17_hnat_udp_thresh = 0;
unsigned char nf_athrs17_hnat_wan_ip[ATHRS17_IP_LEN] = {0};
unsigned char nf_athrs17_hnat_ppp_peer_ip[ATHRS17_IP_LEN] = {0};
unsigned char nf_athrs17_hnat_ppp_peer_mac[ATHRS17_MAC_LEN] = {0};
unsigned char nf_athrs17_hnat_wan_mac[ATHRS17_MAC_LEN] = {0};

/**
 * This structure hold information about the /proc file
 *
 */
static struct proc_dir_entry *qca_switch_dir;

static struct proc_dir_entry *nf_athrs17_hnat_file;
static struct proc_dir_entry *nf_athrs17_hnat_wan_type_file;
static struct proc_dir_entry *nf_athrs17_hnat_ppp_id_file;
static struct proc_dir_entry *nf_athrs17_hnat_udp_thresh_file;
static struct proc_dir_entry *nf_athrs17_hnat_wan_ip_file;
static struct proc_dir_entry *nf_athrs17_hnat_ppp_peer_ip_file;
static struct proc_dir_entry *nf_athrs17_hnat_ppp_peer_mac_file;
static struct proc_dir_entry *nf_athrs17_hnat_wan_mac_file;

void simple_strtohex(uint8_t *hex, uint8_t *str, uint8_t len)
{
    int cnt, val = 0;

    for (cnt = 0; cnt < len; cnt ++)
    {
        val = (( (*str >= '0') && (*str <= '9') ) ? ((*str - '0') << 4) : ((*str - 'a' + 10) << 4) &0xf0);
        val += (( (*(str+1) >= '0') && (*(str+1) <= '9') ) ? ((*(str+1) - '0')) : ((*(str+1) - 'a' + 10) )&0xf);
        *hex = val;
        hex++;
        str += 2;
    }
}

/**
 * This function is called then the /proc file is read
 *
 */
static int procfile_read_int(char *page, char **start, off_t off, int count,  int *eof, void *data)
{
    int ret;
    int *prv_data = (int *)data;

    ret = sprintf(page, "0x%x\n", *prv_data);

    return ret;
}

/**
 * This function is called with the /proc file is written
 *
 */
static int procfile_write_int(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int len;
    uint8_t tmp_buf1[9] = {'0', '0', '0', '0', '0', '0', '0', '0', '0'};
    uint8_t tmp_buf2[9] = {'0', '0', '0', '0', '0', '0', '0', '0', '0'};
    int i;
    unsigned int tmp_data;
    unsigned int *prv_data = (unsigned int *)data;
    
    if(count > sizeof(tmp_buf1))
        len = sizeof(tmp_buf1);
    else
        len = count;

    if(copy_from_user(tmp_buf1, buffer, len))
        return -EFAULT;

    *prv_data = 0;
    tmp_buf1[len-1] = '0';

    for (i=0;i<8;i++)
    {
        tmp_buf2[i] = tmp_buf1[7-i];
    }
    simple_strtohex((uint8_t *)prv_data, tmp_buf2, 8);
    tmp_data = *prv_data;
    *prv_data = (((tmp_data & 0xf)>>0)<<28);
    *prv_data |= (((tmp_data & 0xf0)>>4)<<24);
    *prv_data |= (((tmp_data & 0xf00)>>8)<<20);
    *prv_data |= (((tmp_data & 0xf000)>>12)<<16);
    *prv_data |= (((tmp_data & 0xf0000)>>16)<<12);
    *prv_data |= (((tmp_data & 0xf00000)>>20)<<8);
    *prv_data |= (((tmp_data & 0xf000000)>>24)<<4);
    *prv_data |= (((tmp_data & 0xf0000000)>>28));
    *prv_data = (*prv_data)>>((9-len)*4);

    return len;
}

static int procfile_read_char(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int ret;
	unsigned char *prv_data = (unsigned char *)data;

	ret = sprintf(page, "%s\n", prv_data);

	return ret;
}


static int procfile_write_char(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len;
	unsigned char *prv_data = (unsigned char *)data;

	if(count > ATHRS17_CHAR_MAX_LEN)
		len = ATHRS17_CHAR_MAX_LEN;
	else
		len = count;

	if(copy_from_user(prv_data, buffer, len))
		return -EFAULT;

	prv_data[len-1] = '\0';

	return len;
}

static void setup_proc_entry(void)
{
    nf_athrs17_hnat = 1;
    nf_athrs17_hnat_wan_type = 0;
    nf_athrs17_hnat_ppp_id = 0;
    memset(&nf_athrs17_hnat_ppp_peer_mac, 0, 12);
    memset(&nf_athrs17_hnat_wan_mac, 0, 12);
    memset(&nf_athrs17_hnat_ppp_peer_ip, 0, 8);
    memset(&nf_athrs17_hnat_wan_ip, 0, 8);
}

int napt_procfs_init(void)
{
    int ret = 0;

    setup_proc_entry();

    /* create directory */
    qca_switch_dir = proc_mkdir(NF_PROCFS_DIR, NULL);
    if(qca_switch_dir == NULL)
    {
        ret = -ENOMEM;
        goto err_out;
    }

    /* create the /proc file */
    nf_athrs17_hnat_file = create_proc_entry(NF_ATHRS17_HNAT_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_NAME);
        goto no_athrs17_hnat;
    }
    nf_athrs17_hnat_file->data = &nf_athrs17_hnat;
    nf_athrs17_hnat_file->read_proc  = procfile_read_int;
    nf_athrs17_hnat_file->write_proc = procfile_write_int;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_NAME);

    nf_athrs17_hnat_wan_type_file = create_proc_entry(NF_ATHRS17_HNAT_WAN_TYPE_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_wan_type_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_WAN_TYPE_NAME);
        goto no_athrs17_hnat_wan_type;
    }
    nf_athrs17_hnat_wan_type_file->data = &nf_athrs17_hnat_wan_type;
    nf_athrs17_hnat_wan_type_file->read_proc  = procfile_read_int;
    nf_athrs17_hnat_wan_type_file->write_proc = procfile_write_int;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_WAN_TYPE_NAME);

    nf_athrs17_hnat_ppp_id_file = create_proc_entry(NF_ATHRS17_HNAT_PPP_ID_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_ppp_id_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_PPP_ID_NAME);
        goto no_athrs17_hnat_ppp_id;
    }
    nf_athrs17_hnat_ppp_id_file->data = &nf_athrs17_hnat_ppp_id;
    nf_athrs17_hnat_ppp_id_file->read_proc  = procfile_read_int;
    nf_athrs17_hnat_ppp_id_file->write_proc = procfile_write_int;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_PPP_ID_NAME);

    nf_athrs17_hnat_udp_thresh_file = create_proc_entry(NF_ATHRS17_HNAT_UDP_THRESH_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_udp_thresh_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_UDP_THRESH_NAME);
        goto no_athrs17_hnat_udp_thresh;
    }
    nf_athrs17_hnat_udp_thresh_file->data = &nf_athrs17_hnat_udp_thresh;
    nf_athrs17_hnat_udp_thresh_file->read_proc  = procfile_read_int;
    nf_athrs17_hnat_udp_thresh_file->write_proc = procfile_write_int;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_UDP_THRESH_NAME);

    nf_athrs17_hnat_wan_ip_file = create_proc_entry(NF_ATHRS17_HNAT_WAN_IP_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_wan_ip_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_WAN_IP_NAME);
        goto no_athrs17_hnat_wan_ip;
    }
    nf_athrs17_hnat_wan_ip_file->data = &nf_athrs17_hnat_wan_ip;
    nf_athrs17_hnat_wan_ip_file->read_proc  = procfile_read_char;
    nf_athrs17_hnat_wan_ip_file->write_proc = procfile_write_char;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_WAN_IP_NAME);

    nf_athrs17_hnat_ppp_peer_ip_file = create_proc_entry(NF_ATHRS17_HNAT_PPP_PEER_IP_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_ppp_peer_ip_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_PPP_PEER_IP_NAME);
        goto no_athrs17_hnat_ppp_peer_ip;
    }
    nf_athrs17_hnat_ppp_peer_ip_file->data = &nf_athrs17_hnat_ppp_peer_ip;
    nf_athrs17_hnat_ppp_peer_ip_file->read_proc  = procfile_read_char;
    nf_athrs17_hnat_ppp_peer_ip_file->write_proc = procfile_write_char;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_PPP_PEER_IP_NAME);

    nf_athrs17_hnat_ppp_peer_mac_file = create_proc_entry(NF_ATHRS17_HNAT_PPP_PEER_MAC_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_ppp_peer_mac_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_PPP_PEER_MAC_NAME);
        goto no_athrs17_hnat_ppp_peer_mac;
    }
    nf_athrs17_hnat_ppp_peer_mac_file->data = &nf_athrs17_hnat_ppp_peer_mac;
    nf_athrs17_hnat_ppp_peer_mac_file->read_proc  = procfile_read_char;
    nf_athrs17_hnat_ppp_peer_mac_file->write_proc = procfile_write_char;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_PPP_PEER_MAC_NAME);

    nf_athrs17_hnat_wan_mac_file = create_proc_entry(NF_ATHRS17_HNAT_WAN_MAC_NAME, 0644, qca_switch_dir);
    if (NULL == nf_athrs17_hnat_wan_mac_file)
    {
        printk("Error: Can not create /proc/%s/%s\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_WAN_MAC_NAME);
        goto no_athrs17_hnat_wan_mac;
    }
    nf_athrs17_hnat_wan_mac_file->data = &nf_athrs17_hnat_wan_mac;
    nf_athrs17_hnat_wan_mac_file->read_proc  = procfile_read_char;
    nf_athrs17_hnat_wan_mac_file->write_proc = procfile_write_char;
    printk("/proc/%s/%s is created\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_WAN_MAC_NAME);

    return 0;

no_athrs17_hnat_wan_mac:
    remove_proc_entry(NF_ATHRS17_HNAT_PPP_PEER_MAC_NAME, NF_PROCFS_DIR);
no_athrs17_hnat_ppp_peer_mac:
    remove_proc_entry(NF_ATHRS17_HNAT_PPP_PEER_IP_NAME, NF_PROCFS_DIR);
no_athrs17_hnat_ppp_peer_ip:
    remove_proc_entry(NF_ATHRS17_HNAT_WAN_IP_NAME, NF_PROCFS_DIR);
no_athrs17_hnat_wan_ip:
    remove_proc_entry(NF_ATHRS17_HNAT_UDP_THRESH_NAME, NF_PROCFS_DIR);
no_athrs17_hnat_udp_thresh:
    remove_proc_entry(NF_ATHRS17_HNAT_PPP_ID_NAME, NF_PROCFS_DIR);
no_athrs17_hnat_ppp_id:
    remove_proc_entry(NF_ATHRS17_HNAT_WAN_TYPE_NAME, NF_PROCFS_DIR);
no_athrs17_hnat_wan_type:
    remove_proc_entry(NF_ATHRS17_HNAT_NAME, NF_PROCFS_DIR);
no_athrs17_hnat:
    remove_proc_entry(NF_PROCFS_DIR, NULL);
err_out:
    return ret;
}

void napt_procfs_exit(void)
{
    remove_proc_entry(NF_ATHRS17_HNAT_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_ATHRS17_HNAT_WAN_TYPE_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_ATHRS17_HNAT_PPP_ID_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_ATHRS17_HNAT_UDP_THRESH_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_ATHRS17_HNAT_WAN_IP_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_ATHRS17_HNAT_PPP_PEER_IP_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_ATHRS17_HNAT_PPP_PEER_MAC_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_ATHRS17_HNAT_WAN_MAC_NAME, NF_PROCFS_DIR);
    remove_proc_entry(NF_PROCFS_DIR, NULL);
    printk(KERN_INFO "/proc/%s/%s removed\n", NF_PROCFS_DIR, NF_ATHRS17_HNAT_NAME);
}


