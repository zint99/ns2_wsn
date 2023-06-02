# 计算归一化路由开销 - new trace format
BEGIN{
    recvLine=0;
    load=0;
    Normalized_load=0;
}
{
    event=$1;
    if(event != "M" && event != "N"){
        time=$3;
        trace_type=$19;
        packet_type=$35;
        if((event=="r")&&(trace_type=="AGT")&&(packet_type=="cbr"))
        {
            recvLine++;
        }
        if(((event=="s")||(event=="f"))&&(trace_type=="RTR")&&((packet_type=="AODV")||(packet_type=="message")))
        {
            load++;
        }
    }
    
}
END{
    Normalized_load=load/recvLine;
    printf("%.4f\n",Normalized_load);
}