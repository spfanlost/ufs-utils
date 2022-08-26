VD RB5 UFS验证环境搭建

A.新板子上手
1.连接
使用串口连接RB5，或者接显示器、鼠标、键盘启动,，板子要联网。

2.账号
用户名：root
密码：oelinux123

3.安装软件：
apt-get install lsscsi ntpdate gcc make git sudo

4.更新时间
断电后重新上电启动，需要更新时间
ntpdate 10.30.1.4
如果不能联网需要手动更改时间如：
date -s "20190712 18:30:50"

5.允许root远程登录
vim /etc/ssh/sshd_config
查找 PermitRootLogin ,去掉# 并修改这个为PermitRootLogin yes

6.ufs-utils
git clone --depth=1 https://github.com/westerndigitalcorporation/ufs-utils.git
git clone --depth=1 https://github.com/spfanlost/ufs-utils.git

7.ufs-tool
git clone --depth=1 https://github.com/AviShchislowski-zz/ufs-tool.git

Note1：
使用U盘
mkdir /mnt/udisk
mount /dev/sdbx /mnt/udisk
cp /mnt/udisk ~/

umount /mnt/udisk

Note2：
zip解压
unzip *.zip


B.ufs-utils
1.软件安装Vscode和插件
复制 L:\ddtest\meng_yu\sw\VSCode 到电脑本地并安装VSCodeSetup-x64.exe 
打开Vscode，选择左侧的Extensions，点击左侧框框右上角的菜单中Install form VSIX...选择上述文件夹的*.vsix文件
安装的插件有：
Remote-SSH,c/c++,doxygen doc,git histtory,hex editer

2.安装编译器

3.查看源码


