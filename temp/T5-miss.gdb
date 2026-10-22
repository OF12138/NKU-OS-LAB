break store_helper if addr == 0xffffffffc0207ff8
echo HOST_READY\n
continue
until accel/tcg/cputlb.c:1519
p/x addr
p/x val
p mmu_idx
p index
p sizeof(CPUTLBEntry)
p/x env_tlb(env)->f[1].mask
p/x tlb_addr
p tlb_hit(tlb_addr, addr)
bt 4
set $env = env
set $addr = addr
set $idx = index
break tlb_set_page_with_attrs if vaddr == 0xffffffffc0207000
continue
p/x vaddr
p/x paddr
p prot
p size
finish
p/x env_tlb($env)->f[1].table[$idx].addr_write
p/x env_tlb($env)->f[1].table[$idx].addend
p tlb_hit(env_tlb($env)->f[1].table[$idx].addr_write, $addr)
set $host = (unsigned long)($addr + env_tlb($env)->f[1].table[$idx].addend)
p/x $host
break accel/tcg/cputlb.c:1607 if addr == 0xffffffffc0207ff8
continue
next
p/x haddr
finish
x/gx $host
python
assert int(gdb.parse_and_eval('*(unsigned long *)$host')) == 0x80000a02
end
detach
quit
