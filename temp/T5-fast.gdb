break store_helper if addr == 0x80209000
echo HOST_READY\n
continue
set $ret = retaddr
set $env = env
p/x $ret
python
insns = gdb.selected_frame().architecture().disassemble(int(gdb.parse_and_eval('$ret')), count=20)
cmp_insn = next(x for x in insns if x['asm'].startswith('cmp '))
gdb.set_convenience_variable('cmp_pc', cmp_insn['addr'])
end
x/6i $cmp_pc
tbreak *$cmp_pc
continue
p/x $env->satp
p/x $rbx
p/x $rsi
p/x ((CPUTLBEntry *)$rdi)->addr_write
p/x ((CPUTLBEntry *)$rdi)->addend
si
p ($eflags >> 6) & 1
si
si
x/i $pc
si
p/x $rsi
p/x $r12
si
x/gx $rsi
python
assert int(gdb.parse_and_eval('*(unsigned long *)$rsi')) == int(gdb.parse_and_eval('$r12'))
end
detach
quit
