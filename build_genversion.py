# 每次编译前自动生成 src/FwVersion.h：
#   优先取 git 短 SHA + UTC 日期（与 CI / web/version.txt 同源），
#   无 git 时回退为 "local + 构建日期"。
# CI 也会写入该头文件，这里（extra_scripts pre）在 env 构造阶段执行，
#   早于任何源码编译，保证本地/CI 版本号一致、可比对。
Import("env")
from os.path import join
import os, subprocess, datetime

root = os.getcwd()   # pio run 在工程根目录执行
sha = ""
when = ""
try:
    sha = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=root, stderr=subprocess.DEVNULL,
    ).decode().strip()
    # 取该提交的提交时间（分钟级），支持同一天多版本按时间序判断新旧
    when = subprocess.check_output(
        ["git", "log", "-1", "--format=%cd", "--date=format:%Y-%m-%dT%H:%M"],
        cwd=root, stderr=subprocess.DEVNULL,
    ).decode().strip()
except Exception:
    pass
if not when:
    when = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M")   # 无 git 时退回构建时间
head = (str(sha) + " " + when) if sha else "local " + when
with open(join(root, "src", "FwVersion.h"), "w") as f:
    f.write('#pragma once\n#define FW_VERSION "%s"\n' % head)