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
try:
    sha = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=root, stderr=subprocess.DEVNULL,
    ).decode().strip()
except Exception:
    sha = ""
date = datetime.datetime.utcnow().strftime("%Y-%m-%d")
head = str(sha) + " " + date if sha else "local " + date
with open(join(root, "src", "FwVersion.h"), "w") as f:
    f.write('#pragma once\n#define FW_VERSION "%s"\n' % head)