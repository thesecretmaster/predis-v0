#!/bin/bash

for ent in types/*; do echo $ent; done | sed "s/types\/\([^\.]*\).*/\1/" | uniq -c | grep "\s*2\s" | sed "s/[[:space:]]*[0-9]*[[:space:]]\(.*\)/\1/g"
