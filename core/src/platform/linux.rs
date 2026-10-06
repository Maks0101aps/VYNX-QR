//! Qt owns Linux desktop theme discovery. The engine does not query DBus.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SystemInfo {
    pub os_build: String,
    pub is_windows_11: bool,
    pub accent_color: Option<String>,
    pub development: bool,
}

pub fn system_info() -> SystemInfo {
    SystemInfo {
        os_build: "Linux".into(),
        is_windows_11: false,
        accent_color: None,
        development: cfg!(debug_assertions),
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn system_info_does_not_invent_windows_or_desktop_details() {
        let info = super::system_info();
        assert_eq!(info.os_build, "Linux");
        assert!(!info.is_windows_11);
        assert!(info.accent_color.is_none());
    }
}
