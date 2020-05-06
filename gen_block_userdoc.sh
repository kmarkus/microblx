#!/usr/bin/bash

BLOCKS="random pid luablock ramp_int32 cconst iconst ptrig trig lfds_cyclic hexdump mqueue"
BLOCK_INDEX=docs/user/block_index.rst

cat <<EOF > $BLOCK_INDEX
Microblx Module Index
=====================

EOF

for b in $BLOCKS; do
    ubx-modinfo dump $b -f rest > docs/user/block_$b.rst
    echo ".. include:: block_$b.rst" >> $BLOCK_INDEX
done
