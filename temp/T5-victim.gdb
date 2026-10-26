break victim_tlb_hit if mmu_idx == 1 && page == 0xffffffffc2200000
ignore 1 1
echo HOST_READY\n
continue
bt 3
p/x page
p index
p elt_ofs
set $env = env
p/x env_tlb(env)->f[1].mask
p/x env_tlb(env)->f[1].table[index].addr_read
set $old_main = env_tlb(env)->f[1].table[index].addr_read
set $i = 0
set trace-commands off
while $i < 8
printf "victim[%d].addr_read = %#lx\n", $i, env_tlb($env)->d[1].vtable[$i].addr_read
set $i = $i + 1
end
set trace-commands on
break accel/tcg/cputlb.c:996
continue
p vidx
set $vi = vidx
set $old_victim = env_tlb($env)->d[1].vtable[$vi].addr_read
finish
set $hit = $
p $hit
p/x $old_main
p/x $old_victim
p/x env_tlb($env)->f[1].table[0].addr_read
p/x env_tlb($env)->d[1].vtable[$vi].addr_read
set trace-commands off
printf "VICTIM TLB: main[0] and victim[%d]\n", $vi
printf "victim_tlb_hit returned: %d\n", $hit
printf "Before: main[0].addr_read=%#lx\n", $old_main
printf "Before: victim[%d].addr_read=%#lx\n", $vi, $old_victim
printf "After:  main[0].addr_read=%#lx\n", env_tlb($env)->f[1].table[0].addr_read
printf "After:  victim[%d].addr_read=%#lx\n", $vi, env_tlb($env)->d[1].vtable[$vi].addr_read
printf "END VICTIM\n"
set trace-commands on
detach
quit
