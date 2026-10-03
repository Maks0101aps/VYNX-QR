//! Windows specific integrations.
//!
//! Everything here is read-only and best effort. When a registry value is missing
//! the app falls back to its own defaults instead of failing, and the module
//! compiles on non Windows hosts so the rest of the core stays testable anywhere.

/// Information the UI needs from the host operating system.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SystemInfo {
    /// `10.0.<build>` on Windows, or `0.0.0` elsewhere.
    pub os_build: String,
    /// Windows 11 and newer start at build 22000.
    pub is_windows_11: bool,
    /// `#RRGGBB` accent colour, or `None` when Windows reports no custom colour.
    pub accent_color: Option<String>,
    /// True when the app runs from a development build.
    pub development: bool,
}

/// Read the host details. Never fails: unknown values become `None`/`0`.
pub fn system_info() -> SystemInfo {
    let build = platform::current_build_number();
    SystemInfo {
        os_build: build.map_or_else(|| "0.0.0".to_string(), |value| format!("10.0.{value}")),
        is_windows_11: build.is_some_and(|value| value >= 22_000),
        accent_color: platform::accent_color(),
        development: cfg!(debug_assertions),
    }
}

#[cfg(windows)]
mod platform {
    use windows::core::HSTRING;
    use windows::Win32::Foundation::ERROR_SUCCESS;
    use windows::Win32::System::Registry::{
        RegGetValueW, HKEY, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, RRF_RT_REG_DWORD,
    };

    fn read_dword(root: HKEY, subkey: &str, name: &str) -> Option<u32> {
        let subkey = HSTRING::from(subkey);
        let name = HSTRING::from(name);
        let mut value = 0u32;
        let mut size = std::mem::size_of::<u32>() as u32;
        let status = unsafe {
            RegGetValueW(
                root,
                &subkey,
                &name,
                RRF_RT_REG_DWORD,
                None,
                Some(&mut value as *mut u32 as *mut core::ffi::c_void),
                Some(&mut size),
            )
        };
        (status == ERROR_SUCCESS).then_some(value)
    }

    pub(super) fn current_build_number() -> Option<u32> {
        read_dword(
            HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Windows NT\CurrentVersion",
            "CurrentBuildNumber",
        )
    }

    pub(super) fn accent_color() -> Option<String> {
        let value = read_dword(
            HKEY_CURRENT_USER,
            r"SOFTWARE\Microsoft\Windows\DWM",
            "ColorizationColor",
        )?;
        if value == 0 {
            return None;
        }
        // Stored as 0x00BBGGRR.
        let red = (value >> 16) & 0xFF;
        let green = (value >> 8) & 0xFF;
        let blue = value & 0xFF;
        if red + green + blue == 0 {
            return None;
        }
        Some(format!("#{red:02X}{green:02X}{blue:02X}"))
    }
}

#[cfg(not(windows))]
mod platform {
    pub(super) fn current_build_number() -> Option<u32> {
        None
    }

    pub(super) fn accent_color() -> Option<String> {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn system_info_is_always_populated() {
        let info = system_info();
        assert!(!info.os_build.is_empty());
        assert!(!info.development || cfg!(debug_assertions));
    }

    #[test]
    fn accent_colour_is_well_formed_when_present() {
        if let Some(color) = system_info().accent_color {
            assert!(color.starts_with('#'), "accent must start with #");
            assert_eq!(color.len(), 7, "accent must be #RRGGBB");
            assert!(color[1..].chars().all(|c| c.is_ascii_hexdigit()));
        }
    }
}
