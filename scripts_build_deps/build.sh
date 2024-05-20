SCRIPT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

export ROOT_DIR=`pwd`

export CROSSTOOL_INSTALL_DIR=${ROOT_DIR}/sysroot_ct
export CROSSTOOL_CONFIG_FILE=${SCRIPT_ROOT}/crosstool-riscv64-unknown-linux-gnu
export TOOLCHAIN_INSTALL_ROOT=${ROOT_DIR}/sysroot_riscv
export RISCV_TOOLCHAIN_DIR=${TOOLCHAIN_INSTALL_ROOT}/riscv64-unknown-linux-gnu
export LLVM_INSTALL_DIR=${ROOT_DIR}/sysroot_llvm
export OPENMP_INSTALL_DIR=${ROOT_DIR}/sysroot_openmp
export LLVM_SRC_DIR=${ROOT_DIR}/llvm-project

# Init
cd ${ROOT_DIR}
echo "Initial directory: ${ROOT_DIR}"
##
## Installing crosstool-ng
##
git clone https://github.com/crosstool-ng/crosstool-ng
cd crosstool-ng
./bootstrap
./configure --prefix=${CROSSTOOL_INSTALL_DIR}
make install
cd ${ROOT_DIR}

##
## Build gcc toolchain and riscv system root
##
TMP_DIR=${ROOT_DIR}/tmp_dir
mkdir ${TMP_DIR}
cd ${TMP_DIR}
cp ${CROSSTOOL_CONFIG_FILE} ${TMP_DIR}/.config
export CT_PREFIX=${TOOLCHAIN_INSTALL_ROOT} # The toolchain will be installed in ${CT_PREFIX}/riscv64-unknown-linux-gnu
export WORKSPACE=${TMP_DIR}
export CT_BUILD_DIR=${TMP_DIR}
${CROSSTOOL_INSTALL_DIR}/bin/ct-ng upgradeconfig
${CROSSTOOL_INSTALL_DIR}/bin/ct-ng build
cd ${ROOT_DIR}

##
## Build LLVM
##
git clone https://github.com/llvm/llvm-project.git
mkdir build_llvm
cd build_llvm
cmake \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_TARGETS_TO_BUILD="RISCV" \
    -DCMAKE_INSTALL_PREFIX=${LLVM_INSTALL_DIR} \
    ${LLVM_SRC_DIR}/llvm

make
make install
cd ${ROOT_DIR}

##
## Build OpenMP
##
mkdir build_openmp
cd build_openmp

set -x

cmake --debug -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CROSSCOMPILING=True \
      -DOPENMP_ENABLE_LIBOMPTARGET=False \
      -DLIBOMP_ENABLE_SHARED=off \
      -DLIBOMP_ARCH=riscv64 \
      -DCMAKE_C_COMPILER=${LLVM_INSTALL_DIR}/bin/clang \
      -DCMAKE_C_FLAGS="--target=riscv64-unknown-linux -march=rv64gc -mabi=lp64d --sysroot=${RISCV_TOOLCHAIN_DIR}/riscv64-unknown-linux-gnu/sysroot --gcc-toolchain=${RISCV_TOOLCHAIN_DIR}" \
      -DCMAKE_CXX_COMPILER=${LLVM_INSTALL_DIR}/bin/clang++ \
      -DCMAKE_CXX_FLAGS="--target=riscv64-unknown-linux -march=rv64gc -mabi=lp64d --sysroot=${RISCV_TOOLCHAIN_DIR}/riscv64-unknown-linux-gnu/sysroot --gcc-toolchain=${RISCV_TOOLCHAIN_DIR}" \
      -DLIBOMP_OMPT_SUPPORT=TRUE \
      -DCMAKE_INSTALL_PREFIX=${OPENMP_INSTALL_DIR} \
      ${LLVM_SRC_DIR}/openmp
make
make install

cd ${ROOT_DIR}
