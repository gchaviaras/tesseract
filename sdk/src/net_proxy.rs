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
    /// A Manual proxy was requested but its URL is invalid: fail closed. Every
    /// request is routed to an unresolvable proxy and errors, rather than
    /// quietly going out directly (or through the OS proxy) against the
    /// user's intent.
    Blocked,
}

/// Proxy used for [`ProxyMode::Blocked`]. `.invalid` is reserved (RFC 6761)
/// and never resolves, so every connection through it fails.
const BLOCKED_PROXY: &str = "http://proxy.invalid:9";

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

/// What an invalid request resolves to: a bad Manual URL fails closed
/// (the user asked for a proxy, so never go out without one); an unknown
/// mode value falls back to System.
fn fallback(mode: u8) -> ProxyMode {
    if mode == 2 {
        ProxyMode::Blocked
    } else {
        ProxyMode::System
    }
}

/// Store the process-wide proxy setting (0 = system, 1 = none, 2 = manual).
/// Returns false if the mode or Manual URL is invalid: a bad Manual URL then
/// fails closed (all requests error), an unknown mode falls back to System. Call before any client is built and before runtime threads start
/// (it edits the process environment).
pub fn set_proxy(mode: u8, url: &str) -> bool {
    store(mode, url, true)
}

/// Like [`set_proxy`] but safe to call while runtime threads exist: it only
/// updates the setting that every newly built `reqwest` client reads, and
/// leaves the process environment alone (so LiveKit call signalling, which
/// reads the environment, keeps the launch-time proxy until the next
/// launch). Clients that are already built keep their old proxy.
pub fn set_proxy_live(mode: u8, url: &str) -> bool {
    store(mode, url, false)
}

fn store(mode: u8, url: &str, touch_env: bool) -> bool {
    let (parsed, ok) = match parse(mode, url) {
        Some(m) => (m, true),
        None => (fallback(mode), false),
    };
    if touch_env {
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
            ProxyMode::Blocked => {
                for v in ENV_VARS {
                    std::env::set_var(v, BLOCKED_PROXY);
                }
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
            // Validated on entry, so unreachable; never fall open.
            Err(_) => apply_mode(&ProxyMode::Blocked, b),
        },
        ProxyMode::Blocked => match reqwest::Proxy::all(BLOCKED_PROXY) {
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
    fn bad_manual_url_fails_closed() {
        assert_eq!(fallback(2), ProxyMode::Blocked);
        assert_eq!(fallback(9), ProxyMode::System);
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
            ProxyMode::Blocked,
        ] {
            assert!(apply_mode(&m, reqwest::Client::builder()).build().is_ok());
        }
    }

    #[test]
    fn live_updates_setting_but_not_environment() {
        let before: Vec<_> = ENV_VARS.iter().map(|v| std::env::var(v).ok()).collect();
        assert!(set_proxy_live(2, "http://u:p@h:3128"));
        assert_eq!(current(), ProxyMode::Manual("http://u:p@h:3128".into()));
        // An invalid URL falls back to System and reports false.
        assert!(!set_proxy_live(2, "garbage"));
        assert_eq!(current(), ProxyMode::Blocked);
        // An unknown mode is not a proxy request: fall back to System.
        assert!(!set_proxy_live(9, ""));
        assert_eq!(current(), ProxyMode::System);
        let after: Vec<_> = ENV_VARS.iter().map(|v| std::env::var(v).ok()).collect();
        assert_eq!(before, after);
    }
}
