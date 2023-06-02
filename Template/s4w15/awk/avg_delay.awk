# awk script about avg_delay for new trace format
BEGIN{
       highest_packet_id=0;
       duration_total=0;
}

$0 ~/^r.*AGT/{
       receives++;
}

{
       time=$3;
       packet_id=$41;
       if(($1=="s")&&($19=="AGT")&&(start_time[packet_id]==0)){
              start_time[packet_id]=time;
       if(packet_id>highest_packet_id)
       highest_packet_id=packet_id;
       }

       if(($1=="r")&&($19=="AGT")&&(end_tim[packet_id]==0)){
              end_time[packet_id]=time;
       }
       if($1=="d"){
       end_time[packet_id]=-1;
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

       printf("%.4f\n",duration_total/receives);

}
