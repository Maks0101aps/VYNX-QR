//! Calendar event (iCalendar `VEVENT`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};

/// Escape special characters in iCalendar text fields according to RFC 5545:
/// commas, semicolons, backslashes and newlines must be backslash-escaped.
fn escape_ical(value: &str) -> String {
    let mut out = String::with_capacity(value.len() + 4);
    for ch in value.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            ';' => out.push_str("\\;"),
            ',' => out.push_str("\\,"),
            '\n' => out.push_str("\\n"),
            '\r' => {}
            _ => out.push(ch),
        }
    }
    out
}

/// Format an ISO date/time like "2026-11-20T10:00:00" into iCalendar format "20261120T100000Z".
fn format_datetime(value: &str) -> String {
    let trimmed = value.trim();
    let cleaned: String = trimmed
        .chars()
        .filter(|c| c.is_ascii_digit() || *c == 'T' || *c == 'Z')
        .collect();
    if cleaned.ends_with('Z') || cleaned.contains('T') {
        cleaned
    } else if !cleaned.is_empty() {
        format!("{cleaned}T000000Z")
    } else {
        cleaned
    }
}

/// Build an iCalendar `VEVENT` payload.
pub fn encode(
    title: &str,
    start: &str,
    end: &str,
    location: &str,
    description: &str,
) -> AppResult<String> {
    let title = title.trim();
    if title.is_empty() {
        return Err(err_with(
            ErrorCode::EmptyInput,
            "Event title cannot be empty",
        ));
    }

    let start_dt = format_datetime(start);
    let end_dt = format_datetime(end);

    let mut ical = String::with_capacity(128 + title.len() + location.len() + description.len());
    ical.push_str("BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\n");
    ical.push_str(&format!("SUMMARY:{}\n", escape_ical(title)));

    if !start_dt.is_empty() {
        ical.push_str(&format!("DTSTART:{start_dt}\n"));
    }
    if !end_dt.is_empty() {
        ical.push_str(&format!("DTEND:{end_dt}\n"));
    }
    let loc = location.trim();
    if !loc.is_empty() {
        ical.push_str(&format!("LOCATION:{}\n", escape_ical(loc)));
    }
    let desc = description.trim();
    if !desc.is_empty() {
        ical.push_str(&format!("DESCRIPTION:{}\n", escape_ical(desc)));
    }

    ical.push_str("END:VEVENT\nEND:VCALENDAR");
    Ok(ical)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn event_basic() {
        let res = encode(
            "Team Meeting",
            "2026-11-20T10:00:00",
            "2026-11-20T11:00:00",
            "Room 101",
            "Weekly sync",
        )
        .unwrap();
        assert!(res.contains("SUMMARY:Team Meeting"));
        assert!(res.contains("LOCATION:Room 101"));
        assert!(res.contains("BEGIN:VEVENT"));
        assert!(res.contains("END:VEVENT"));
    }
}
