//! Host integrations selected at compile time; no desktop-specific Linux queries.
#[cfg(target_os = "linux")]
pub mod linux;
#[cfg(windows)]
pub mod windows;
#[cfg(target_os = "linux")]
pub use linux::{system_info, SystemInfo};
#[cfg(windows)]
pub use windows::{system_info, SystemInfo};
