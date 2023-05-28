# 计算平均端到端延时
BEGIN{
    highest_packet_id=0;
    duration_total=0;
}

$0 ~/^r.*AGT/{
    receives++;
}
{
    event=$1;
    if(event != "M" && event != "N"){
        time=$2;
        trace_type=$4;
        packet_id=$6;
        if((event=="s")&&(trace_type=="AGT")&&(start_time[packet_id]==0)){
                start_time[packet_id]=time;
            if(packet_id>highest_packet_id){
                highest_packet_id=packet_id;
            }
        }
        if((event=="r")&&(trace_type=="AGT")&&(end_tim[packet_id]==0)){
            end_time[packet_id]=time;
        }
        if(event=="d"){
            end_time[packet_id]=-1;
        }
    }
}
END{
    for(packet_id=0;packet_id<=highest_packet_id;packet_id++){
            start=start_time[packet_id];
            end=end_time[packet_id];
        if(end!=-1&&start<end){
            packet_duration=end-start;
            duration_total=duration_total+packet_duration;
        }
    }
    print duration_total/receives
}