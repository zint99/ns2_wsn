#=============================================
#   该脚本为节点4 * 4固定放置的仿真场景
#       - 手动指定CBR数据
#           - 节点什么时候开始发送数据流到另外一个节点
#       - 指定节点位置排列
#=============================================


#===================================
#     Simulation parameters setup
#===================================
set val(chan)   Channel/WirelessChannel    ;# channel type
set val(prop)   Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)  Phy/WirelessPhy            ;# network interface type
set val(mac)    Mac/802_11                 ;# MAC type
set val(ifq)    Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)     LL                         ;# link layer type
set val(ant)    Antenna/OmniAntenna        ;# antenna model
set val(ifqlen) 50                         ;# max packet in ifq
set val(nn)     16                         ;# number of mobilenodes
set val(rp)     AODV                       ;# routing protocol
set val(x)      1000                      ;# X dimension of topography
set val(y)      1000                      ;# Y dimension of topography
set val(stop)   10.0                         ;# time of simulation end
set val(energymodel) EnergyModel ; # Energy set up
set scriptName [file tail [info script]]
set baseName [file rootname $scriptName]
set val(tr)     $baseName.tr
set val(nam)    $baseName.nam

#===================================
#        Initialization        
#===================================
#Create a ns simulator
set ns [new Simulator]

#Setup topography object
set topo       [new Topography]
$topo load_flatgrid $val(x) $val(y)
create-god $val(nn)

#Open the NS trace file
set tracefile [open $val(tr) w]
$ns trace-all $tracefile

#Open the NAM trace file
set namfile [open $val(nam) w]
$ns namtrace-all $namfile
$ns namtrace-all-wireless $namfile $val(x) $val(y)
set chan [new $val(chan)];#Create wireless channel

#===================================
#     Mobile node parameter setup
#===================================
$ns node-config -adhocRouting  $val(rp) \
                -llType        $val(ll) \
                -macType       $val(mac) \
                -ifqType       $val(ifq) \
                -ifqLen        $val(ifqlen) \
                -antType       $val(ant) \
                -propType      $val(prop) \
                -phyType       $val(netif) \
                -channel       $chan \
                -topoInstance  $topo \
                -energyModel $val(energymodel) \
                -initialEnergy 10 \
                -rxPower 0.5 \
                -txPower 1.0 \
                -idlePower 0.1 \
                -agentTrace    ON \
                -routerTrace   ON \
                -macTrace      OFF \
                -movementTrace OFF

#===================================
#        Nodes Definition        
#===================================
#Create 16 nodes
set n1 [$ns node]
$n1 set X_ 200
$n1 set Y_ 200
$n1 set Z_ 0.0
$ns at 0.0 "$n1 label Source"
#$n1 node-id 1
$ns initial_node_pos $n1 60
set n2 [$ns node]
$n2 set X_ 400
$n2 set Y_ 200
$n2 set Z_ 0.0
#$n2 node-id 2
$ns initial_node_pos $n2 60
set n3 [$ns node]
$n3 set X_ 600
$n3 set Y_ 200
$n3 set Z_ 0.0
#$n3 node-id 3
$ns initial_node_pos $n3 60
set n4 [$ns node]
$n4 set X_ 800
$n4 set Y_ 200
$n4 set Z_ 0.0
#$n4 node-id 4
$ns initial_node_pos $n4 60
set n5 [$ns node]
$n5 set X_ 200
$n5 set Y_ 400
$n5 set Z_ 0.0
#$n5 node-id 5
$ns initial_node_pos $n5 60
set n6 [$ns node]
$n6 set X_ 400
$n6 set Y_ 400
$n6 set Z_ 0.0
#$n6 node-id 6
$ns initial_node_pos $n6 60
set n7 [$ns node]
$n7 set X_ 600
$n7 set Y_ 400
$n7 set Z_ 0.0
#$n7 node-id 7
$ns initial_node_pos $n7 60
set n8 [$ns node]
$n8 set X_ 800
$n8 set Y_ 400
$n8 set Z_ 0.0
#$n8 node-id 8
$ns initial_node_pos $n8 60
set n9 [$ns node]
$n9 set X_ 200
$n9 set Y_ 600
$n9 set Z_ 0.0
#$n9 node-id 9
$ns initial_node_pos $n9 60
set n10 [$ns node]
$n10 set X_ 400
$n10 set Y_ 600
$n10 set Z_ 0.0
#$n10 node-id 10
$ns initial_node_pos $n10 60
set n11 [$ns node]
$n11 set X_ 600
$n11 set Y_ 600
$n11 set Z_ 0.0
$ns at 0.0 "$n11 label Destination"
#$n11 node-id 11
$ns initial_node_pos $n11 60
set n12 [$ns node]
$n12 set X_ 800
$n12 set Y_ 600
$n12 set Z_ 0.0
#$n12 node-id 12
$ns initial_node_pos $n12 60
set n13 [$ns node]
$n13 set X_ 200
$n13 set Y_ 800
$n13 set Z_ 0.0
#$n13 node-id 13
$ns initial_node_pos $n13 60
set n14 [$ns node]
$n14 set X_ 400
$n14 set Y_ 800
$n14 set Z_ 0.0
#$n14 node-id 14
$ns initial_node_pos $n14 60
set n15 [$ns node]
$n15 set X_ 600
$n15 set Y_ 800
$n15 set Z_ 0.0
#$n15 node-id 15
$ns initial_node_pos $n15 60
set n16 [$ns node]
$n16 set X_ 800
$n16 set Y_ 800
$n16 set Z_ 0.0
#$n16 node-id 16
$ns initial_node_pos $n16 60

#===================================
#        Agents Definition        
#===================================
#Setup a UDP connection
set udp0 [new Agent/UDP]
$ns attach-agent $n1 $udp0
set null1 [new Agent/Null]
$ns attach-agent $n11 $null1
$ns connect $udp0 $null1
$udp0 set packetSize_ 1500


#===================================
#        Applications Definition        
#===================================
#Setup a CBR Application over UDP connection
set cbr0 [new Application/Traffic/CBR]
$cbr0 attach-agent $udp0
$cbr0 set packetSize_ 1000
$cbr0 set rate_ 1.0Mb
$cbr0 set random_ null
$ns at 1.0 "$cbr0 start"
$ns at $val(stop) "$cbr0 stop"


#===================================
#        Termination        
#===================================
#Define a 'finish' procedure
proc finish {} {
    global ns tracefile namfile 
    close $tracefile
    close $namfile
    exit 0
}
for {set i 1} {$i <= $val(nn) } { incr i } {
    $ns at $val(stop) "\$n$i reset"
}
$ns at $val(stop) "$ns nam-end-wireless $val(stop)"
$ns at $val(stop) "finish"
$ns at $val(stop) "puts \"done\" ; $ns halt"
$ns run