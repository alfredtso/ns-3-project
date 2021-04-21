for pc in {1000..10000..1000}
do ./waf --run "scratch/udp-client-server.cc --PacketCount=$pc --useIpv6=false"
done
