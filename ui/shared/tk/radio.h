#pragma once

// RadioButton + RadioGroup. A RadioGroup owns a set of RadioButtons laid out
// along one axis; exactly one is checked at a time. The group is a single Tab
// stop (the checked button) and arrow keys move + commit the selection, wrapping.

#include "layout.h"

#include <functional>
#include <string>
#include <vector>

namespace tk
{

class RadioGroup;

class RadioButton : public Widget
{
protected:
    explicit RadioButton(std::string label);
    TK_WIDGET_FACTORY_FRIEND(RadioButton)

public:
    void set_checked(bool checked)
    {
        checked_ = checked;
    }
    bool checked() const
    {
        return checked_;
    }
    const std::string& label() const
    {
        return label_;
    }
    // Measure to the label's natural width instead of the offered width
    // (used by horizontal groups).
    void set_natural_width(bool v)
    {
        natural_width_ = v;
    }

    Size measure(LayoutCtx&, Size constraints) override;
    void arrange(LayoutCtx&, Rect bounds) override;
    void paint(PaintCtx&) override;

    bool on_pointer_down(Point local) override;
    void on_pointer_up(Point local, bool inside_self) override;
    bool on_pointer_move(Point local) override;
    void on_pointer_leave() override;

    // Only the checked button is a Tab stop; arrow keys reach the others.
    bool focusable() const override
    {
        return enabled_ && checked_;
    }
    bool focus_on_click() const override
    {
        return enabled_;
    }
    bool on_key_down(const KeyEvent& e) override;

    Role access_role() const override
    {
        return Role::RadioButton;
    }
    std::string access_name() const override
    {
        return label_;
    }
    AccessState access_state() const override
    {
        AccessState s;
        s.checked = checked_;
        return s;
    }
    bool access_default_action() override;

private:
    friend class RadioGroup;
    // Set by the owning group.
    std::function<void(RadioButton*)> on_select_;
    std::function<void(RadioButton*, int)> on_move_;

    std::string label_;
    bool checked_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
    bool natural_width_ = false;
    CachedTextLayout label_layout_;
    float cached_max_w_ = -2.0f;
    Size label_size_{};
};

class RadioGroup : public FlexBox
{
protected:
    explicit RadioGroup(Axis axis);
    TK_WIDGET_FACTORY_FRIEND(RadioGroup)

public:
    struct Option
    {
        std::string label;
        std::string value;
    };

    void set_options(std::vector<Option> options);
    // Silent (does not fire on_changed).
    void set_selected_value(const std::string& value);
    const std::string& selected_value() const;

    std::function<void(std::string)> on_changed;

    Role access_role() const override
    {
        return Role::Group;
    }

private:
    void select_(RadioButton* b, bool fire);
    void move_(RadioButton* from, int dir);

    std::vector<RadioButton*> buttons_;
    std::vector<std::string> values_;
    std::string selected_;
};

} // namespace tk
