//! Generates the C++ shim that matches the bridge in `src/lib.rs`.
//!
//! Both are build artefacts of `#[cxx::bridge]`, so the two languages cannot
//! drift apart: changing the bridge regenerates the header and the shim.

fn main() {
    cxx_build::bridge("src/lib.rs").compile("vynx_qr_bridge");
    println!("cargo:rerun-if-changed=src/lib.rs");
}