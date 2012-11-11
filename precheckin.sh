#!/bin/sh
for x in nogl; do
	cp qtwayland.spec qtwayland-$x.spec
	sed -i "s/define _qtwayland_variant .*/define _qtwayland_variant $x/g" qtwayland-$x.spec
done
