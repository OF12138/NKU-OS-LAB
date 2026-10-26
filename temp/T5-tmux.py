"""在真实 tmux 三窗口中重放双 GDB 实验，保留交互界面及原始窗格内容。

WSL 登录 shell：python3 temp/T5-tmux.py miss|fast|victim|modes
"""
from pathlib import Path
import shlex
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
name = sys.argv[1]
session = 'lyp-t5-' + name
code = Path.home() / 'Code/NKU-OS-LAB/lyp-t5-c0844a7/code'
qemu = Path.home() / 'Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64'

def tmux(*args):
    return subprocess.check_output(['tmux', *args], text=True)

def send(target, line):
    tmux('send-keys', '-t', target, '-l', line)
    tmux('send-keys', '-t', target, 'Enter')

def wait_prompt():
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        pane = tmux('capture-pane', '-p', '-t', session + ':host')
        if pane.rstrip().endswith('(gdb)'):
            return
        time.sleep(0.1)
    raise RuntimeError('GDB 未返回提示符：\n' + pane)

def capture():
    content = tmux('capture-pane', '-p', '-e', '-S', '-', '-t', session + ':host')
    (root / f'temp/T5-{name}.ans').write_text(content)

if subprocess.run(['tmux', 'has-session', '-t', session], capture_output=True).returncode == 0:
    raise SystemExit(f'{session} 已存在，请先检查该实验窗口。')
tmux('new-session', '-d', '-s', session, '-n', 'qemu', '-x', '112', '-y', '42')
tmux('set-option', '-t', session, 'history-limit', '10000')
tmux('set-option', '-t', session, 'status', 'off')
qcmd = shlex.join([str(qemu), '-machine', 'virt', '-nographic', '-bios', 'default',
    '-device', f'loader,file={code}/bin/ucore.img,addr=0x80200000',
    '-gdb', 'tcp:127.0.0.1:12345', '-S'])
send(session + ':qemu', 'exec ' + qcmd)
time.sleep(0.3)
pid = tmux('display-message', '-p', '-t', session + ':qemu', '#{pane_pid}').strip()
tmux('new-window', '-t', session, '-n', 'host')
send(session + ':host', f'sudo -n gdb -q -nx -p {pid}')
time.sleep(0.5)
wait_prompt()
for cmd in ['set pagination off', 'set confirm off', 'set print frame-arguments none',
            'set print pretty off', 'set style enabled on', 'handle SIGPIPE nostop noprint']:
    send(session + ':host', cmd)
    time.sleep(0.1)
    wait_prompt()

started = False
block = []
lines = (root / f'temp/T5-{name}.gdb').read_text().splitlines()
for line in lines:
    if not line or line.startswith(('echo HOST_READY', 'set trace-commands')):
        continue
    if line.startswith('printf '):
        continue
    if line in ('detach', 'quit'):
        capture()
        break
    if block:
        block.append(line)
        if line != 'end':
            continue
        commands, block = block, []
    elif line == 'python' or line.startswith('while '):
        block = [line]
        continue
    else:
        commands = [line]
    for command in commands:
        send(session + ':host', command)
        time.sleep(0.08)
    if not started and line == 'continue':
        started = True
        tmux('new-window', '-t', session, '-n', 'guest')
        gcmd = shlex.join(['riscv64-unknown-elf-gdb', '-q', '-nx', str(code / 'bin/kernel'),
            '-ex', 'set remotetimeout 120', '-ex', 'target remote 127.0.0.1:12345', '-ex', 'continue'])
        send(session + ':guest', gcmd)
    wait_prompt()
capture()
print(f'captured {session}:host -> temp/T5-{name}.ans')
# 保留宿主机 GDB 窗格供检查，结束本脚本创建的 QEMU，释放实验端口。
send(session + ':host', 'detach')
time.sleep(0.2)
tmux('kill-window', '-t', session + ':qemu')
