import qbs

Project {
    minimumQbsVersion: "1.22.0"

    property bool pico_double: true
    property bool pico_float: true
    property bool pico_printf: true

    property stringList pico_sdk_defines: [
        "NDEBUG",
        "PICO_PANIC_FUNCTION=",
        "PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=0",
    ]

    references: [
        "pico-sdk.qbs",
    ]

    StaticLibrary {
        name: "bmp2"
        Depends {name: "pico-sdk"}

        cpp.includePaths: ["inc", "bmp2"]

        files: [
            "bmp2/bmp2_defs.h",
            "bmp2/bmp2.h",
            "bmp2/bmp2.c",
            "inc/sensor_bmp2.h",
            "src/sensor_bmp2.c",
        ]
    }

    StaticLibrary {
        name: "bme68x"
        Depends {name: "pico-sdk"}

        cpp.includePaths: ["inc", "bme68x"]

        files: [
            "bme68x/bme68x_defs.h",
            "bme68x/bme68x.h",
            "bme68x/bme68x.c",
            "inc/sensor_bme68x.h",
            "src/sensor_bme68x.c",
        ]
    }

    CppApplication {
        type: ["application", "hex", "size", "map"]
        Depends {name: "gcc-none"}
        Depends {name: "pico-sdk"}
        Depends {name: "bmp2"}
        Depends {name: "bme68x"}

        cpp.includePaths: ["inc"]

        files: [
            "inc/epd.h",
            "inc/epd_test.h",
            "src/epd.c",
            "src/epd_test.c",
            "src/epd_test_4in2.c",
            "src/epd_test_5in65.c",
            "src/epd_test_7in5.c",
            "src/epd_test_7in5_480p.c",
            "src/main.c",
        ]

        Group {     // Properties for the produced executable
            fileTagsFilter: ["application", "hex", "bin", "map"]
            qbs.install: true
        }
    }
}
