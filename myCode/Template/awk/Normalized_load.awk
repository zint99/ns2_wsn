# 计算归一化路由开销
BEGIN{
    recvLine=0;
    load=0;
    Normalized_load=0;
}
{
    event=$1;
    if(event != "M" && event != "N"){
        time=$2;
        trace_type=$4;
        packet_id=$6;
        packet_type=$7;
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
    printf("l: %d, nload: %.4f\n",load,Normalized_load);
}