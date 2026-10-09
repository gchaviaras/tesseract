#pragma once

// Shared ShellBase test double. Supplies every pure virtual with a benign
// default so a test only overrides what it cares about; adding a hook to
// ShellBase means editing this one file instead of every test.
//
// Defaults: post_to_ui_* run inline, everything else is a no-op. A test that
// wants queued UI work overrides post_to_ui_ in its own subclass.

#include "app/AccountManager.h"
#include "app/ShellBase.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tesseract::test
{

// Base-class-first holder: AccountManager must outlive ShellBase, and ShellBase
// takes a reference to it in its constructor.
struct AccountManagerHolder
{
    tesseract::AccountManager am_;
};

struct TestShellBase : AccountManagerHolder, tesseract::ShellBase
{
    TestShellBase() : ShellBase(am_) {}

    // Workers post back into derived state; let them finish before it goes away.
    ~TestShellBase() override
    {
        const auto t = std::chrono::seconds(5);
        pool_.wait_idle(t);
        mut_pool_.wait_idle(t);
        media_prefetch_pool_.wait_idle(t);
    }

    void post_to_ui_(std::function<void()> fn) override { fn(); }
    void post_to_ui_after_(int, std::function<void()> fn) override { fn(); }
    void request_relayout_() override {}
    void request_repaint_() override {}
    void on_rooms_updated_() override {}
    void on_media_bytes_ready_(const tk::CacheKey&, MediaKind,
                               std::vector<uint8_t>) override {}
    void on_tab_state_changed_ui_() override {}
    DecodedImage decode_image_(const std::vector<uint8_t>&, int, int) override
    {
        return {};
    }
    std::int64_t monotonic_ms_() override { return 1000; }
    void start_anim_tick_() override {}
    void repaint_pickers_() override {}
    void navigate_to_room_(const std::string&) override {}
    void pick_image_file_(
        std::function<void(std::vector<uint8_t>, std::string)>) override {}
    void show_encryption_setup_overlay_(
        tesseract::views::EncryptionSetupOverlay::Mode) override {}
    void raise_and_activate_() override {}
    std::unique_ptr<tk::AudioPlayback> make_call_audio_output_() override
    {
        return nullptr;
    }
    tesseract::CallWindowBase* create_call_window_() override { return nullptr; }
    bool is_ctrl_held_() const override { return false; }
    void switch_active_account_(const std::string&) override {}
    void refresh_account_ui_after_switch_() override {}
    void bind_settings_controller_() override {}
    void spawn_main_window_(std::shared_ptr<tesseract::AccountSession>) override {}
    std::unique_ptr<tesseract::IEventHandler>
    make_account_bridge_(const std::string&) override { return nullptr; }
    void install_account_notifier_(tesseract::AccountSession&) override {}
    void request_relogin_(const std::string&) override {}
};

} // namespace tesseract::test
