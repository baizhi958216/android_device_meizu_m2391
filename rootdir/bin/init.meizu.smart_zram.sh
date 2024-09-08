#!/vendor/bin/sh

smartzram_tool=/vendor/bin/smartzram_tool
swapfile_path=/data/vendor/smart_zram/swapfile
nandswap_sz_bytes=0
# zram size
swap_size_mb=0
fn_enable=false
data_type=erofs
prop_condition="persist.vendor.meizu.smart_zram.condition"
prop_nandswap_err="persist.vendor.meizu.smart_zram.err"
prop_nandswap_curr="persist.vendor.meizu.smart_zram.swapsize.curr"
prop_nandswap_size="persist.vendor.meizu.smart_zram.swapsize"

# unit M
zram_increase=0
smartswap_has_insmod=0
smartswap_init=0
zram2ufs_ratio=30
zram_critical_threshold=0
threshold_wakeup_smartswapd="0 0 0 0"
nandswap_sz_gb=0
dd_mb_cnt=0
# unit k
mem_total=0
zram_increase_limit=2048
vm_swappiness=160
direct_swappiness=0
comp_algorithm="lz4"
mem_gb=0

# readable
sz_16g_kb=16777216
sz_16g_mb=16384
sz_12g_kb=12582912
sz_12g_mb=12288
sz_8g_kb=8388608
sz_8g_mb=8192
sz_6g_kb=8388608
sz_6g_mb=6144
sz_10g_mb=10240

function configure_platform_parameters()
{
	platform_id=`cat /sys/devices/soc0/soc_id`
	echo "configure_platform_parameters"
	echo "mem_total "$mem_total
	case "$platform_id" in
		"519")
		# SM8550
		  # <= 8G
			if [[ $mem_total -lt $sz_8g_kb ]]; then
			# 6G
				swap_size_mb=$sz_6g_mb
				echo ""
			#
			else
				swap_size_mb=$sz_8g_mb
			fi
			;;
		"557")
		# SM8650
		  # <= 8G
			if [[ $mem_total -lt $sz_8g_kb ]]; then
			# 6G
				swap_size_mb=$sz_6g_mb
				echo "configure_platform_parameters 6g"
			#
			else
				swap_size_mb=$sz_8g_mb
				echo "configure_platform_parameters 8g"
			fi
			;;
		*)
			echo -e "***WARNING***: Invalid SoC ID\n"
			;;
	esac
	echo "swap_size_mb:"$swap_size_mb
}

function configure_smartswap_parameters()
{
	options_cfg=persist.vendor.meizu.smart_zram.cfg
	# <= 8G
	if [ $mem_total -le $sz_8g_kb ]; then
		zram2ufs_ratio=15
		threshold_wakeup_smartswapd="2400 2200 2400 1536"
		setprop $options_cfg "0/4/6/8"
	# 12G
	elif [ $mem_total -le $sz_12g_kb ]; then
		zram2ufs_ratio=15
		threshold_wakeup_smartswapd="3000 2700 3000 1536"
		setprop $options_cfg "0/4/8/12"
	# 16G
	elif [ $mem_total -le $sz_16g_kb ]; then
		zram2ufs_ratio=15
		threshold_wakeup_smartswapd="2500 2200 2500 1536"
		setprop $options_cfg "0/4/8/12"
	# > 16G
	else
		zram2ufs_ratio=15
		threshold_wakeup_smartswapd="2400 2200 2400 1536"
		setprop $options_cfg "0/4/8/12"
	fi

	# 8G * 15% = 1228MB
	#dd_mb_cnt=$(expr $swap_size_mb \* $zram2ufs_ratio \/ 100)
	#local eswap_size_mb=$(expr $nandswap_sz_gb \* 1024)
	#if [ $eswap_size_mb -lt $dd_mb_cnt ]; then
	#	dd_mb_cnt=$eswap_size_mb
	#fi
	zram_critical_threshold=$(expr $swap_size_mb \- 128)
}

function configure_zram_parameters()
{
	#echo $comp_algorithm > /sys/block/zram0/comp_algorithm
	if [ -f /sys/block/zram0/disksize ]; then

		if [ $swap_size_mb -eq 0 ]; then
			echo "swap_size_mb should define"
		fi

		zram_increase=$(expr $nandswap_sz_bytes \/ 1024 \/ 1024)
		if [ $zram_increase -gt $zram_increase_limit ]; then
			zram_increase=$zram_increase_limit
		fi

		if [[ "$fn_enable" == "false" || $smartswap_has_insmod -eq 0 ]]; then
			zram_increase=0
		fi
		# swap_size_mb is configured by configure_platform_parameters
		disksize=$(expr $swap_size_mb)
		#disksize=$(expr $swap_size_mb \+ $zram_increase)
		echo "$disksize""M" > /sys/block/zram0/disksize
		echo "disksize:"$disksize
	fi

	if [ $smartswap_has_insmod -eq 1 ]; then
		configure_smartswap_parameters
	fi
}

function configure_swappiness()
{

}

function zram_init()
{
	local magic=32758

	if [ $# -eq 1 ]; then
		magic=$1
	fi

	configure_swappiness

	mkswap /dev/block/zram0
	swapon /dev/block/zram0 -p $magic
}

function write_nandswap_err()
{
	setprop $prop_nandswap_err $1
	setprop $prop_condition false
}

function configure_nandswap_parameters()
{
	init=`getprop vendor.meizu.smart_zram.init`
	[ "$init" == "true" ] && exit

	setprop vendor.meizu.smart_zram.init true

	# hardcode data_type, too much selinux avc
	data_type=f2fs
	#data_type=`mount |grep -E " /data " |awk '{print $5}'`
	#[ $data_type != "f2fs" ] && [ $data_type != "ext4" ] && write_nandswap_err 1001 && return 22

	#is_ufs=`find /sys/bus/platform/devices/ |grep ufshc`
	#[ ! -n "$is_ufs" ] && write_nandswap_err 1002 && return 22

	if [ -f /sys/block/zram0/smartswap_core_enable ]; then
		smartswap_has_insmod=1
	fi

	nandswap_sz_gb=`getprop $prop_nandswap_size`
	if  [ -z $nandswap_sz_gb ]; then
		# default
		nandswap_sz_gb=4
	fi
	# compress: storage -> memory size / 2
	nandswap_sz_gb=$(expr $nandswap_sz_gb / 2)
	nandswap_sz_bytes=$(expr $nandswap_sz_gb \* 1024 \* 1024 \* 1024)

	return 0
}

function check_swapfile()
{
	if [ "$data_type" == "f2fs" ]; then
		check_pin=`$smartzram_tool -g $swapfile_path |awk '{print $2}'`
		[ "$check_pin" == "pinned" ] || ( rm -rf $swapfile_path && return 22 )
	fi

	check_size=`ls -al $swapfile_path |awk '{print $5}'`
	[ "$check_size" == "$nandswap_sz_bytes" ] || ( rm -rf $swapfile_path && return 22 )

	return 0
}

function nandswap_init()
{
	swap_offset=0
	swap_size=$(expr $nandswap_sz_gb \* 1024 \* 1024 \* 1024)
	dd_mb_cnt=$(expr $nandswap_sz_gb \* 1024)
	if [[ $smartswap_has_insmod -eq 1 ]]; then
		# only once
		if [ ! -f $swapfile_path ]; then
			dd if=/dev/zero of=$swapfile_path bs=1M count=$dd_mb_cnt
			fallocate -l ${swap_size} $swapfile_path

			[ "$data_type" == "f2fs" ] && $smartzram_tool -s1 $swapfile_path
		fi
	fi

	touch $swapfile_path
	check_swapfile
	if [ $? -eq 22 ]; then
		write_nandswap_err 1003
		return 22
	fi

	#for i in {0..2} ; do
	for i in 0 1 2; do
		losetup -f
		sleep 1
		loop_device=$(losetup -f -s $swapfile_path 2>&1)
		loop_device_ret=`echo $loop_device |awk -Floop '{print $1}'`
		if [ "$loop_device_ret" == "/dev/block/" ]; then
			break
		fi
		sleep 1
	done
	[ "$loop_device_ret" != "/dev/block/" ] && rm -rf $swapfile_path && write_nandswap_err 1004 && return 22

	set_dio=`$smartzram_tool -l $loop_device |awk '{print $2}'`
	#set_dio="success"
	if [ "$set_dio" == "success" ]; then
		if [ $smartswap_has_insmod -eq 1 ]; then
			chmod o+w `ls -l /sys/block/zram0/smartswap_* | grep ^'\-rw\-' | awk '{print $NF}'`
			echo "3 0 99 0 0 0 100 399 80 0 0 400 499 70 0 0 " > /dev/memcg/memory.swapd_memcgs_param
			#echo "3 0 99 0 0 0 100 399 80 50 0 400 499 70 50 0 " > /dev/memcg/memory.swapd_memcgs_param
			echo 400 > /dev/memcg/memory.app_score
			echo 300 > /dev/memcg/apps/memory.app_score
			echo root > /dev/memcg/memory.name
			echo apps > /dev/memcg/apps/memory.name
			echo "$threshold_wakeup_smartswapd" > /dev/memcg/memory.avail_buffers
			echo $zram_critical_threshold > /dev/memcg/memory.zram_critical_threshold
			echo 50 > /dev/memcg/memory.swapd_max_reclaim_size
			echo "1000 50" > /dev/memcg/memory.swapd_shrink_parameter
			echo 5000 > /dev/memcg/memory.max_skip_interval
			echo 50 > /dev/memcg/memory.reclaim_exceed_sleep_ms
			echo 60 > /dev/memcg/memory.cpuload_threshold
			echo 30 > /dev/memcg/memory.max_reclaimin_size_mb
			echo 80 > /dev/memcg/memory.zram_wm_ratio
			echo 512 > /dev/memcg/memory.empty_round_skip_interval
			echo 20 > /dev/memcg/memory.empty_round_check_threshold
			echo 1 > /sys/block/zram0/smartswap_loglevel
			echo -n $loop_device > /sys/block/zram0/smartswap_loop_device
			echo 1 > /sys/block/zram0/smartswap_enable
			echo 1 > /dev/memcg/memory.lat_loglevel
			echo 1 > /sys/block/zram0/smartswap_fault_out_first
			#echo "$zram_increase" > /sys/block/zram0/smartswap_zram_increase
			loop_device_num=`echo $loop_device |awk -F/ '{print $4}'`
			echo mq-deadline > /sys/block/$loop_device_num/queue/scheduler
			echo "0-1,5-6" > /dev/memcg/memory.swapd_bind
			smartswap_init=1

			zram_init
		else
			zram_init
			write_nandswap_err 1005
			losetup -d $loop_device
			rm -rf $swapfile_path

			return 22
		fi
	else
		zram_init
		write_nandswap_err 1006
		losetup -d $loop_device
		rm -rf $swapfile_path
		return 22
	fi

	return 0;
}

# only  getprop persist.vendor.meizu.smart_zram = ture, otherwise,  default behavior
function main()
{
	mem_total_str=`cat /proc/meminfo |grep MemTotal`
	mem_total=${mem_total_str:16:8}
	fn_enable=`getprop persist.vendor.meizu.smart_zram`
	sz_cfg=`getprop $prop_nandswap_size`
	sz_cur=`getprop $prop_nandswap_curr`

	if [ "$fn_enable" != "true" ]; then
		echo "fn_enable not set or false"
		setprop vendor.meizu.smart_zram.init_ok false
		exit -1
	fi

	if  [ -z $sz_cfg ]; then
		# default
		setprop $prop_nandswap_size 4
		sz_cfg=4
	elif  [ $sz_cfg -eq 0 ]; then
		setprop $prop_nandswap_size 0
		sz_cfg=0
	fi

	if  [ -z $sz_cur ]; then
		sz_cur=0
	fi

	if [ -z $mem_total ]; then
		echo -e "read meminfo failed\n"
		setprop vendor.meizu.smart_zram.init_ok false
		exit -1
	fi

	# enable -> disable, or swap size change, need rm
	if [ -f $swapfile_path ]; then
		if [ $sz_cur -ne $sz_cfg ]; then
			rm -rf $swapfile_path
		fi
		if [ "$fn_enable" != "true" ]; then
			rm -rf $swapfile_path
		fi
	fi

	# record curr size by config size
	setprop $prop_nandswap_curr $sz_cfg
	configure_platform_parameters

	configure_nandswap_parameters
	ret=$?
	if [ $ret -eq 22 ]; then
		nandswap_sz_bytes=0
		configure_zram_parameters
		setprop $prop_nandswap_curr 0
		zram_init
		setprop vendor.meizu.smart_zram.init_ok false
		exit 0
	fi

	configure_zram_parameters
	if [ "$fn_enable" == "true" -a "$nandswap_sz_bytes" != "0" ]; then
		nandswap_init
		if [ $? -eq 22 ]; then
 			zram_init
			setprop vendor.meizu.smart_zram.init_ok false
			exit -1
		fi
		setprop vendor.meizu.smart_zram.init_ok true
		exit 0
	else
		setprop $prop_nandswap_curr 0
		setprop vendor.meizu.smart_zram.init_ok true
	fi
	zram_init
}

main
