//! Cryptocurrency payment URI payloads.

use crate::error::{err_with, AppResult, ErrorCode};
use crate::formats::percent_encode;

/// Build a cryptocurrency payment URI.
pub fn encode(currency: &str, address: &str, amount: &str, label: &str) -> AppResult<String> {
    let address = address.trim();
    if address.is_empty() {
        return Err(err_with(ErrorCode::EmptyInput, "Cryptocurrency address cannot be empty"));
    }

    let currency = currency.trim().to_lowercase();
    let amount = amount.trim();
    let label = label.trim();

    let uri = match currency.as_str() {
        "bitcoin" | "btc" => {
            let mut out = format!("bitcoin:{address}");
            let mut has_param = false;
            if !amount.is_empty() {
                out.push_str(&format!("?amount={amount}"));
                has_param = true;
            }
            if !label.is_empty() {
                out.push(if has_param { '&' } else { '?' });
                out.push_str(&format!("label={}", percent_encode(label)));
            }
            out
        }
        "ethereum" | "eth" => {
            let mut out = format!("ethereum:{address}");
            if !amount.is_empty() {
                out.push_str(&format!("?value={amount}"));
            }
            out
        }
        "ton" => {
            let mut out = format!("ton://transfer/{address}");
            let mut has_param = false;
            if !amount.is_empty() {
                out.push_str(&format!("?amount={amount}"));
                has_param = true;
            }
            if !label.is_empty() {
                out.push(if has_param { '&' } else { '?' });
                out.push_str(&format!("text={}", percent_encode(label)));
            }
            out
        }
        _ => {
            // Generic address fallback
            address.to_string()
        }
    };

    Ok(uri)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn btc_encode() {
        let uri = encode("btc", "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa", "0.05", "Donation").unwrap();
        assert_eq!(uri, "bitcoin:1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa?amount=0.05&label=Donation");
    }

    #[test]
    fn ton_encode() {
        let uri = encode("ton", "EQCD39VS5jcptHL8vMjEXrzGaRcCVYto7HUn4bpAOg8xqB2N", "10", "Coffee").unwrap();
        assert_eq!(uri, "ton://transfer/EQCD39VS5jcptHL8vMjEXrzGaRcCVYto7HUn4bpAOg8xqB2N?amount=10&text=Coffee");
    }
}
