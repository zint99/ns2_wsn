BEGIN {
	highest_packet_id = 0; # 记录目前处理的封包的最大ID号码
}

{
    event = $1;
    if(event != "N") {
        time = $2;
        node = $3;
        # print node;
        len = length(node);
        if(len == 3){
            node = substr(node,2,1);
        }else{
            node = substr(node,2,2);
        }
        
        trace_type = $4;
        packet_id = $6;
        pkt_type = $7;
        pkt_size = $8;
        # if((event=="s")&&(node==0)&&(trace_type=="AGT")&&(start_time[packet_id]==0)){

        #     start_time[packet_id]=time;
        #     if(packet_id>highest_packet_id){
        #         highest_packet_id=packet_id;
        #     }
        # }

        if (event=="s" && node == 0 && pkt_type== "cbr" && trace_type == "AGT" && packet_id > highest_packet_id)
        {
            highest_packet_id = packet_id;
        }
        #记录封包的传送时间
        if (event=="s" && node == 0 && pkt_type== "cbr" && trace_type == "AGT" && start_time[packet_id] == 0){
            start_time[packet_id] = time;    # start_time[]表明这是一个数组if(event=="s" && node== "0") 
	    }
	    if(event=="r" && node == 10 && pkt_type== "cbr" && trace_type == "AGT" && end_time[packet_id] == 0) {
		    end_time[packet_id] = time;
	    }

        # if((event=="r")&&(node==10)&&(trace_type=="AGT")&&(end_tim[packet_id]==0)){
        #     end_time[packet_id]=time;
        # }
        # if(event=="d"){
        #     end_time[packet_id]=-1;
        # }
    }
}

END {
    #当每行资料都读取完毕后，开始计算有效封包的端到端延迟时间。
    for (packet_id = 0; packet_id <= highest_packet_id; packet_id++)
    {
        start = start_time[packet_id];
        end = end_time[packet_id];
        if(end != -1 && start < end){
            packet_duration = end - start;
            print packet_id,packet_duration
        }
        # printf("start:%f end:%f \n", start, end);
        #只把接收时间大于传送时间的记录打印出来
        # if ( start < end ) printf("%d %f\n", packet_id, packet_duration);
    }     
}