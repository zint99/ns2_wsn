# 计算路由发起频率 - new trace format
BEGIN{
    requests=0;
    frequency=0;
}
{
    event=$1;
    if(event != "M" && event != "N"){
        id=$5;
        source_ip=$57;
        aodv_type=$61;

        if((event=="s")&&(aodv_type=="REQUEST")&&(id==source_ip))
        {
            requests++;
        }
    }
}
END{
    frequency=requests/300;
    printf("%.4f\n",frequency);
}