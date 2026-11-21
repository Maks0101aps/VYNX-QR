//! Payment payloads (e.g. Russian SBP / GOST standard ST00012).

use crate::error::{err_with, AppResult, ErrorCode};

/// Build a Russian banking QR code payment string according to GOST R 56042-2014 / SBP (`ST00012`).
pub fn encode(
    name: &str,
    personal_acc: &str,
    bank_name: &str,
    bic: &str,
    corresp_acc: &str,
    sum: &str,
    purpose: &str,
) -> AppResult<String> {
    let name = name.trim();
    let personal_acc = personal_acc.trim();
    let bank_name = bank_name.trim();
    let bic = bic.trim();

    if name.is_empty() {
        return Err(err_with(
            ErrorCode::EmptyInput,
            "Recipient name cannot be empty",
        ));
    }
    if personal_acc.is_empty() {
        return Err(err_with(
            ErrorCode::EmptyInput,
            "Personal account number cannot be empty",
        ));
    }
    if bank_name.is_empty() {
        return Err(err_with(ErrorCode::EmptyInput, "Bank name cannot be empty"));
    }
    if bic.is_empty() {
        return Err(err_with(ErrorCode::EmptyInput, "BIC cannot be empty"));
    }

    let mut out =
        format!("ST00012|Name={name}|PersonalAcc={personal_acc}|BankName={bank_name}|BIC={bic}");

    let corresp_acc = corresp_acc.trim();
    if !corresp_acc.is_empty() {
        out.push_str(&format!("|CorrespAcc={corresp_acc}"));
    }
    let sum = sum.trim();
    if !sum.is_empty() {
        // Amount in kopecks if whole or decimal
        let kopecks = if let Ok(val) = sum.replace(',', ".").parse::<f64>() {
            format!("{:.0}", (val * 100.0).round())
        } else {
            sum.to_string()
        };
        out.push_str(&format!("|Sum={kopecks}"));
    }
    let purpose = purpose.trim();
    if !purpose.is_empty() {
        out.push_str(&format!("|Purpose={purpose}"));
    }

    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn payment_gost() {
        let res = encode(
            "ИП Иванов И.И.",
            "40802810000000000001",
            "ПАО СБЕРБАНК",
            "044525225",
            "30101810400000000225",
            "1500.50",
            "Оплата услуг",
        )
        .unwrap();

        assert!(res.starts_with("ST00012|Name=ИП Иванов И.И.|PersonalAcc=40802810000000000001"));
        assert!(res.contains("|BIC=044525225"));
        assert!(res.contains("|Sum=150050"));
        assert!(res.contains("|Purpose=Оплата услуг"));
    }
}
