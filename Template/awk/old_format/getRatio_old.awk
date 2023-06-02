# 计算分组投递率
BEGIN {
    sends = 0;
    recvs = 0;
    forward = 0;
}
#应用层收到包
$0 ~/^s.* AGT/ {
    sendLine ++ ;
}
#应用层发送包
$0 ~/^r.* AGT/ {
    recvLine ++ ;
}
#路由转发包
$0 ~/^f.* RTR/ {
    fowardLine ++ ;
}
#输出结果
END {
    printf("finish\n");
    printf("cbr send: %d, recv: %d, forward: %d, r/s Ratio: %.4 \n", sendLine, recvLine,fowardLine, (recvLine/sendLine)) ;
}