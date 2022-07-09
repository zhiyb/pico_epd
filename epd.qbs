import qbs

Project {
    minimumQbsVersion: "1.21.0"

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
        name: "adc"
        Depends {name: "pico-sdk"}

        cpp.includePaths: ["inc"]

        files: [
            "inc/adc.h",
            "src/adc.c",
        ]
    }

    CppApplication {
        type: ["application", "hex", "size", "map"]
        Depends {name: "gcc-none"}
        Depends {name: "pico-sdk"}
        Depends {name: "adc"}

        cpp.includePaths: ["inc"]

        files: [
            "src/main.c",
        ]

        Group {     // Properties for the produced executable
            fileTagsFilter: ["application", "hex", "bin", "map"]
            qbs.install: true
        }
    }
}
