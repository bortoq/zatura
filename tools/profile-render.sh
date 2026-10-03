#!/bin/sh
# SPDX-License-Identifier: Zlib
set -eu
binary=${1:-build/tests/benchmark_render}
output=${2:-dist/render-profile.csv}
mkdir -p "$(dirname "$output")"
export GTK_A11Y=none GSK_RENDERER=cairo
printf '%s\n' 'zoom,device_scale,columns,budget_mib,recolor,raster_width,cold_ms,adjustment_ms,raw_mib,display_mib,unique_cache_mib,peak_rss_mib' > "$output"
for scenario in '1 1 1 256 0' '1 1 2 256 0' '2 1 2 256 0' '1 2 2 256 0' '2 2 2 256 0' '2 2 2 64 0' '2 2 2 512 0' '1 2 2 256 1'; do
  # Scenario deliberately expands into five numeric arguments.
  xvfb-run -s '-screen 0 1400x900x24 -ac +extension GLX +render -noreset' -a "$binary" $scenario >> "$output"
done
printf '%s\n' "$output"
