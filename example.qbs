import qbs

Project {
    minimumQbsVersion: "1.21.0"

    property bool pico_double: false
    property bool pico_float: false
    property bool pico_printf: false

    property stringList pico_sdk_defines: [
        "NDEBUG",
        "PICO_PANIC_FUNCTION=",
        "PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1",
    ]

    references: [
        "pico-sdk.qbs",
    ]

    CppApplication {
        name: "example"
        type: ["application", "hex", "size", "map"]
        Depends {name: "gcc-none"}
        Depends {name: "pico-sdk"}

        files: [
            "src/main.c",
        ]

        Group {     // Properties for the produced executable
            fileTagsFilter: ["application", "hex", "bin", "map"]
            qbs.install: true
        }
    }
}
