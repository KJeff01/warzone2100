/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2021  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** @file
 *  Definitions for the form functions.
 */

#ifndef __INCLUDED_LIB_WIDGET_FORM_H__
#define __INCLUDED_LIB_WIDGET_FORM_H__

#include "lib/widget/widget.h"

#include <nonstd/optional.hpp>
using nonstd::optional;
using nonstd::nullopt;

/* The standard form */
class W_FORM : public WIDGET
{
public:
	enum FormState
	{
		NORMAL,
		MINIMIZED
	};

public:
	W_FORM(W_FORMINIT const *init);
	W_FORM();

	void clicked(W_CONTEXT *psContext, WIDGET_KEY key) override;
	void released(W_CONTEXT *psContext, WIDGET_KEY = WKEY_PRIMARY) override;
	void run(W_CONTEXT *psContext) override;
	void highlightLost() override;
	void display(int xOffset, int yOffset) override;

	void screenSizeDidChange(int oldWidth, int oldHeight, int newWidth, int newHeight) override;
	bool hitTest(int x, int y) const override;
	std::shared_ptr<WIDGET> findMouseTargetRecursive(W_CONTEXT *psContext, WIDGET_KEY key, bool wasPressed) override;
	void displayRecursive(WidgetGraphicsContext const &context) override;
	using WIDGET::displayRecursive;

	void enableMinimizing(const std::string& minimizedTitle, PIELIGHT titleColour = WZCOL_FORM_LIGHT);
	void disableMinimizing();
	bool setFormState(FormState state);
	bool toggleMinimized();
	FormState getFormState() const { return formState; }

	bool            disableChildren;        ///< Disable all child widgets if true
	bool			userMovable = false;	///< Whether the user can drag the form around (NOTE: should only be used with forms on overlay screens, currently)

protected:
	bool capturesMouseDrag(WIDGET_KEY) override;
	void mouseDragged(WIDGET_KEY, W_CONTEXT *start, W_CONTEXT *current) override;

private:
	Vector2i calcMinimizedSize() const;
	const WzRect& minimizedGeometry() const;
	bool isUserMovable() const;
	void displayMinimized(int xOffset, int yOffset);

private:
	std::shared_ptr<W_LABEL>	minimizedTitle;
	WzRect				minimizedRect;
	int					minimizedLeftButtonWidth = 0;
	FormState			formState = FormState::NORMAL;
	optional<Vector2i>	dragStart;
	bool				minimizable = false;
};

/* The clickable form data structure */
class W_CLICKFORM : public W_FORM
{

public:
	W_CLICKFORM(W_FORMINIT const *init);
	W_CLICKFORM();

	void clicked(W_CONTEXT *psContext, WIDGET_KEY key) override;
	virtual bool clickHeld(W_CONTEXT *psContext, WIDGET_KEY key);
	void released(W_CONTEXT *psContext, WIDGET_KEY key) override;
	void highlight(W_CONTEXT *psContext) override;
	void highlightLost() override;
	void run(W_CONTEXT *psContext) override;
	void display(int xOffset, int yOffset) override;
	std::string getTip() override
	{
		return pTip;
	}
	WidgetHelp const * getHelp() const override
	{
		if (!help.has_value()) { return nullptr; }
		return &(help.value());
	}

	unsigned getState() const override;
	void setState(unsigned state) override;
	void setFlash(bool enable) override;
	void setTip(std::string string) override;
	void setHelp(optional<WidgetHelp> help) override;

	using WIDGET::setString;
	using WIDGET::setTip;

	bool isDown() const;
	bool isHighlighted() const;

	unsigned state;                     // Button state of the form

private:
	std::string pTip;                   // Tip for the form
	optional<WidgetHelp> help;
	SWORD HilightAudioID;				// Audio ID for form clicked sound
	SWORD ClickedAudioID;				// Audio ID for form hilighted sound
	WIDGET_AUDIOCALLBACK AudioCallback;	// Pointer to audio callback function
	optional<std::chrono::steady_clock::time_point> clickDownStart; // the start time of click down on this form
	optional<WIDGET_KEY> clickDownKey;
};

class W_FULLSCREENOVERLAY_CLICKFORM : public W_CLICKFORM
{
protected:
	W_FULLSCREENOVERLAY_CLICKFORM(W_FORMINIT const *init);
	W_FULLSCREENOVERLAY_CLICKFORM();
public:
	static std::shared_ptr<W_FULLSCREENOVERLAY_CLICKFORM> make(UDWORD formID = 0);
	void clicked(W_CONTEXT *psContext, WIDGET_KEY key) override;
	void display(int xOffset, int yOffset) override;
	void run(W_CONTEXT *psContext) override;

	void setCutoutWidget(std::shared_ptr<WIDGET> widget)
	{
		cutoutWidget = widget;
	}

public:
	PIELIGHT backgroundColor = pal_RGBA(0, 0, 0, 125);
	std::function<void ()> onClickedFunc;
	std::function<void ()> onCancelPressed;
protected:
	std::weak_ptr<WIDGET> cutoutWidget;
};

void displayChildDropShadows(WIDGET *psWidget, UDWORD xOffset, UDWORD yOffset);

#endif // __INCLUDED_LIB_WIDGET_FORM_H__
