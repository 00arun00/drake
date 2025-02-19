# -*- mode: python -*-

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(
    "@drake//tools/workspace:os.bzl",
    "determine_os",
)

def _impl(repo_ctx):
    os_result = determine_os(repo_ctx)
    if os_result.error != None:
        fail(os_result.error)

    # Add the BUILD file -- same for both platforms.
    repo_ctx.symlink(
        Label("@drake//tools/workspace/doxygen:package.BUILD.bazel"),
        "BUILD.bazel",
    )

    if os_result.ubuntu_release == "20.04":
        # Download and extract Drake's pre-compiled Doxygen binary.
        archive = "doxygen-1.8.15-focal-x86_64.tar.gz"
        url = [
            pattern.format(archive = archive)
            for pattern in repo_ctx.attr.mirrors.get("doxygen")
        ]
        sha256 = "3c4886763ec27e1797b0fd5bfe576602f5e408649c0e282936e17bde5c7ed7e6"  # noqa
        repo_ctx.download_and_extract(
            url = url,
            sha256 = sha256,
            type = "tar.gz",
        )
    else:
        # On other platforms, documentation builds are not supported, so just
        # provide a dummy binary.
        repo_ctx.file("doxygen", "# Doxygen is not supported on this platform")

doxygen_repository = repository_rule(
    attrs = {
        "mirrors": attr.string_list_dict(),
    },
    implementation = _impl,
)
"""Provides a library target for @doxygen//:doxygen.  On macOS, uses homebrew;
on Ubuntu, downloads and extracts a precompiled binary release of Doxygen.
"""
