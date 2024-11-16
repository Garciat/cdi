const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "cdi",
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibC();
    exe.addCSourceFiles(.{ .files = &[_][]const u8{
        "src/main.c",
    }, .flags = &[_][]const u8{
        "-std=c23",
        "-pedantic",
        "-Wall",
        "-W",
        "-Wno-missing-field-initializers",
    } });
    b.installArtifact(exe);

    const run_exe = b.addRunArtifact(exe);

    const run_step = b.step("run", "Run the application");
    run_step.dependOn(&run_exe.step);
}