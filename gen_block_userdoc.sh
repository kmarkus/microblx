
BLOCKS="const_int random pid luablock ramp_int32 ptrig trig lfds_cyclic hexdump mqueue"

for b in $BLOCKS; do
    ubx-modinfo dump -f rest > docs/user/block_$b
done
