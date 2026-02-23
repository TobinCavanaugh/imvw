pub const packages = struct {
    pub const @"122043a4f3319c9ad13e992d79f36b070b17b4d596c27f5ed65f12d2c53fffd28078" = struct {
        pub const build_root = "C:\\Users\\tobin\\AppData\\Local\\zig\\p\\raylib-5.5.0-whq8uCM5NgRDpPMxnJrRPpktefNrBwsXtNWWwn9e1l8S";
        pub const build_zig = @import("122043a4f3319c9ad13e992d79f36b070b17b4d596c27f5ed65f12d2c53fffd28078");
        pub const deps: []const struct { []const u8, []const u8 } = &.{
            .{ "xcode_frameworks", "N-V-__8AABHMqAWYuRdIlflwi8gksPnlUMQBiSxAqQAAZFms" },
            .{ "emsdk", "N-V-__8AALRTBQDo_pUJ8IQ-XiIyYwDKQVwnr7-7o5kvPDGE" },
        };
    };
    pub const @"N-V-__8AABHMqAWYuRdIlflwi8gksPnlUMQBiSxAqQAAZFms" = struct {
        pub const available = false;
    };
    pub const @"N-V-__8AALRTBQDo_pUJ8IQ-XiIyYwDKQVwnr7-7o5kvPDGE" = struct {
        pub const available = false;
    };
};

pub const root_deps: []const struct { []const u8, []const u8 } = &.{
    .{ "raylib", "122043a4f3319c9ad13e992d79f36b070b17b4d596c27f5ed65f12d2c53fffd28078" },
};
