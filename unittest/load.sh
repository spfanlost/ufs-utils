#!/bin/bash


lsscsi -g

# if [ -n "$lsscsi" ] ; then
#    echo "Find device!"
# else
#    echo -e "\033[41;37mWARNING: NOT FIND UFS controller!!!\033[0m"  
#    exit 0
# fi

AUTO_HEADER="${PWD}/auto_header.h"
echo ${AUTO_HEADER}
rm -f ${AUTO_HEADER}


lun0=`lsscsi|sed -n '1p'`
lun1=`lsscsi|sed -n '2p'`
lun2=`lsscsi|sed -n '3p'`
lun3=`lsscsi|sed -n '4p'`
lun4=`lsscsi|sed -n '5p'`
lun5=`lsscsi|sed -n '6p'`
wlun_boot=`lsscsi|sed -n '7p'`
wlun_rpmb=`lsscsi|sed -n '8p'`
wlun_device=`lsscsi|sed -n '9p'`

# echo "${wlun_boot:1:11}"
# echo "${wlun_rpmb:1:11}"
# echo "${wlun_device:1:11}"

echo "#ifndef __AUTO_HEADER_H__">>${AUTO_HEADER}
echo "#define __AUTO_HEADER_H__">>${AUTO_HEADER}
echo "">>${AUTO_HEADER}

echo "#define UFS_RW_BUFFER_SIZE (100*2024*1024)">>${AUTO_HEADER}
echo "#define UFS_WLUN_DEVICE \"0:0:0:49488\"">>${AUTO_HEADER}
echo "#define UFS_WLUN_BOOT \"0:0:0:49456\"">>${AUTO_HEADER}
echo "#define UFS_WLUN_RPMB \"0:0:0:49476\"">>${AUTO_HEADER}

echo "">>${AUTO_HEADER}
echo "#endif /* __AUTO_HEADER_H_ */">>${AUTO_HEADER} 
echo -e "\033[32m----- make! -----\033[0m"
make clean
make -j
./test
echo -e "\n\033[32mrun test!\033[0m"
