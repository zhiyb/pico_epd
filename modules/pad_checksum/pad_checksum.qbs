import qbs

Module {
    Rule {
        id: pad_checksum
        inputs: ["bin"]
        Artifact {
            fileTags: ["pad_checksum"]
            filePath: input.fileName + ".S"
        }

        prepare: {
            var args = ["pico-sdk/src/rp2_common/boot_stage2/pad_checksum",
                        "-s", "0xffffffff", input.filePath, output.filePath];
            var cmd = new Command("python3", args);
            cmd.description = "generating " + output.fileName;
            return cmd;
        }
    }
}
