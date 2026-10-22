break get_physical_address if addr == 0x80209000 && access_type == 1
echo HOST_READY\n
continue
p/x env->satp
set $bare_satp = env->satp
set $bare_va = addr
p mmu_idx
bt 5
set $phys = physical
until target/riscv/cpu_helper.c:197
p vm
next
p/x *physical
set $bare_pa = *physical
finish
break tlb_set_page_with_attrs if vaddr == 0x80209000
continue
p/x vaddr
p/x paddr
p size
break get_physical_address if addr == 0xffffffffc0207ff8 && access_type == 1
continue
p/x env->satp
set $sv39_satp = env->satp
set $sv39_va = addr
p mmu_idx
bt 5
set $phys = physical
finish
p/x *$phys
set $sv39_pa = *$phys
break tlb_set_page_with_attrs if vaddr == 0xffffffffc0207000
continue
p/x vaddr
p/x paddr
p size
set trace-commands off
printf "BARE / SV39: observed S-mode 8-byte stores\n"
printf "Bare: satp=%#lx VA=%#lx PA=%#lx\n", $bare_satp, $bare_va, $bare_pa
printf "Sv39: satp=%#lx VA=%#lx PA=%#lx\n", $sv39_satp, $sv39_va, $sv39_pa | ($sv39_va & 0xfff)
printf "Sv39 TLB fill: vaddr=%#lx paddr=%#lx size=%lu\n", vaddr, paddr, size
printf "Both backtraces: store_helper -> tlb_fill -> riscv_cpu_tlb_fill -> get_physical_address\n"
printf "END MODES\n"
set trace-commands on
p/x $bare_satp
p/x $bare_va
p/x $bare_pa
p/x $sv39_satp
p/x $sv39_va
p/x $sv39_pa | ($sv39_va & 0xfff)
p size
detach
quit
