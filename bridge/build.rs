//! Generates the C++ shim that matches the bridge in `src/lib.rs`.
//!
//! Both are build artefacts of `#[cxx::bridge]`, so the two languages cannot
//! drift apart: changing the bridge regenerates the header and the shim.

fn main() {
    let mut build = cxx_build::bridge("src/lib.rs");
    // The shim this compiles is C++, and the C runtime it must agree with is
    // decided by the profile. Without this the shim is built with the release
    // CRT even in a debug build, and the linker refuses to mix it with the debug
    // objects Cargo produced alongside it.
    build.debug(cfg!(debug_assertions));
    build.compile("vynx_qr_bridge");
    println!("cargo:rerun-if-changed=src/lib.rs");
}