# linux-device-dirver

一个简单的字符设备驱动程序，实现了open，release，read，write，llseek等功能。
参照《linux设备驱动程序》第三章的代码实现  
未考虑信号量同步

usage：  
#make  
#sudo ./scull_load  
test file in /test dir  


after test
#sudo ./scull_unload  


