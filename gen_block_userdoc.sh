#!/usr/bin/bash

BLOCK_INDEX=docs/user/block_index.rst

BLOCKS="trig ptrig \
	math_double \
	rand_double \
	ramp_double \
	pid \
	saturation_double \
	luablock \
	cconst \
	iconst \
        lfds_cyclic mqueue hexdump \
	"

cat <<EOF > $BLOCK_INDEX
Microblx Module Index
=====================

EOF

for b in $BLOCKS; do
    ubx-modinfo dump $b -f rest > docs/user/block_$b.rst
    echo ".. include:: block_$b.rst" >> $BLOCK_INDEX
done
