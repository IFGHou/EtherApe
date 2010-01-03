#!/usr/bin/perl -w

# Spits a constant flow of UDP packets with random source and destination addresses
# The porpouse is to test the DNS caching algorithm of EtherApe for leaks.
# All messages come and go to and from fixed unused ethernet addreses, so as not
# to disrupt normal network operations

$es="01:01:01:01:01:01";
$ed="02:02:02:02:02:02";


while (1)
{
    $sp=int(rand(65000))+1;
    $dp=int(rand(65000))+1;
    
    $s1=int(rand(253))+1;
    $s2=int(rand(253))+1;
    $s3=int(rand(253))+1;
    $s4=int(rand(253))+1;
    $sa=$s1.".".$s2.".".$s3.".".$s4;
    
    $d1=int(rand(253))+1;
    $d2=int(rand(253))+1;
    $d3=int(rand(253))+1;
    $d4=int(rand(253))+1;
    $da=$d1.".".$d2.".".$d3.".".$d4;
    
    $c="nemesis-udp -x ".$sp." -y ".$dp." -S ".$sa." -D ".$da." -H ".$es." -M ".$ed;
    
    print $c."\n";
    system ($c);
    
    sleep (.1)
}
    
