for pc in {15000000..150000000..15000000}
do ./waf --run "scratch/tcp-test.cc --SendSize=$pc"
done
