#!/bin/sh
# [TEMP] BF2_FRAME_TRACE summary: frame stats between "perf-stand: standing" and "perf-stand done" in a client.log
awk '/perf-stand: standing/{on=1} /perf-stand done/{on=0} on && /\[FRAME\] game/ {
  for(i=1;i<=NF;i++){ if($i ~ /^fps=/){v=substr($i,5); s+=v; n++; if(min==""||v<min)min=v}
    if($i ~ /^max=/){m=substr($i,5)+0; if(m>mx)mx=m} if($i=="render"){r+=$(i+1)} if($i=="update"){u+=$(i+1)} } }
  END{ if(n) printf "%s: %d s, mean fps %.1f, worst-second fps %.1f, worst frame %.0f ms, render %.1f ms, update %.1f ms\n", FILENAME, n, s/n, min, mx, r/n, u/n }' "$1"
