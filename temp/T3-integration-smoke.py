"""实现方冒烟：临时内核验证 SLUB 大对象按原页数释放 Buddy 整块。"""
from pathlib import Path
import shutil
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1] / 'code'
with tempfile.TemporaryDirectory(prefix='lyp-t3-integration-') as tmp:
    work = Path(tmp) / 'code'
    shutil.copytree(source, work, ignore=shutil.ignore_patterns('obj', 'bin'))
    init = work / 'kern/init/init.c'
    code = init.read_text()
    assert code.count('    slub_init();') == 1
    check = '''
    size_t before = nr_free_pages();
    void *p = kmalloc(9000), *q = kmalloc(20000);
    assert(p != NULL && q != NULL && p != q);
    assert(nr_free_pages() == before - 4 - 8);
    ((volatile char *)p)[0] = 0x12;
    ((volatile char *)p)[8999] = 0x34;
    ((volatile char *)q)[0] = 0x56;
    ((volatile char *)q)[19999] = 0x78;
    assert(((volatile char *)p)[0] == 0x12);
    assert(((volatile char *)p)[8999] == 0x34);
    assert(((volatile char *)q)[0] == 0x56);
    assert(((volatile char *)q)[19999] == 0x78);
    kfree(p);
    kfree(q);
    assert(nr_free_pages() == before);
    cprintf("[T3 integration] SLUB large requests/reclaim: PASS\\n");
'''
    init.write_text(code.replace('    slub_init();', '    slub_init();' + check))
    build = subprocess.run(['make', '-j4', 'PMM=buddy'], cwd=work,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if build.returncode:
        print(build.stdout)
        raise SystemExit(build.returncode)
    run = subprocess.run(['timeout', '4s', 'make', 'PMM=buddy', 'qemu'], cwd=work,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(run.stdout)
    assert run.returncode in (0, 124)
    assert 'panic' not in run.stdout.lower() and 'FAIL' not in run.stdout
    assert '[T3 integration] SLUB large requests/reclaim: PASS' in run.stdout
