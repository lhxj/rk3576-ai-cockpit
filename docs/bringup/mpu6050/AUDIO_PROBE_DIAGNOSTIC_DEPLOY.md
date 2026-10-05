# audio-probe-d1：旧v3已安装，新v4兼容修正待替换

当前旧v3八件文件与新release模块已安装，Windows collector已发布；窗口实际root-p3 LOAD3696B/addr0x4c000000/filesize0xe70通过，SOURCE一次在首行echo之前返回prompt，未启动诊断kernel/M0业务。根第二次有限SSH确认默认6.1.99-rk3576/card0ES8388/no两KO/node/actors/8554恢复，首NoRoute保留，collector88328/lock6516退出0；旧v3文件/模块仍在，未撤回。音频原因UNKNOWN，errno-only内核不是修复，RTP pacing实板仍NOT_RUN。完整文件bytes/current与target SHA、原生和initrd独立root审查证据见 [部署manifest](AUDIO_PROBE_DIAGNOSTIC_DEPLOY.json)。

新release为`6.1.99-rk3576-audioprobe-d1`。Host actual initrd6603260B/SHA53810e61…636658，gzip-only/newc448成员；429非module成员全部13字段/name/content与stock相等，14module regularfiles为新pin modules.tar的实际bytes；唯一cdc_eem KO新vermagic。其余完整325成员新release模块必须安装到实体`/usr/lib/modules/6.1.99-rk3576-audioprobe-d1`（当前`/lib/modules` canonical `/usr/lib/modules`），switch_root后不可使用旧p026/stock模块兜底。matching headers与Module.symvers仅Host构建配套，不写运行期rootfs。

`/boot`可用12,574,720B放不下Image+initrd49,791,996B。新Image/initrd/原sensorDT以及stage.cmd/scr五件只放fresh `/home/cat/cockpit/audio-probe-diagnostic-d1/boot`，由mmc0:3/rootfs读取；不写/boot、不删默认或冻结文件、不扩分区。原FIT仍是mmc0:2/boot `/amp-p029/mpu-sensor-coexistence-v1/amp-signed.itb`，root实际SHA21ee9479…9bcb、143872B已核；它是immutable dependency，不复制安装、不撤回。两新KO只存userdir，不加载。

分区actual为p2 PARTNAME=boot/p3 PARTNAME=rootfs，rootfs4KiB block/64B group descriptor/256B inode。原U-Boot只读ext4代码支持指定分区、extent/filetype、64bit descriptor及inode_size，未发现实际features显式unsupported阻断；journal replay与checksum更新属write路径，未启EXT4_WRITE。此结论仅SOURCE_VERIFIED：metadata_csum并无只读完整验证保证，journal_checksum_v3不称回放PASS，在线needs_recovery不是失败证据。SOURCE前必须正常shutdown/Power down后冷动作，禁止dirtyroot上盲读、fsck/remount/replay或修文件系统。资源与features证据为`audio-probe-install-parent-inventory.log`、`audio-probe-rootfs-features-readonly.log`、`audio-probe-partition-identities.log`。

## 窗口失败与最小Host修正（根待审，无部署）

当前U-Boot启用CONFIG_AMP_PROJECT_FACTORY_BOOT。cmd/source.c的64–90行私有legacy验证函数要求IH_ARCH_PPC，73行架构守卫失败75行静默return1；128–129行legacy magic直接进入该函数。源码SHA6a8796f3…4c30。原成功脚本为(OS,arch,type,comp)=(5,7,6,0)，旧v3为(5,2,6,0)。之前我们仅核CRC/body/p3而遗漏factory archpolicy，这是Host封装审查遗漏；不能外推为Linux/音频/MPU故障。

Host新v4仅mkimage -A ppc/header arch7；stage3696B/e70，SHAcef5f058890512e41e37c741b0119baaf03508c7239c388bacf95fc7401375d0，与v3仅byte29/头CRC byte4–7变化，单component向量/正文及另八个包文件逐字节相同。编译actual U-Boot legacy函数fixture：实际v3 ret1/called0，实际v4 ret0/called1；OS/type/comp/CRC/loadedsize/vector负例均拒绝，证明policy入口而非脚本执行或实板成功。

默认已恢复之后，根可审只替换一个受控scr，其他七install文件、完整module树、冻结FIT/DT/loader保持。不要再次执行fresh mkdir/tar或上传packet/*。以下是明确的单文件更换草案，不执行：

```sh
# cat阶段：先拒绝fresh transfer文件存在，根只上传一个reviewed v4 scr。
test ! -e /home/cat/audio-probe-stage-v4.scr
test ! -L /home/cat/audio-probe-stage-v4.scr
# 根既有SFTP只传新scr到上述temporary path。
# sudo-root阶段：commonlock/default/no任务占用前置通过后执行。
set -eu
test "$(uname -r)" = 6.1.99-rk3576
test -f /home/cat/audio-probe-stage-v4.scr && test ! -L /home/cat/audio-probe-stage-v4.scr
test -f /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr && test ! -L /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr
printf '%s  %s\n' b12650983ef6c805e4eba9b2490027220c13e146684fed402418da4b51826f8d /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr | sha256sum -c -
printf '%s  %s\n' cef5f058890512e41e37c741b0119baaf03508c7239c388bacf95fc7401375d0 /home/cat/audio-probe-stage-v4.scr | sha256sum -c -
install -o root -g root -m 0644 -- /home/cat/audio-probe-stage-v4.scr /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr
printf '%s  %s\n' cef5f058890512e41e37c741b0119baaf03508c7239c388bacf95fc7401375d0 /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr | sha256sum -c -
rm -- /home/cat/audio-probe-stage-v4.scr
```

若已实际换成v4后要仅撤销header更换，根另cat传原v3包scr到fresh `/home/cat/audio-probe-stage-v3-rollback.scr`（拒存在/链接），再sudo-root持锁/默认/no占用核以下精确逆向方案；目前未执行：

```sh
set -eu
test "$(uname -r)" = 6.1.99-rk3576
test -f /home/cat/audio-probe-stage-v3-rollback.scr && test ! -L /home/cat/audio-probe-stage-v3-rollback.scr
test -f /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr && test ! -L /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr
printf '%s  %s\n' cef5f058890512e41e37c741b0119baaf03508c7239c388bacf95fc7401375d0 /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr | sha256sum -c -
printf '%s  %s\n' b12650983ef6c805e4eba9b2490027220c13e146684fed402418da4b51826f8d /home/cat/audio-probe-stage-v3-rollback.scr | sha256sum -c -
install -o root -g root -m 0644 -- /home/cat/audio-probe-stage-v3-rollback.scr /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr
printf '%s  %s\n' b12650983ef6c805e4eba9b2490027220c13e146684fed402418da4b51826f8d /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr | sha256sum -c -
rm -- /home/cat/audio-probe-stage-v3-rollback.scr
```

这是恢复旧已登记scr，不允许再次SOURCE旧v3；完整撤回仍见后方，仅默认恢复后执行。正常replacement与其rollback均只同一个explicit scr，不覆盖其他文件/冻结组合。

根读回3696/e70、arch7/CRC及正文后才更新安装receipt；失败不启动。临时文件是独立精确transfer path，不属正式八文件manifest且成功即删，不增加userdir成员。旧v3/窗口raw日志在原目录保留。若选择撤回旧组合而非单文件替换，后方完整撤回门只允许已登记old-v3或new-v4这两个scr SHA。

## fresh完整安装命令（现有旧v3必须先受审撤回；不可直接重跑）

以下只供根审批后执行，子代理没有运行这些命令。前置：独占commonlock、实际默认uname6.1.99-rk3576/no任务KO/node/actor，三fresh路径及dangling symlink均拒绝覆盖；当前ABSENT是只读快照，安装前必须重新核。cat在根核前置后创建fresh userdir/boot，并将packet-v4八个`install=true`文件传到manifest精确target；不要传入或安装packet内FIT副本。传输可采用根既有工具，传后必须以下actual hash全部通过才继续。

```sh
set -eu
test "$(uname -r)" = 6.1.99-rk3576
test "$(readlink -f /lib/modules)" = /usr/lib/modules
test ! -e /home/cat/cockpit/audio-probe-diagnostic-d1
test ! -L /home/cat/cockpit/audio-probe-diagnostic-d1
test ! -e /usr/lib/modules/6.1.99-rk3576-audioprobe-d1
test ! -L /usr/lib/modules/6.1.99-rk3576-audioprobe-d1
# cat SSH/SFTP阶段（无隐含root SSH）：先fresh创建两目录。
mkdir -- /home/cat/cockpit/audio-probe-diagnostic-d1
mkdir -- /home/cat/cockpit/audio-probe-diagnostic-d1/boot
# cat仅传入manifest精确八件target：五件boot/* + 两KO + modules.tar，非packet/*通配。
# 随后sudo-root阶段，根既有受审有限入口执行；不是cat直接运行root安装。
cd /home/cat/cockpit/audio-probe-diagnostic-d1
sha256sum -c - <<'PINS'
8c7494558f5bfb0d9cf7dad48e1d284486409486b4c067deb1fe160165a1d606  boot/Image
53810e61d7679d71f14dc663647e6f8d8eb993754862d5e7d0932bd9fd636658  boot/initrd
11f79aab2bf799f7ea4e0ffb2a08c075a53c2b955516957dbd96a7a89f08d0fc  modules.tar
d68be42c6643718db22d7b733b0bc56100a189c577ca2ba253636d0128290829  rk3576_amp_health_test.ko
1df92d99e7f437b388b52a07cf4a0dba70fdd285688544690cc654d8d93a869f  rk3576_sensor.ko
33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e  boot/sensor.dtb
4e14f52c7cb791804f46f7972edd01833c0af00bd851c2e2f5ef896d3b3ce40d  boot/stage-audio.cmd
cef5f058890512e41e37c741b0119baaf03508c7239c388bacf95fc7401375d0  boot/stage-audio.scr
PINS
# eight exact targets are ordinary files; reject links before chmod/chown.
test "$(pwd -P)" = /home/cat/cockpit/audio-probe-diagnostic-d1
test -d boot && test ! -L boot
for name in boot/Image boot/initrd boot/sensor.dtb boot/stage-audio.cmd boot/stage-audio.scr rk3576_amp_health_test.ko rk3576_sensor.ko modules.tar; do
  test -f "$name" && test ! -L "$name"
done
chown root:root -- . boot boot/Image boot/initrd boot/sensor.dtb boot/stage-audio.cmd boot/stage-audio.scr rk3576_amp_health_test.ko rk3576_sensor.ko modules.tar
chmod 0755 -- . boot
chmod 0644 -- boot/Image boot/initrd boot/sensor.dtb boot/stage-audio.cmd boot/stage-audio.scr rk3576_amp_health_test.ko rk3576_sensor.ko modules.tar
# whole archive hash/325 safe members already reviewed; recheck exact newrelease absence/canonical.
test "$(readlink -f /lib/modules)" = /usr/lib/modules
test ! -e /usr/lib/modules/6.1.99-rk3576-audioprobe-d1
test ! -L /usr/lib/modules/6.1.99-rk3576-audioprobe-d1
tar --strip-components=2 -C /usr/lib/modules -xpf modules.tar
tar --strip-components=2 -C /usr/lib/modules --compare -f modules.tar
# 根按manifest再读回五个boot文件/twoKO/full modules与所有metadata。
```

任何hash、archive comparison、权限、空间、fresh/identity门失败立即停止，不执行SOURCE。以上cat创建/传输与sudo-root权限阶段明确分开；没有flat Image副本或module-stage scratch，userdir仅八文件和boot目录。完整module archive是新native产物，非改名旧模块；绝不执行depmod旧release或自动modprobe。/boot无需新建诊断目录。安装新module directory无startup服务变更。

## 一次诊断启动

审核collector源码`scripts/board/mpu6050/capture_audio_probe_dual_uart_v2.ps1`，SHA4ee847b6…185b9、4754B。独立于旧manual6和原collector；后者保持SHA95f55bef…e9e35，不得发送旧LOAD。collector原hash/path/hex/caps均不变；已发布并实际单次使用，当前不再启动。以下仅后续根审核并正常关机冷动作后的入口记录：

```powershell
powershell.exe -NoProfile -File C:/Users/27432/Documents/MPU6050-AudioProbe-d1/capture_audio_probe_dual_uart_v2.ps1 -LinuxLog C:/Users/27432/Documents/MPU6050-AudioProbe-d1/linux-com5.log -M0Log C:/Users/27432/Documents/MPU6050-AudioProbe-d1/m0-com6.log -ControlFile C:/Users/27432/Documents/MPU6050-AudioProbe-d1/control.txt -InterruptColdBoot
```

根实际cold/精确U-Boot身份/prompt后人工依次写LOAD、INSPECT，读回身份/文件结果再写SOURCE一次：

```text
LOAD:    load mmc 0:3 0x4c000000 /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr
INSPECT: printenv fileaddr filesize
EXPECTED: fileaddr=4c000000 filesize=e70（3696B）
SOURCE:  source 0x4c000000
```

stage保留boot=2 guard，新增rootfs=3 guard；Linux Image/initrd/DT六条size/load实参为0:3，FIT size与amp_m0load保持原p2路径。loader ABI固定named boot，无partition参数；不改U-Boot或M0loader。RAM Image0x40400000/initrd0x4a200000/DT0x48300000、AMP签名/一次尝试、所有size/shortread/bootargs root及boot_part=2保持，仅新增audio_probe标识。无法读p3、任一size不符或身份失败即停，不SOURCE、不重试。

SOURCE后180s、总600s、每UART256KiB，cold120s/人工初始等待300s保持；cap/串口错误停止并保留原日志。窗口只取初次codec errno、uname/cmdline/cards/driver/deferred信息，无两KO/HELLO/MPU/Qt/coex/PA setter。负errno/缺卡/风暴/超时保存后停止，不reprobe/clock修改或第二次同组合cold。M0启动文字不称health/sensorPASS。

## 结束与撤回

根正常shutdown并取得UART Power down，CANCEL关闭双UART/释放锁；用户断主电10s默认冷恢复，根有限实读uname6.1.99-rk3576/no任务KO/node/cards，无额外35hash重复。未确认默认时不可移除正在运行release。

默认身份、独占锁、无任务占用确认后，撤回先验证原八件userdir hashes（上方同一sha256sum命令）、五boot文件actual hash与manifest相等，以及module archive strip2与实体/usr/lib/modules逐项相等。目录若出现未登记内容或被替换即停供根审，不做recursive删除。精确已知目录删除命令仅在这些检查全部通过后执行：

```sh
set -eu
test "$(uname -r)" = 6.1.99-rk3576
test "$(readlink -f /lib/modules)" = /usr/lib/modules
cd /home/cat/cockpit/audio-probe-diagnostic-d1
tar --strip-components=2 -C /usr/lib/modules --compare -f modules.tar
# 对新module树的成员集合逐项拒绝额外内容（metadata/bytes已经compare）。
tar -tf modules.tar | sed 's#^lib/modules/#/usr/lib/modules/#;s#/$##' | sort > expected-module-paths.txt
find /usr/lib/modules/6.1.99-rk3576-audioprobe-d1 -print | sort > actual-module-paths.txt
cmp -- expected-module-paths.txt actual-module-paths.txt
rm -- expected-module-paths.txt actual-module-paths.txt
# 有界只读核userdir全体对象；未知文件/链接/改名或hash不符停止。
python3 - <<'VERIFY'
import hashlib,os,pathlib,stat,tarfile,time
root=pathlib.Path('/home/cat/cockpit/audio-probe-diagnostic-d1')
assert stat.S_ISDIR(root.lstat().st_mode) and root.resolve()==root
pins={'boot/initrd': (6603260, '53810e61d7679d71f14dc663647e6f8d8eb993754862d5e7d0932bd9fd636658'), 'boot/Image': (43188736, '8c7494558f5bfb0d9cf7dad48e1d284486409486b4c067deb1fe160165a1d606'), 'boot/sensor.dtb': (308361, '33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e'), 'boot/stage-audio.cmd': (3624, '4e14f52c7cb791804f46f7972edd01833c0af00bd851c2e2f5ef896d3b3ce40d'), 'modules.tar': (74444800, '11f79aab2bf799f7ea4e0ffb2a08c075a53c2b955516957dbd96a7a89f08d0fc'), 'boot/stage-audio.scr': (3696, 'cef5f058890512e41e37c741b0119baaf03508c7239c388bacf95fc7401375d0'), 'rk3576_amp_health_test.ko': (89600, 'd68be42c6643718db22d7b733b0bc56100a189c577ca2ba253636d0128290829'), 'rk3576_sensor.ko': (143824, '1df92d99e7f437b388b52a07cf4a0dba70fdd285688544690cc654d8d93a869f')}
deadline=time.monotonic()+60
expected=set(pins)|{'boot'}
for directory in [root,root/'boot']:
    st=directory.lstat()
    assert stat.S_ISDIR(st.st_mode) and stat.S_IMODE(st.st_mode)==0o755 and st.st_uid==0 and st.st_gid==0
for name,(size,sha) in pins.items():
    path=root/name;st=path.lstat()
    assert stat.S_ISREG(st.st_mode) and st.st_size==size and stat.S_IMODE(st.st_mode)==0o644 and st.st_uid==0 and st.st_gid==0
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        while True:
            assert time.monotonic()<deadline
            block=stream.read(1048576)
            if not block:break
            digest.update(block)
    allowed={sha}
    if name=='boot/stage-audio.scr':allowed.add('b12650983ef6c805e4eba9b2490027220c13e146684fed402418da4b51826f8d')
    assert digest.hexdigest() in allowed
actual=set()
for current,dirs,files in os.walk(root,followlinks=False):
    assert time.monotonic()<deadline
    for name in dirs+files:
        path=pathlib.Path(current)/name
        assert not path.is_symlink()
        actual.add(str(path.relative_to(root)))
assert actual==expected,(actual-expected,expected-actual)
print('EXACT_FRESH_USERDIR_VERIFIED')
VERIFY
cd /
rm -r -- /usr/lib/modules/6.1.99-rk3576-audioprobe-d1
rm -r -- /home/cat/cockpit/audio-probe-diagnostic-d1
test ! -e /usr/lib/modules/6.1.99-rk3576-audioprobe-d1
test ! -e /home/cat/cockpit/audio-probe-diagnostic-d1
```

绝不删除或改写原FIT/旧release/defaultboot。新v4替换/诊断内核启动/撤回仍NOT_RUN；旧v3安装及p3读取实际已执行；现有目录ABSENT及工具SOURCE_VERIFIED不替代后续实际读回/停止/恢复证据。

旧packet-v1因boot空间不足停止；packet-v2因内部Linux argv仍指p2被根review拒绝，已保留原包和stopped collector作为证据，不可安装/运行。v3 production parser核实named guard/七条argv，并加入旧v2负例；历史只作审查记录。
