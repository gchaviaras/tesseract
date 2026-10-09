//! Process-wide HTTP proxy configuration.
//!
//! Set once at launch (from the persisted setting, before any client is
//! built and before any runtime threads exist) via [`set_proxy`]; every
//! `reqwest` client builder in the crate then goes through [`apply`].
//!
//! Modes: `System` (reqwest's default: OS/env proxy), `None` (never use a
//! proxy, ignoring OS/env), `Manual` (an explicit `http(s)://[user:pass@]host:port`
//! URL; credentials in the URL are sent as basic auth).
//!
//! LiveKit call signalling (`livekit-api`) builds its own HTTP stack and only
//! reads `HTTPS_PROXY`/`HTTP_PROXY` from the environment, so [`set_proxy`]
//! mirrors the choice into the process environment. WebRTC media (UDP/TURN)
//! cannot traverse an HTTP proxy and is unaffected.

use std::sync::RwLock;

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) enum ProxyMode {
    System,
    None,
    Manual(String),
}

static PROXY: RwLock<ProxyMode> = RwLock::new(ProxyMode::System);

const ENV_VARS: [&str; 4] = ["HTTPS_PROXY", "HTTP_PROXY", "https_proxy", "http_proxy"];

/// True if `url` is an http/https proxy URL reqwest accepts.
pub fn validate_proxy_url(url: &str) -> bool {
    let url = url.trim();
    let Ok(parsed) = url::Url::parse(url) else {
        return false;
    };
    matches!(parsed.scheme(), "http" | "https")
        && parsed.host_str().is_some_and(|h| !h.is_empty())
        && reqwest::Proxy::all(url).is_ok()
}

/// Pure mode parsing: 0 = System, 1 = None, 2 = Manual. `None` result means
/// the input was invalid (unknown mode, or a bad Manual URL).
fn parse(mode: u8, url: &str) -> Option<ProxyMode> {
    match mode {
        0 => Some(ProxyMode::System),
        1 => Some(ProxyMode::None),
        2 => validate_proxy_url(url).then(|| ProxyMode::Manual(url.trim().to_owned())),
        _ => None,
    }
}

/// Store the process-wide proxy setting (0 = system, 1 = none, 2 = manual).
/// Returns false and falls back to System if the mode or Manual URL is
/// invalid. Call before any client is built and before runtime threads start
/// (it edits the process environment).
pub fn set_proxy(mode: u8, url: &str) -> bool {
    let (parsed, ok) = match parse(mode, url) {
        Some(m) => (m, true),
        None => (ProxyMode::System, false),
    };
    match &parsed {
        ProxyMode::System => {}
        ProxyMode::None => {
            for v in ENV_VARS {
                std::env::remove_var(v);
            }
        }
        ProxyMode::Manual(u) => {
            for v in ENV_VARS {
                std::env::set_var(v, u);
            }
        }
    }
    if let Ok(mut g) = PROXY.write() {
        *g = parsed;
    }
    ok
}

pub(crate) fn current() -> ProxyMode {
    PROXY.read().map(|g| g.clone()).unwrap_or(ProxyMode::System)
}

/// True when the user configured an explicit (Manual) proxy, so every
/// connection's peer is that proxy rather than the target.
pub(crate) fn is_manual() -> bool {
    matches!(current(), ProxyMode::Manual(_))
}

fn apply_mode(mode: &ProxyMode, b: reqwest::ClientBuilder) -> reqwest::ClientBuilder {
    match mode {
        ProxyMode::System => b,
        ProxyMode::None => b.no_proxy(),
        ProxyMode::Manual(url) => match reqwest::Proxy::all(url.as_str()) {
            Ok(p) => b.no_proxy().proxy(p),
            Err(_) => b,
        },
    }
}

/// Apply the configured proxy to a client builder.
pub(crate) fn apply(b: reqwest::ClientBuilder) -> reqwest::ClientBuilder {
    apply_mode(&current(), b)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_modes() {
        assert_eq!(parse(0, ""), Some(ProxyMode::System));
        assert_eq!(parse(1, ""), Some(ProxyMode::None));
        assert_eq!(
            parse(2, "http://u:p@h:3128"),
            Some(ProxyMode::Manual("http://u:p@h:3128".into()))
        );
        assert_eq!(parse(9, ""), None);
    }

    #[test]
    fn rejects_bad_urls() {
        assert!(!validate_proxy_url("ftp://h:21"));
        assert!(!validate_proxy_url("socks5://h:1080"));
        assert!(!validate_proxy_url("garbage"));
        assert!(!validate_proxy_url(""));
        assert!(validate_proxy_url("http://u:p@h:3128"));
        assert!(validate_proxy_url("https://proxy.example.com:8080"));
        assert_eq!(parse(2, "socks5://h:1080"), None);
    }

    #[test]
    fn apply_builds() {
        for m in [
            ProxyMode::System,
            ProxyMode::None,
            ProxyMode::Manual("http://u:p@h:3128".into()),
        ] {
            assert!(apply_mode(&m, reqwest::Client::builder()).build().is_ok());
        }
    }
}
