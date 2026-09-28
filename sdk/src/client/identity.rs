//! MSC4153 "exclude insecure devices" mode plus the per-room identity-change
//! (pinning) warnings it depends on.
//!
//! The mode is a process-wide flag read when a matrix-sdk `Client` is built
//! (`oauth::build_configured_client`), so toggling it takes effect on the
//! next account start. The identity watcher runs regardless of the flag:
//! warning about a changed identity is useful in either mode, and in strict
//! mode a verified user's identity change blocks sending until it is resolved.

use super::{err, ok, ClientFfi};
use crate::ffi::OpResult;

use matrix_sdk_base::crypto::{CollectStrategy, DecryptionSettings, TrustRequirement};
use std::sync::atomic::{AtomicBool, Ordering};

#[cfg(not(test))]
use super::SendHandler;
#[cfg(not(test))]
use parking_lot::Mutex;
#[cfg(not(test))]
use std::sync::Arc;

static EXCLUDE_INSECURE_DEVICES: AtomicBool = AtomicBool::new(false);

/// Set the process-wide MSC4153 mode. Called by the C++ layer at launch (from
/// the persisted setting) before any client is built.
pub fn set_exclude_insecure_devices(enabled: bool) {
    EXCLUDE_INSECURE_DEVICES.store(enabled, Ordering::Relaxed);
}

pub(crate) fn exclude_insecure_devices() -> bool {
    EXCLUDE_INSECURE_DEVICES.load(Ordering::Relaxed)
}

/// Who receives room keys (and other Olm-encrypted to-device payloads).
/// `IdentityBasedStrategy` shares only with devices their owner cross-signed
/// and refuses to send while a verified user's identity has changed.
pub(crate) fn room_key_recipient_strategy(exclude_insecure: bool) -> CollectStrategy {
    if exclude_insecure {
        CollectStrategy::IdentityBasedStrategy
    } else {
        CollectStrategy::AllDevices
    }
}

/// Which senders' messages decrypt. `CrossSignedOrLegacy` still decrypts
/// Megolm sessions received before matrix-sdk recorded sender trust, so old
/// history doesn't turn into UTDs.
pub(crate) fn decryption_settings(exclude_insecure: bool) -> DecryptionSettings {
    DecryptionSettings {
        sender_device_trust_requirement: if exclude_insecure {
            TrustRequirement::CrossSignedOrLegacy
        } else {
            TrustRequirement::Untrusted
        },
    }
}

/// Wire encoding of an identity-violation kind for `on_identity_status_changed`.
pub(crate) const IDENTITY_PIN_VIOLATION: u8 = 1;
pub(crate) const IDENTITY_VERIFICATION_VIOLATION: u8 = 2;

/// Watch `room`'s members for identity changes and report the room's full
/// current set of violations to the UI on every change (an empty set clears
/// the banner). Returns `None` for unencrypted rooms, which have nothing to
/// warn about.
#[cfg(not(test))]
pub(super) fn spawn_identity_watcher(
    room: &matrix_sdk::Room,
    room_id: String,
    handler: &Arc<Mutex<SendHandler>>,
    rt: &tokio::runtime::Runtime,
    cancelled: Arc<AtomicBool>,
) -> Option<tokio::task::AbortHandle> {
    use futures_util::StreamExt;
    use matrix_sdk_base::crypto::IdentityState;
    use std::collections::BTreeMap;

    if !room.encryption_state().is_encrypted() {
        return None;
    }
    let room = room.clone();
    let h = Arc::clone(handler);
    let task = rt.spawn(async move {
        let stream = match room.subscribe_to_identity_status_changes().await {
            Ok(s) => s,
            Err(e) => {
                tracing::warn!("identity status subscription failed for {room_id}: {e}");
                return;
            }
        };
        let mut stream = std::pin::pin!(stream);
        // user_id → violation kind; users back in Pinned/Verified are removed.
        let mut violations: BTreeMap<String, u8> = BTreeMap::new();
        while let Some(changes) = stream.next().await {
            for change in changes {
                let uid = change.user_id.to_string();
                match change.changed_to {
                    IdentityState::PinViolation => {
                        violations.insert(uid, IDENTITY_PIN_VIOLATION);
                    }
                    IdentityState::VerificationViolation => {
                        violations.insert(uid, IDENTITY_VERIFICATION_VIOLATION);
                    }
                    IdentityState::Pinned | IdentityState::Verified => {
                        violations.remove(&uid);
                    }
                }
            }
            let mut user_ids = Vec::with_capacity(violations.len());
            let mut names = Vec::with_capacity(violations.len());
            let mut kinds = Vec::with_capacity(violations.len());
            for (uid, kind) in &violations {
                let name = match matrix_sdk::ruma::UserId::parse(uid.as_str()) {
                    Ok(parsed) => match room.get_member_no_sync(&parsed).await {
                        Ok(Some(m)) => m.display_name().unwrap_or_default().to_owned(),
                        _ => String::new(),
                    },
                    Err(_) => String::new(),
                };
                user_ids.push(uid.clone());
                names.push(name);
                kinds.push(*kind);
            }
            if cancelled.load(Ordering::Acquire) {
                return;
            }
            let guard = h.lock();
            guard.on_identity_status_changed(&room_id, &user_ids, &names, &kinds);
        }
    });
    Some(task.abort_handle())
}

/// Wire encoding of `get_user_trust`.
pub(crate) const TRUST_UNKNOWN: u8 = 0; // no cross-signing identity known
pub(crate) const TRUST_NOT_VERIFIED: u8 = 1;
pub(crate) const TRUST_VERIFIED: u8 = 2;
pub(crate) const TRUST_VERIFICATION_VIOLATION: u8 = 3; // was verified, identity reset since

/// Map an identity's flags to the `get_user_trust` code. An unacknowledged
/// reset of an unverified identity isn't distinguished here (matrix-sdk's
/// wrapper doesn't expose it); the room's identity banner covers that.
pub(crate) fn trust_code(verified: bool, verification_violation: bool) -> u8 {
    if verified {
        TRUST_VERIFIED
    } else if verification_violation {
        TRUST_VERIFICATION_VIOLATION
    } else {
        TRUST_NOT_VERIFIED
    }
}

impl ClientFfi {
    /// How far `user_id`'s cross-signing identity is trusted (`TRUST_*`).
    /// Reads the local crypto store only (downloads the keys first when the
    /// user isn't tracked yet). Blocks — worker thread.
    #[cfg(not(test))]
    pub fn get_user_trust(&self, user_id: &str) -> u8 {
        let Some(client) = self.client.clone() else {
            return TRUST_UNKNOWN;
        };
        let Ok(uid) = matrix_sdk::ruma::OwnedUserId::try_from(user_id) else {
            return TRUST_UNKNOWN;
        };
        self.rt.block_on(async move {
            let enc = client.encryption();
            let identity = match enc.get_user_identity(&uid).await {
                Ok(Some(i)) => Some(i),
                // Not tracked yet: ask the server once.
                Ok(None) => enc.request_user_identity(&uid).await.ok().flatten(),
                Err(_) => None,
            };
            match identity {
                None => TRUST_UNKNOWN,
                Some(i) => trust_code(i.is_verified(), i.has_verification_violation()),
            }
        })
    }

    #[cfg(test)]
    pub fn get_user_trust(&self, _user_id: &str) -> u8 {
        TRUST_UNKNOWN
    }

    /// Accept `user_id`'s new identity after a pin violation (the user saw the
    /// "identity was reset" warning and chose to continue). Blocks — worker
    /// thread.
    #[cfg(not(test))]
    pub fn pin_user_identity(&self, user_id: &str) -> OpResult {
        let Some(client) = self.client.clone() else {
            return err("not logged in");
        };
        let uid = match matrix_sdk::ruma::OwnedUserId::try_from(user_id) {
            Ok(u) => u,
            Err(e) => return err(format!("invalid user id: {e}")),
        };
        match self.rt.block_on(async move {
            let identity = client
                .encryption()
                .get_user_identity(&uid)
                .await?
                .ok_or_else(|| anyhow::anyhow!("no identity known for user"))?;
            identity.pin().await?;
            Ok::<(), anyhow::Error>(())
        }) {
            Ok(()) => ok(""),
            Err(e) => err(e.to_string()),
        }
    }

    #[cfg(test)]
    pub fn pin_user_identity(&self, _user_id: &str) -> OpResult {
        err("not logged in")
    }

    /// Withdraw verification of `user_id` after their identity changed while
    /// verified. The new identity becomes pinned (unverified), which unblocks
    /// sending in exclude-insecure-devices mode. Blocks — worker thread.
    #[cfg(not(test))]
    pub fn withdraw_user_verification(&self, user_id: &str) -> OpResult {
        let Some(client) = self.client.clone() else {
            return err("not logged in");
        };
        let uid = match matrix_sdk::ruma::OwnedUserId::try_from(user_id) {
            Ok(u) => u,
            Err(e) => return err(format!("invalid user id: {e}")),
        };
        match self.rt.block_on(async move {
            let identity = client
                .encryption()
                .get_user_identity(&uid)
                .await?
                .ok_or_else(|| anyhow::anyhow!("no identity known for user"))?;
            identity.withdraw_verification().await?;
            Ok::<(), anyhow::Error>(())
        }) {
            Ok(()) => ok(""),
            Err(e) => err(e.to_string()),
        }
    }

    #[cfg(test)]
    pub fn withdraw_user_verification(&self, _user_id: &str) -> OpResult {
        err("not logged in")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_mode_shares_with_all_devices_and_decrypts_everything() {
        assert!(matches!(room_key_recipient_strategy(false), CollectStrategy::AllDevices));
        assert!(matches!(
            decryption_settings(false).sender_device_trust_requirement,
            TrustRequirement::Untrusted
        ));
    }

    #[test]
    fn strict_mode_follows_msc4153() {
        assert!(matches!(
            room_key_recipient_strategy(true),
            CollectStrategy::IdentityBasedStrategy
        ));
        assert!(matches!(
            decryption_settings(true).sender_device_trust_requirement,
            TrustRequirement::CrossSignedOrLegacy
        ));
    }

    #[test]
    fn trust_code_maps_identity_flags() {
        assert_eq!(trust_code(true, false), TRUST_VERIFIED);
        assert_eq!(trust_code(false, true), TRUST_VERIFICATION_VIOLATION);
        assert_eq!(trust_code(false, false), TRUST_NOT_VERIFIED);
    }

    #[test]
    fn flag_round_trips() {
        set_exclude_insecure_devices(true);
        assert!(exclude_insecure_devices());
        set_exclude_insecure_devices(false);
        assert!(!exclude_insecure_devices());
    }
}
