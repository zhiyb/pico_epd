import qbs

Project {
    property stringList common: [
        "pico_base",
        "pico_binary_info",
        "pico_stdlib",
        "pico_divider",
        "pico_time",
        "pico_util",
        "pico_sync",
    ]

    property stringList rp2_common: [
        "hardware_adc",
        "hardware_base",
        "hardware_claim",
        "hardware_clocks",
        "hardware_divider",
        "hardware_dma",
        "hardware_exception",
        "hardware_flash",
        "hardware_gpio",
        "hardware_i2c",
        "hardware_interp",
        "hardware_irq",
        "hardware_pio",
        "hardware_pll",
        "hardware_pwm",
        "hardware_resets",
        "hardware_rtc",
        "hardware_spi",
        "hardware_sync",
        "hardware_timer",
        "hardware_uart",
        "hardware_vreg",
        "hardware_watchdog",
        "hardware_xosc",
        "pico_standard_link",
        "pico_platform",
        "pico_stdio",
        "pico_malloc",
        "pico_runtime",
        "pico_bootrom",
        "pico_mem_ops",
        "pico_multicore",
        "pico_bit_ops",
        "pico_int64_ops",
        "pico_divider",
    ]

    Product {
        name: "boot_stage2"
        type: ["boot_stage2_bin"]
        Depends {name: "cpp"}
        Depends {name: "gcc-none"}
        Depends {name: "pico-sdk-inc"}

        cpp.driverLinkerFlags: ["--specs=nosys.specs", "-nostartfiles"]

        cpp.includePaths: [
            "pico-sdk/src/rp2_common/boot_stage2/asminclude",
            "pico-sdk/src/rp2_common/boot_stage2/include",
        ]

        files: [
            "pico-sdk/src/rp2_common/boot_stage2/boot_stage2.ld",
            "pico-sdk/src/rp2_common/boot_stage2/compile_time_choice.S",
        ]

        Group {
            name: "implementations"
            fileTags: []
            files: [
                "pico-sdk/src/rp2_common/boot_stage2/boot2_*.S",
            ]
        }

        Group {     // Properties for the produced binary
            fileTagsFilter: ["bin"]
            fileTags: ["boot_stage2_bin"]
        }
    }

    StaticLibrary {
        name: "pico-sdk"
        Depends {name: "pico-sdk-inc"}
        Depends {name: "boot_stage2"}

        Rule {
            multiplex: false
            inputsFromDependencies: ["boot_stage2_bin"]

            Artifact {
                fileTags: ["asm_cpp"]
                filePath: input.baseName + ".S"
            }

            prepare: {
                var args = ["pico-sdk/src/rp2_common/boot_stage2/pad_checksum",
                            "-s", "0xffffffff", input.filePath, output.filePath];
                var cmd = new Command("python3", args);
                cmd.description = "generating " + output.fileName;
                return cmd;
            }
        }

        property stringList pico_wrap_function: [
            // pico_bit_ops_pico
            "__clzsi2", "__clzsi2", "__clzdi2", "__ctzsi2", "__ctzdi2", "__popcountsi2", "__popcountdi2", "__clz", "__clzl", "__clzsi2", "__clzll",
            // pico_divider_hardware
            "__aeabi_idiv", "__aeabi_idivmod", "__aeabi_ldivmod", "__aeabi_uidiv", "__aeabi_uidivmod", "__aeabi_uldivmod",
            // pico_double
            "__aeabi_dadd", "__aeabi_ddiv", "__aeabi_dmul", "__aeabi_drsub", "__aeabi_dsub",
            "__aeabi_cdcmpeq", "__aeabi_cdrcmple", "__aeabi_cdcmple",
            "__aeabi_dcmpeq", "__aeabi_dcmplt", "__aeabi_dcmple", "__aeabi_dcmpge", "__aeabi_dcmpgt", "__aeabi_dcmpun",
            "__aeabi_i2d", "__aeabi_l2d", "__aeabi_ui2d", "__aeabi_ul2d",
            "__aeabi_d2iz", "__aeabi_d2lz", "__aeabi_d2uiz", "__aeabi_d2ulz", "__aeabi_d2f",
            "sqrt", "cos", "sin", "tan", "atan2", "exp", "log", "ldexp", "copysign", "trunc", "floor", "ceil", "round",
            "sincos" /*gnu*/, "asin", "acos", "atan", "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
            "exp2", "log2", "exp10", "log10", "pow", "powint" /*gnu*/, "hypot", "cbrt", "fmod",
            "drem", "remainder", "remquo", "expm1", "log1p", "fma",
            // pico_float
            "__aeabi_fadd", "__aeabi_fdiv", "__aeabi_fmul", "__aeabi_frsub", "__aeabi_fsub",
            "__aeabi_cfcmpeq", "__aeabi_cfrcmple", "__aeabi_cfcmple",
            "__aeabi_fcmpeq", "__aeabi_fcmplt", "__aeabi_fcmple", "__aeabi_fcmpge", "__aeabi_fcmpgt", "__aeabi_fcmpun",
            "__aeabi_i2f", "__aeabi_l2f", "__aeabi_ui2f", "__aeabi_ul2f",
            "__aeabi_f2iz", "__aeabi_f2lz", "__aeabi_f2uiz", "__aeabi_f2ulz", "__aeabi_f2d",
            "sqrtf", "cosf", "sinf", "tanf", "atan2f", "expf", "logf", "ldexpf", "copysignf", "truncf", "floorf", "ceilf", "roundf",
            "sincosf" /*gnu*/, "asinf", "acosf", "atanf", "sinhf", "coshf", "tanhf", "asinhf", "acoshf", "atanhf",
            "exp2f", "log2f", "exp10f", "log10f", "powf", "powintf" /*gnu*/, "hypotf", "cbrtf", "fmodf",
            "dremf", "remainderf", "remquof", "expm1f", "log1pf", "fmaf",
            // pico_int64_ops_pico
            "__aeabi_lmul",
            // pico_malloc
            "malloc", "calloc", "free",
            // pico_mem_ops_pico
            "memcpy", "memset", "__aeabi_memcpy", "__aeabi_memset", "__aeabi_memcpy4", "__aeabi_memset4", "__aeabi_memcpy8", "__aeabi_memset8",
            // pico_printf
            "sprintf", "snprintf", "vsnprintf",
            // pico_stdio
            "printf", "vprintf", "puts", "putchar", "getchar",
        ]

        Group {
            name: "Double support"
            condition: project.pico_double
            files: [
                "pico-sdk/src/rp2_common/pico_double/double_aeabi.S",
                "pico-sdk/src/rp2_common/pico_double/double_init_rom.c",
                "pico-sdk/src/rp2_common/pico_double/double_math.c",
                "pico-sdk/src/rp2_common/pico_double/double_v1_rom_shim.S",
            ]
        }

        Group {
            name: "No double support"
            condition: !project.pico_double
            files: [
                "pico-sdk/src/rp2_common/pico_double/double_none.S",
            ]
        }

        Group {
            name: "Float support"
            condition: project.pico_float
            files: [
                "pico-sdk/src/rp2_common/pico_float/float_aeabi.S",
                "pico-sdk/src/rp2_common/pico_float/float_init_rom.c",
                "pico-sdk/src/rp2_common/pico_float/float_math.c",
                "pico-sdk/src/rp2_common/pico_float/float_v1_rom_shim.S",
            ]
        }

        Group {
            name: "No float support"
            condition: !project.pico_float
            files: [
                "pico-sdk/src/rp2_common/pico_float/float_none.S",
            ]
        }

        Group {
            name: "printf support"
            condition: project.pico_printf
            files: [
                "pico-sdk/src/rp2_common/pico_printf/printf.c",
            ]
        }

        Group {
            name: "No printf support"
            condition: !project.pico_printf
            files: [
                "pico-sdk/src/rp2_common/pico_printf/printf_none.S",
            ]
        }

        files: []
         .concat(project.common.map(function (s) {return "pico-sdk/src/common/" + s + "/*.c";}))
         .concat(project.common.map(function (s) {return "pico-sdk/src/common/" + s + "/*.S";}))
         .concat(project.rp2_common.map(function (s) {return "pico-sdk/src/rp2_common/" + s + "/*.c";}))
         .concat(project.rp2_common.map(function (s) {return "pico-sdk/src/rp2_common/" + s + "/*.S";}))

        Export {
            Depends {name: "cpp"}
            Depends {name: "pico-sdk-inc"}

            cpp.linkerFlags: [
                "--gc-sections", "--build-id=none"
            ].concat(product.pico_wrap_function.map(function (f) {return "--wrap=" + f;}))
            //cpp.driverLinkerFlags: ["-nostdlib"]
            //cpp.driverLinkerFlags: ["--specs=nano.specs"]
            //cpp.driverLinkerFlags: ["--specs=nosys.specs"]
            cpp.driverLinkerFlags: ["--specs=nosys.specs", "-nostartfiles"]

            Parameters {
                cpp.linkWholeArchive: true
            }

            Group {
                name: "Linker script"
                files: [
                    "pico-sdk/src/rp2_common/pico_standard_link/memmap_default.ld",
                ]
            }
        }
    }

    Product {
        name: "pico-sdk-inc"
        Depends {name: "cpp"}

        property pathList includePaths: [
            //"pico-sdk/src/rp2_common/cmsis/stub/CMSIS/Core/Include",
            "pico-sdk/src/rp2_common/boot_stage2/include",
            "pico-sdk/src/rp2_common/pico_double/include",
            "pico-sdk/src/rp2_common/pico_float/include",
            "pico-sdk/src/rp2_common/pico_printf/include",

            "pico-sdk/src/rp2040/hardware_structs/include",
            "pico-sdk/src/rp2040/hardware_regs/include",

            "pico-sdk/src/boards/include",

            // Replacements for auto-generated files
            "pico-sdk-inc",
        ].concat(project.common.map(function (s) {return "pico-sdk/src/common/" + s + "/include";}))
         .concat(project.rp2_common.map(function (s) {return "pico-sdk/src/rp2_common/" + s + "/include";}))

        cpp.includePaths: includePaths

        property stringList defines: [
            "PICO_NO_HARDWARE=0",
            "PICO_ON_DEVICE=1",
        ]

        cpp.defines: defines

        files: [
            //"pico-sdk/src/rp2_common/cmsis/stub/CMSIS/Core/Include/**/*.h",
            "pico-sdk/src/rp2_common/boot_stage2/include/**/*.h",
            "pico-sdk/src/rp2_common/pico_double/include/**/*.h",
            "pico-sdk/src/rp2_common/pico_float/include/**/*.h",
            "pico-sdk/src/rp2_common/pico_printf/include/**/*.h",

            "pico-sdk/src/rp2040/hardware_structs/include/**/*.h",
            "pico-sdk/src/rp2040/hardware_regs/include/**/*.h",

            "pico-sdk/src/boards/include/**/*.h",

            "pico-sdk-inc/**/*.h",
        ].concat(project.common.map(function (s) {return "pico-sdk/src/common/" + s + "/include/**/*.h";}))
         .concat(project.rp2_common.map(function (s) {return "pico-sdk/src/rp2_common/" + s + "/include/**/*.h";}))

        Export {
            Depends {name: "cpp"}

            // Compiler flags from pico-sdk/cmake/preload/toolchains/pico_arm_gcc.cmake
            cpp.driverFlags: [
                "-mcpu=cortex-m0plus", "-mthumb",
                "-mfloat-abi=soft",
            ]

            cpp.commonCompilerFlags: [
                "-fdata-sections",
                "-fno-common", "-fno-strict-aliasing",
                "-ffast-math",
            ]
            cpp.positionIndependentCode: false

            cpp.includePaths: product.includePaths
            cpp.defines: product.defines
        }
    }
}
