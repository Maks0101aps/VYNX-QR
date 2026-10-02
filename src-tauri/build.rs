fn main() {
    tauri_build::build();

    #[cfg(target_os = "windows")]
    embed_windows_resources_if_missing();
}

/// Compensate for a resource compiler that silently produced nothing.
///
/// `tauri-build` shells out to `rc.exe` (or `windres`/`llvm-rc`) to build a `.rc`
/// script holding the application manifest, the icon and the version block. On a
/// machine without the Microsoft resource compiler a stub `llvm-rc` can exit with
/// status 0 and write a 32 byte, section-less COFF object: the build looks fine and
/// the executable ends up with an empty `.rsrc`.
///
/// An empty `.rsrc` is not cosmetic. Without the manifest the loader never binds
/// `Microsoft.Windows.Common-Controls` v6, so the `TaskDialogIndirect` import used
/// by the dialog plugin cannot be resolved and the process dies at load time with
/// `STATUS_ENTRYPOINT_NOT_FOUND` (0xC0000139) before any of our code runs.
///
/// `winresource` builds the same resources directly in Rust, so this check makes the
/// executable correct on such machines while leaving the normal path untouched.
#[cfg(target_os = "windows")]
fn embed_windows_resources_if_missing() {
    use std::path::Path;

    let out_dir = std::env::var_os("OUT_DIR").expect("OUT_DIR is always set for a build script");
    let object = Path::new(&out_dir).join("resource.lib");

    // An empty COFF archive is exactly 32 bytes and holds no section table.
    let looks_empty = match std::fs::metadata(&object) {
        Ok(metadata) => metadata.len() <= 64,
        Err(_) => true,
    };
    if !looks_empty {
        return;
    }

    println!(
        "cargo:warning=tauri-build produced an empty Windows resource object; \
         embedding the manifest, icon and version block with winresource instead"
    );

    let version = std::env::var("CARGO_PKG_VERSION").unwrap_or_else(|_| "0.0.0".to_string());
    let (major, minor, patch) = parse_version(&version);

    let mut resource = winresource::WindowsResource::new();
    resource.set_icon("icons/icon.ico");
    resource.set("FileVersion", &format!("{major}.{minor}.{patch}.0"));
    resource.set("ProductVersion", &format!("{major}.{minor}.{patch}.0"));
    resource.set("CompanyName", "VYNX");
    resource.set("FileDescription", "VYNX QR");
    resource.set("ProductName", "VYNX QR");
    resource.set("LegalCopyright", "Copyright (c) 2026 VYNX");
    resource.set_manifest(MANIFEST);
    resource
        .compile()
        .expect("winresource must be able to build the Windows resources");
}

/// Common Controls v6 is required because the dialog plugin calls
/// `TaskDialogIndirect`, which only exists in the v6 flavour of comctl32.
#[cfg(target_os = "windows")]
const MANIFEST: &str = r#"<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <dependency>
    <dependentAssembly>
      <assemblyIdentity
        type="win32"
        name="Microsoft.Windows.Common-Controls"
        version="6.0.0.0"
        processorArchitecture="*"
        publicKeyToken="6595b64144ccf1df"
        language="*"
      />
    </dependentAssembly>
  </dependency>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">permonitorv2,permonitor</dpiAwareness>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
</assembly>
"#;

#[cfg(target_os = "windows")]
fn parse_version(version: &str) -> (u32, u32, u32) {
    let mut parts = version
        .split(['.', '-', '+'])
        .map(|part| part.parse::<u32>().unwrap_or(0));
    let major = parts.next().unwrap_or(0);
    let minor = parts.next().unwrap_or(0);
    let patch = parts.next().unwrap_or(0);
    (major, minor, patch)
}