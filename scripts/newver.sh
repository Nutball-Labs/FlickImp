#!/bin/bash

# First set a new version or save and quit to bypass
vim /z/Nutball-Labs/FlickImp/lib/version.hpp

fmt_time() {
    local s=$1
    local h=$((s/3600)) m=$((s%3600/60)) sec=$((s%60))
    [ $h -gt 0 ] && printf "%dh %02dm %02ds" $h $m $sec \
                 || printf "%dm %02ds" $m $sec
}

t_start=$SECONDS

# Run a new compile
t_build=$SECONDS
/z/Nutball-Labs/FlickImp/scripts/build-linux.sh
t_build=$((SECONDS - t_build))

# Package that new build
t_pkg=$SECONDS
/z/Nutball-Labs/FlickImp/scripts/package-linux.sh
t_pkg=$((SECONDS - t_pkg))

t_total=$((SECONDS - t_start))

echo ""
echo "────────────────────────────────────"
printf "  Build:    %s\n" "$(fmt_time $t_build)"
printf "  Package:  %s\n" "$(fmt_time $t_pkg)"
printf "  Total:    %s\n" "$(fmt_time $t_total)"
echo "────────────────────────────────────"
# SN: 00122
