# The DECADES Compiler (DEC++)

## QuickStart 

Thid pacakge depends on the following packages:

* CMake 3.20 or higher
* CLANG V11.1.0 
* CLANG's developer package V11.1.0
* LLVM V11.1.0 

For example, on a Ubuntu platform, the following line:

```console
sudo apt install cmake clang-11 libclang-11-dev
````

will install all the needed packages. 


Once you have successfully installed/acquired the right versions of the dependecies, run the following:

```console
    mkdir build
    cd build
    cmake ../
    make
```

Alternatively, we provide a docker image for this package. It can be obtained by running:

```console
docker pull sn3332/decades
```




Running DEC++
-----

DEC++ can be invoked from the command line with the following possible arguments:

```console
    DEC++ -m MODE [-t] [NUM_THREADS] [-sps] [SCRATCHPAD_SIZE] [-s] [BISCUIT_SYNC_MODE] [-spp] [SIMULATOR_PREPROCESSING_SCRIPT] [--target] [TARGET] $FILE [$ARGS]
```

`MODE` must be one of the following:

- Native ("n"): simply compiles the source files with no LLVM transformations or passes (useful for verification of correct program output and debugging, as well as application development outside of any DECADES features)

```console
        DEC++ -m n $FILE [$ARGS]
```

No additional arguments are necessary.

- DECADES Base ("db"): the default DEC++ mode that identifies the `kernel` function, performs function inlining and wraps the function invocation in the tile launcher

```console
        DEC++ -m db [-t] [NUM_THREADS] [--target] [TARGET] $FILE [$ARGS]
```
    where `NUM_THREADS` is the number of threads to utilize in parallel and `TARGET` can be either "x86" (default), "simulator" (https://github.com/PrincetonUniversity/pythia), or "riscv64" (generate a RISC-V binary to run on the DECADES FPGA emulation and chip platform). If the target is "simulator", then the path to the simulator preprocessing script is necessary:

```console
        DEC++ -m db [-t] [NUM_THREADS] [-spp] [SIMULATOR_PREPROCESSING_SCRIPT] [--target] [TARGET] $FILE [$ARGS]
```

- Decoupled Implicit ("di"): the decoupling compilation mode that slices the `kernel` function into supply and compute programs completely automatically

```console
        DEC++ -m di [-t] [NUM_THREADS] [--target] [TARGET] $FILE [$ARGS]
```

    where `NUM_THREADS` is the number of threads to utilize in parallel and `TARGET` can be either "x86" (default) or "simulator".
 - Decoupled DeSC ("di"): the decoupling compilation mode that slices the `kernel` function into supply and compute programs semi-automatically, where the variable indirection operations should be annotated explicitely by the user using the compute_exclusive functions:  

```console
        DEC++ -m d [-t] [NUM_THREADS] [--target] [TARGET] $FILE [$ARGS]
```

    where `NUM_THREADS` is the number of threads to utilize in parallel and `TARGET` can be either "x86" (default) or "simulator".
 

## License

  [BSD License (BSD 2-Clause License)](BSD-License.txt)
