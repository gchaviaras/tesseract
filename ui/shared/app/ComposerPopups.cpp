#include "app/ComposerPopups.h"

#include "app/RoomPane.h"
#include "tk/text_area.h"
#include "views/ComposePopups.h"
#include "views/RoomView.h"

namespace tesseract
{

ComposerPopups::ComposerPopups(tk::Host& host, tk::TextArea* text_area,
                               RoomPane* pane, views::RoomView* room_view)
    : text_area_(text_area),
      pane_(pane),
      room_view_(room_view)
{
    // Compose text area -> popups, through the shared priority dispatch
    // (gif > slash > shortcode > mention).
    text_area_->set_on_changed(
        [this](const std::string& s)
        {
            const bool typing = !s.empty();
            if (typing != typing_active_)
            {
                typing_active_ = typing;
                pane_->send_typing_notice_(typing);
            }
            if (room_view_)
            {
                room_view_->set_current_text(s);
            }
            views::dispatch_compose_text_changed(
                s, text_area_->cursor_byte_pos(), gif_controller_.get(),
                slash_controller_.get(), shortcode_controller_.get(),
                mention_controller_.get());
        });
    text_area_->set_on_submit(
        [this]
        {
            if (views::dispatch_compose_submit(
                    gif_controller_.get(), slash_controller_.get(),
                    shortcode_controller_.get(), mention_controller_.get()))
            {
                return;
            }
            if (room_view_)
            {
                room_view_->compose_bar()->trigger_send();
            }
        });
    // Auto-grow (set_on_height_changed) is wired internally by ComposeBar's
    // own constructor — see ComposeBar::ComposeBar()'s text_area_ setup.
    text_area_->push_popup_nav(
        [this](tk::NavKey nk) -> bool
        {
            return views::dispatch_compose_nav(
                nk, gif_controller_.get(), slash_controller_.get(),
                shortcode_controller_.get(), mention_controller_.get());
        });

    // ── @mention autocomplete popup (eager, hidden until shown) ───────────
    mention_popup_ = host.make_popup_surface();
    {
        auto w = std::make_unique<views::MentionPopup>();
        mention_popup_widget_ = w.get();
        if (mention_popup_)
        {
            mention_popup_->set_root(std::move(w));
        }
    }
    {
        views::MentionController::Hooks hooks;
        hooks.show = [this](tk::Rect cursor, int rows)
        {
            RoomPane::position_dropdown_popup_(
                mention_popup_.get(), cursor, rows,
                views::MentionPopup::kRowHeight, views::MentionPopup::kWidth);
        };
        hooks.hide = [this]
        {
            if (mention_popup_)
            {
                mention_popup_->set_visible(false);
            }
        };
        hooks.repaint = [this] { refresh_(mention_popup_.get()); };
        hooks.repaint_selection = [this]
        {
            refresh_(mention_popup_.get(), /*layout=*/false);
        };
        pane_->wire_mention_hooks_(mention_popup_widget_, hooks);
        mention_controller_ = std::make_unique<views::MentionController>(
            text_area_, pane_->shell_client_(), mention_popup_widget_,
            std::move(hooks));
    }
    if (mention_popup_)
    {
        // Pop-outs previously never auto-dismissed on outside click, unlike
        // the main window's mention popup — intentional behavior fix, not a
        // pre-existing pattern being ported.
        mention_popup_->on_dismiss_requested = [this]
        {
            if (mention_controller_)
            {
                mention_controller_->hide();
            }
        };
    }

    // ── /command autocomplete popup ───────────────────────────────────────
    slash_popup_ = host.make_popup_surface();
    {
        auto w = std::make_unique<views::SlashCommandPopup>();
        slash_popup_widget_ = w.get();
        if (slash_popup_)
        {
            slash_popup_->set_root(std::move(w));
        }
    }
    {
        views::SlashCommandController::Hooks sh;
        sh.show = [this](tk::Rect cursor, int rows)
        {
            RoomPane::position_dropdown_popup_(
                slash_popup_.get(), cursor, rows,
                views::SlashCommandPopup::kRowHeight,
                views::SlashCommandPopup::kWidth);
        };
        sh.hide = [this]
        {
            if (slash_popup_)
            {
                slash_popup_->set_visible(false);
            }
        };
        sh.repaint = [this] { refresh_(slash_popup_.get()); };
        sh.repaint_selection = [this]
        {
            refresh_(slash_popup_.get(), /*layout=*/false);
        };
        pane_->wire_slash_hooks_(sh);
        slash_controller_ = std::make_unique<views::SlashCommandController>(
            text_area_, slash_popup_widget_, std::move(sh));
    }
    if (slash_popup_)
    {
        slash_popup_->on_dismiss_requested = [this]
        {
            if (slash_controller_)
            {
                slash_controller_->hide();
            }
        };
    }

    // ── :shortcode: emoji/emoticon autocomplete popup ─────────────────────
    shortcode_popup_ = host.make_popup_surface();
    {
        auto w = std::make_unique<views::ShortcodePopup>();
        shortcode_popup_widget_ = w.get();
        if (shortcode_popup_)
        {
            shortcode_popup_->set_root(std::move(w));
        }
    }
    {
        views::ShortcodeController::Hooks sh;
        sh.show = [this](tk::Rect cursor, int rows)
        {
            RoomPane::position_dropdown_popup_(
                shortcode_popup_.get(), cursor, rows,
                views::ShortcodePopup::kRowHeight,
                views::ShortcodePopup::kWidth);
        };
        sh.hide = [this]
        {
            if (shortcode_popup_)
            {
                shortcode_popup_->set_visible(false);
            }
        };
        sh.repaint = [this] { refresh_(shortcode_popup_.get()); };
        sh.repaint_selection = [this]
        {
            refresh_(shortcode_popup_.get(), /*layout=*/false);
        };
        // Custom-emoticon thumbnails: peek the shell media cache (populated by
        // the controller's fetch_image hook, wired below); Unicode emoji
        // render as glyphs.
        pane_->wire_shortcode_hooks_(shortcode_popup_widget_, sh);
        shortcode_controller_ = std::make_unique<views::ShortcodeController>(
            text_area_, shortcode_popup_widget_, std::move(sh));
    }
    if (shortcode_popup_)
    {
        shortcode_popup_->on_dismiss_requested = [this]
        {
            if (shortcode_controller_)
            {
                shortcode_controller_->hide();
            }
        };
    }

    // ── /gif inline result strip ──────────────────────────────────────────
    gif_popup_ = host.make_popup_surface();
    {
        auto w = std::make_unique<views::GifPopup>();
        gif_popup_widget_ = w.get();
        // Strip cells render via the shell's shared two-stage provider. The
        // repaint refreshes THIS pop-out's surface and is self-guarded by the
        // window's liveness token (the shell's in-flight fetch may outlive us).
        gif_popup_widget_->set_image_provider(
            [this](const GifResult& result) -> const tk::Image*
            {
                return pane_->shell_gif_strip_image_(
                    result,
                    pane_->guarded(
                        [this] { refresh_(gif_popup_.get(), /*layout=*/false); }));
            });
        if (gif_popup_)
        {
            gif_popup_->set_root(std::move(w));
        }
    }
    {
        views::GifController::Hooks gh;
        gh.show = [this] { show_gif_popup(); };
        gh.hide = [this] { hide_gif_popup(); };
        gh.repaint = [this] { refresh_(gif_popup_.get()); };
        gh.repaint_selection = [this]
        {
            refresh_(gif_popup_.get(), /*layout=*/false);
        };
        pane_->wire_gif_hooks_(gh);
        gif_controller_ = std::make_unique<views::GifController>(
            text_area_, gif_popup_widget_, std::move(gh));
    }
}

ComposerPopups::~ComposerPopups()
{
    // Unhook the lambdas that capture `this`. The owning shell declares
    // popups_ after surface_ (and pane_ lives in RoomWindowBase, destroyed
    // after the derived members), so the text area and pane are still alive
    // here.
    if (text_area_)
    {
        text_area_->set_on_changed(nullptr);
        text_area_->set_on_submit(nullptr);
        text_area_->pop_popup_nav(); // the one handler pushed in the ctor
    }
    if (typing_active_)
    {
        typing_active_ = false;
        if (pane_)
        {
            pane_->send_typing_notice_(false);
        }
    }
}

void ComposerPopups::refresh_(tk::PopupSurfaceHandle* popup, bool layout)
{
    if (!popup)
    {
        return;
    }
    // layout=true: the controller's result set changed (new content / a
    // changed row count) and needs a re-measure; relayout ends in a repaint on
    // every backend. layout=false: pixel-only change (an image/thumbnail
    // finished loading), so a plain repaint suffices.
    if (layout)
    {
        popup->request_relayout();
    }
    else
    {
        popup->request_repaint();
    }
}

void ComposerPopups::hide_all()
{
    views::hide_all_compose_popups(gif_controller_.get(), slash_controller_.get(),
                                   shortcode_controller_.get(),
                                   mention_controller_.get());
}

void ComposerPopups::show_gif_popup()
{
    if (!gif_popup_ || !gif_popup_widget_ || !text_area_ || !room_view_)
    {
        return;
    }
    // Full-width strip floating just above the compose bar (like the main
    // window's). content_size() drives the height + the empty/status check.
    const tk::Rect cb = room_view_->compose_bar_rect();
    const tk::Size sz = gif_popup_widget_->content_size(cb.w);
    if (cb.w <= 0.0f || sz.h <= 0.0f)
    {
        hide_gif_popup();
        return;
    }
    gif_popup_->set_rect(cb, {cb.w, sz.h}, tk::PopupPlacement::PreferAbove);
    gif_popup_->set_visible(true);
}

void ComposerPopups::hide_gif_popup()
{
    if (gif_popup_)
    {
        gif_popup_->set_visible(false);
    }
}

void ComposerPopups::on_gif_results(std::uint64_t request_id,
                                    std::vector<GifResult> results)
{
    if (gif_controller_)
    {
        gif_controller_->on_results(request_id, std::move(results));
    }
}

void ComposerPopups::on_gif_search_failed(std::uint64_t request_id,
                                          const std::string& message)
{
    if (gif_controller_)
    {
        gif_controller_->on_search_failed(request_id, message);
    }
}

void ComposerPopups::apply_theme(const tk::Theme& theme)
{
    if (mention_popup_)
    {
        mention_popup_->set_theme(theme);
    }
    if (slash_popup_)
    {
        slash_popup_->set_theme(theme);
    }
    if (shortcode_popup_)
    {
        shortcode_popup_->set_theme(theme);
    }
    if (gif_popup_)
    {
        gif_popup_->set_theme(theme);
    }
}

void ComposerPopups::repaint_anim_frame()
{
    // Advance the /gif strip's animated cells (frames come from the shared
    // anim cache; the shell's tick fires this for every window). No
    // set_anim_cache is wired for a pop-out's popup (unlike MainWindow's), so
    // a plain full repaint is used rather than update_anim_regions()'s
    // partial-redraw path — it still picks up the shared cache's already-
    // centrally-advanced current frame, just without the per-region
    // optimization.
    if (gif_popup_ && gif_popup_->visible())
    {
        gif_popup_->request_repaint();
    }
}

} // namespace tesseract
