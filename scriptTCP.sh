for pc in 512 1024
do ./waf --run "scratch/tcp-test.cc --SendSize=$pc"
done
