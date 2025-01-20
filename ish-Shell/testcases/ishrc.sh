#!/bin/sh
cp ../testcases/ishrc ~/.ishrc
./ish < ../testcases/ishrc.ish
rm -f ~/.ishrc
