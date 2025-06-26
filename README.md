# aca25-spectre
This repository contains the code for the project of Advanced Computer Architectures (A.Y. 2025@Politecnico di Milano). 
## Intro  
The scope of the project is to verify whether **XiangShan** (an open source RISC-V processor) was vulnerable to speculative attacks such as **Spectre**.
In the results contained in the report, we found out that both XiangShan V2 (Nanhu) and  XiangShan V3 (Kunmighu) are vulnerable.
We tested Spectre-v1 attack with a Flush+Reload cache side channel on the following commits of the XiangShan repository:
- Nanhu: `nanhu/0a68ebc`.
- Kunmingu: `master/167da6a`.

Using the following `xs-env` commit: `d857953`
## Replicating the attacks
In the following we explain how to replicate the attack:
1. Setup the XiangShan simulation environment documented here [XiangShan Front-end Development Environment](https://docs.xiangshan.cc/zh-cn/latest/tools/xsenv-en/) 
    1. Make sure the XiangShan folder is at either of the commits specified above (`167da6a` for V3 or `0a68ebc` for V2).
    2. When performing the make emu command use:
        - `CONFIG=MinimalConfig` for V2
        - `CONFIG=KunminghuV2Config` for V3
      > Note: to improve simulation performance you can set `EMU_THREADS=num` to make the simulator run on `num` threads.
    3. Navigate to the `nexus-am` folder, edit the `Makefile.compile` file and remove:
        - `CC_OPT ?= -O2`
        - in `CFLAGS` remove `-Wall and -Werror`
    4. *Only if you want to use the version with CMO instructions*: navigate to `nexus-am/am/arch` and edit the `riscv64-xs.mk` file adding `_zicbom_zicboz` at the end of the `MARCH` list.
 2. Navigate to the `xs-env/nexus-am/apps/` folder.
 3. Clone this repository.
 4. Enter the cloned folder and go to the spectre-v1 directory.
 5. Run `make ARCH=riscv64-xs` to compile the workload.
 6. To run the workload use `path_to_xs-env/XiangShan/build/emu  --no-diff -i ./build/spectre-v1-riscv64-xs.bin`.
## Credits  
- [BOOM Speculative Attacks](https://github.com/riscv-boom/boom-attacks.git): spectre-v1 attack code for the BOOM processor, which we adapted to XiangShan.
- [PLRU-based eviction function](https://github.com/OpenXiangShan/XiangShan/issues/2534): other than the function used by the BOOM attacks and CMOs, we also tried the attack with this function.
