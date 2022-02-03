import qbs

Project {
    minimumQbsVersion: "1.21.0"

    property bool pico_double: false
    property bool pico_float: false
    property bool pico_printf: false

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
