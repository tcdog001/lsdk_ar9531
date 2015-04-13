#!/usr/bin/perl

=head1
	#
	# Copyright (c) 2013 Qualcomm Atheros, Inc..
	# All Rights Reserved.
	# Qualcomm Atheros Confidential and Proprietary.
	#
=cut

use strict;

my @sourcename;	
#name of downloaded open source
$sourcename[0]='buildroot-2009.08.tar.bz2';
$sourcename[1]='busybox-1.01.tar';
$sourcename[2]='iptables-1.4.5.tar.bz2';
$sourcename[3]='linux-2.6.31.tar.bz2';
$sourcename[4]='lzma457.tar.bz2';
$sourcename[5]='sysstat-6.0.1.tar.bz2';
$sourcename[6]='u-boot-2006-04-06-1725.tar.bz2';
$sourcename[7]='wireless_tools.29.tar.gz';
$sourcename[8]='busybox-1.15.0.tar.bz2';
$sourcename[9]='genext2fs-1.4.1.tar.gz';
$sourcename[10]='mtd-utils-1.0.1.tar.gz';
$sourcename[11]='Arx-Kamikaze-5d8753b8e92c16d570386e0b29536534134fdf88.zip'; #u-boot/lib_bootstrap source code

my @patchname;
#name of patches
$patchname[0]='build.diff';
$patchname[1]='bb-brctl.patch';
$patchname[2]='apps.diff';
$patchname[3]='008-jffs2_make_lzma_available.patch'; #open source patch
$patchname[4]='linux-2.6.31.6-imq.man'; #open source patch
$patchname[5]='001-squashfs_move_zlib_decomp.patch'; #open source patch 
$patchname[6]='002-squashfs_factor_out_remaining_zlib.patch'; #open source patch 
$patchname[7]='003-squashfs_add_decompressor_framework.patch'; #open source patch 
$patchname[8]='004-squashfs_add_decompressor_lzma_lzo.patch'; #open source patch 
$patchname[9]='005-squashfs_extra_parameter.patch'; #open source patch 
$patchname[10]='006-squashfs_add_lzma.patch'; #open source patch 
$patchname[11]='007-squashfs_make_lzma_available.patch'; #open source patch 
$patchname[12]='linux-2.6.31.diff';
$patchname[13]='001-add_lzma_decompression_support.patch'; #open source patch
$patchname[14]='u-boot-1.1.4.diff';
$patchname[15]='020-iptables-disable-modprobe.patch';

my @foldername;
#name of opensource folder
$foldername[0]='buildroot-2009.08';
$foldername[1]='busybox-1.01';
$foldername[2]='iptables';
$foldername[3]='mips-linux-2.6.31';
$foldername[4]='lzma457';
$foldername[5]='sysstat-6.0.1';
$foldername[6]='u-boot';
$foldername[7]='wireless_tools.29';
$foldername[8]='busybox-1.15.0';
$foldername[9]='genext2fs-1.4.1';
$foldername[10]='mtd-utils-1.0.1';
$foldername[11]='Arx-Kamikaze/package/uboot-ifxmips/files/lib_bootstrap/';
	
my @folderpath;
#opensource folder destination
$folderpath[0]='../build/';
$folderpath[1]='../apps/';
$folderpath[2]='../apps/';
$folderpath[3]='../linux/kernels/';
$folderpath[4]='../apps/';
$folderpath[5]='../apps/';
$folderpath[6]='../boot/';
$folderpath[7]='../apps/';
$folderpath[8]='../apps/';
$folderpath[9]='../build/util/';
$folderpath[10]='../build/util/';
$folderpath[11]='../boot/u-boot/';
$folderpath[12]='../apps/iptables/iptables-1.4.5';

&Main();

sub Main{
	&CheckPatches;
	&CheckOpen;
	&ExtractOpen;	
	&MoveOpen;
	&ApplyPatches;
	exit;
}
# Move and verify open source
sub MoveOpen{
	my $name; my $count=0;
	# Create directories if needed
	print "\nCreating directories ~/apps/, ~/build/, ~/rootfs/, ~/linux/kernels/\n";
	system "cp ../linux/kernels/mips-linux-2.6.31/ath_version.mk mips-linux-2.6.31/ath_version.mk";
    system "rm -rf ../linux/kernels";
	system "mkdir ../linux/kernels; mkdir ../boot; mkdir ../patches;";
	
	# Move open source
	foreach $name(@foldername){system ("mv $name $folderpath[$count]") == 0 or die "\nsystem command failed: $?";$count++}
	$count=0;
	
	# Verify move
	foreach $name(@foldername){
		if (-e "$folderpath[$count]$name") {print "Moved $name open source found\n";}
		else {
			if (($count==11) && (-e "$folderpath[$count]lib_bootstrap")) {
				print "Moved u-boot/lib_bootstrap open source found\n";
				system "rm -rf Arx-Kamikaze";
			}
			else
			{
				print "Moved $name open source NOT found\n";
				exit -1;
			}
		} $count++;
	}
}
# Extract customer downloaded open source
sub ExtractOpen{
	my $extract; my $count=0;	
	
	# Change permissions on extracted files in case open source tars come from secondary sources
	system ('chmod -R 755 ../patches') == 0 or die "\system command failed: $?";
	foreach $extract(@sourcename){
		if ($count == 2) {system ("mkdir iptables; tar xvf $extract -C iptables") == 0 or die "\nsystem command failed: $?";}
		elsif ($count == 4) {system ("mkdir lzma457; tar xvf $extract -C lzma457") == 0 or die "\nsystem command failed: $?";}
		elsif ($count == 11) {system ("unzip $extract") == 0 or die "\nsystem command failed: $?";}
		else { system ("tar xvf $extract") == 0 or die "\nsystem command failed: $?";}
		$count++;	
	}
	system ('mv linux-2.6.31 mips-linux-2.6.31') == 0 or die "\nsystem command failed: $?"; 
	system ('mv u-boot-2006-04-06-1725 u-boot') == 0 or die "\nsystem command failed: $?";
	system ('mv Arx-Kamikaze-5d8753b8e92c16d570386e0b29536534134fdf88 Arx-Kamikaze') == 0 or die "\nsystem command failed: $?";
	return;
}
# Verify QCA supplied patches are in ~/patches
sub CheckPatches{
	my $checkfile;
	foreach $checkfile(@patchname){
		if (-e $checkfile) {print "Patch $checkfile found\n";}
		else {
			print "Patch $checkfile NOT found\n";
			exit -1;
		}
	}
	return;
}
# Verify downloaded open sources are in ~/pacthes
sub CheckOpen{
	my $checkfile;
	# Account for differenence in name for same source downloads between Linux and Windows	
	if (-e "5d8753b8e92c16d570386e0b29536534134fdf88.zip"){system ("mv 5d8753b8e92c16d570386e0b29536534134fdf88.zip Arx-Kamikaze-5d8753b8e92c16d570386e0b29536534134fdf88.zip") == 0 or die "\nsystem command failed: $?";}
	foreach $checkfile(@sourcename){
		if (-e $checkfile) {print "File $checkfile found\n";}
		else {
			print "File $checkfile NOT found\n";
			exit -1;
		}
	}
	return;
}
# Apply the patches in ~/patches, clean up for RC, using @folderpath in 2 different functions is not efficient	
sub ApplyPatches{
	my $patch; my $count=0;
	foreach $patch(@patchname){
		print "Applying patch $patch\n";
		#ignoring known errors on cetain patches
		if ($count == 1) {system "cd $folderpath[1]busybox-1.01; patch -p1 -u --binary <../../patches/$patch; cd ../busybox-1.15.0; patch -p1 -f -u --binary <../../patches/$patch";}
		elsif (($count >= 3) && ($count <= 12)) {system ("cd $folderpath[3]mips-linux-2.6.31; patch -p1 -u --binary <../../../patches/$patch") == 0 or die "\nsystem command failed: $?";}
		elsif ($count == 13) {system "cd $folderpath[11]; patch -p1 -f -u --binary <../../patches/$patch";}
		elsif ($count == 14) {system ("cd $folderpath[11]; patch -p1 -u --binary <../../patches/$patch") == 0 or die "\nsystem command failed: $?";}
		elsif ($count == 15) {system ("cd $folderpath[12]; patch -p2 -u --binary <../../../patches/$patch") == 0 or die "\nsystem command failed: $?";}	
		else {system ("cd $folderpath[$count]; patch -p1 -u --binary <../patches/$patch") == 0 or die "\nsystem command failed: $?";}
		$count++;
	}	
}
