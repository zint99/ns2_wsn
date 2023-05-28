#=========================================================================================
#   无线传感器网络多路径路由协议研究
#   脚本适用于由setdest & cbrgen随机生成不同发包速率仿真场景的脚本，需要注意：
#   1. cbrgen生成不同发包速率的数据流文件格式为：cbrX.X，作为参数传入该脚本 -> Usage: ns aodv.tcl cbrX.X
#   2. 生成的nam文件与trace文件自动命名为：$baseName$opt(cp).tr/nam.
#       - 分别自动保存在同一目录下的trace & nam文件夹下（注意创建）
#
#=========================================================================================

# shell process
# 获取命令行参数
set argc [llength $argv]
set argv0 [lindex $argv 0]
# 判断命令行参数是否符合要求
if {$argc != 1 || ![string match "cbr*" $argv0]} {
    puts "Usage: ns aodv.tcl cbrX.X"
    exit 1
}

#=====================================

#Define options

#=====================================
set val(chan)           Channel/WirelessChannel    ;#Channel Type
set val(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)          Phy/WirelessPhy            ;# network interface type
set val(mac)            Mac/802_11                 ;# MAC type
set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         50                         ;# max packet in ifq
set val(nn)             50                          ;# number of mobilenodes
set val(rp)             AODV                       ;# routing protocol
set val(x)  800
set val(y)  800
set val(stop) 300
set val(energymodel) EnergyModel ; # Energy set up

# 从命令行参数中获取 cbr 文件名，并保存到 opt(cp) 变量中
set opt(cp) [lindex $argv 0]
puts "Using cbr file: $opt(cp)"
set opt(sc) "sc-n50-p0-M0-t300-x800-y800";  # 若P设置成和仿真时间一样长，则NAM中会异常

set scriptName [file tail [info script]]
set baseName [file rootname $scriptName]
set outName $baseName$opt(cp)
set val(tr)     $outName.tr
set val(nam)    $outName.nam

#===============================

# Main Program

#===============================

# 创建ns对象
set ns_ [new Simulator]
# 创建trace文件
set tracefd [open trace/$val(tr) w]
# use new trace file
#$ns_ use-newtrace
$ns_ trace-all $tracefd
set namtracefd [open nam/$val(nam) w]
$ns_ namtrace-all-wireless $namtracefd $val(x) $val(y)

# 创建topo
set topo [new Topography]
$topo load_flatgrid $val(x) $val(y)

# 创建GOD
set god_ [new God]
create-god $val(nn)

set chan [new $val(chan)];#Create wireless channel

# node config
$ns_ node-config -adhocRouting $val(rp) \
    -llType $val(ll) \
    -macType $val(mac) \
    -ifqType $val(ifq) \
    -ifqLen $val(ifqlen) \
    -antType $val(ant) \
    -propType $val(prop) \
    -phyType $val(netif) \
    -channel $chan \
    -topoInstance $topo \
    -energyModel $val(energymodel) \
                -initialEnergy 30.0 \
                -rxPower 0.395 \
                -txPower 0.660 \
                -idlePower 0.035 \
    -agentTrace ON \
    -routerTrace ON \
    -macTrace OFF \
    -movementTrace OFF \

    # create node
    for {set i 0} {$i < $val(nn)} {incr i} {
        set node_($i) [$ns_ node]
        $ns_ initial_node_pos $node_($i) 20
        $node_($i) random-motion 0
    }

# load cp & sc file
puts "Loading traffic file..."
source $opt(cp)
puts "Loading scenario file..."
source $opt(sc)

# reset nodes
for {set i 0} {$i < $val(nn)} {incr i} {
    $ns_ at $val(stop) "$node_($i) reset";
}

$ns_ at $val(stop) "finish"

$ns_ at $val(stop) "puts \"NS EXITING...\"; $ns_ halt"

proc finish {} {
    global ns_ tracefd namtracefd
    $ns_ flush-trace
    close $tracefd
    close $namtracefd
    exit 0
}

puts "Starting Simulation..."

$ns_ run