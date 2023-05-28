# 计算路由发起频率

BEGIN{
    requests=0;
    frequency=0;
}
{
    event=$1;
    if(event != "M" && event != "N"){
        trace_type=$4;
        packet_id=$6;
        packet_type=$7;
        # 使用正则表达式匹配第 24 列中的数字
        match($24, /\[([0-9]+):/);
        source_ip=substr($24, RSTART+1, RLENGTH-2);
        aodv_type=substr($35, 2, 7);

        if((event=="s")&&(aodv_type=="REQUEST")&&(packet_id==source_ip))
        {
            requests++;
        }
    }
}
END{
    frequency=requests/300;
    printf("%.4f\n",frequency);
}