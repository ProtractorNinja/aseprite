// Aseprite UI Library
// Copyright (C) 2001-2013, 2015  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "base/unique_ptr.h"
#include "gfx/size.h"
#include "ui/graphics.h"
#include "ui/intern.h"
#include "ui/paint_event.h"
#include "ui/preferred_size_event.h"
#include "ui/ui.h"

#include <string>

static const int kTooltipDelayMsecs = 300;

namespace ui {

using namespace gfx;

TooltipManager::TooltipManager()
  : Widget(kGenericWidget)
{
  Manager* manager = Manager::getDefault();
  manager->addMessageFilter(kMouseEnterMessage, this);
  manager->addMessageFilter(kKeyDownMessage, this);
  manager->addMessageFilter(kMouseDownMessage, this);
  manager->addMessageFilter(kMouseLeaveMessage, this);

  setVisible(false);
}

TooltipManager::~TooltipManager()
{
  Manager* manager = Manager::getDefault();
  manager->removeMessageFilterFor(this);
}

void TooltipManager::addTooltipFor(Widget* widget, const std::string& text, int arrowAlign)
{
  m_tips[widget] = TipInfo(text, arrowAlign);
}

void TooltipManager::removeTooltipFor(Widget* widget)
{
  auto it = m_tips.find(widget);
  if (it != m_tips.end())
    m_tips.erase(it);
}

bool TooltipManager::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kMouseEnterMessage: {
      UI_FOREACH_WIDGET(msg->recipients(), itWidget) {
        Tips::iterator it = m_tips.find(*itWidget);
        if (it != m_tips.end()) {
          m_target.widget = it->first;
          m_target.tipInfo = it->second;

          if (m_timer == NULL) {
            m_timer.reset(new Timer(kTooltipDelayMsecs, this));
            m_timer->Tick.connect(&TooltipManager::onTick, this);
          }

          m_timer->start();
        }
      }
      return false;
    }

    case kKeyDownMessage:
    case kMouseDownMessage:
    case kMouseLeaveMessage:
      if (m_tipWindow) {
        m_tipWindow->closeWindow(NULL);
        m_tipWindow.reset();
      }

      if (m_timer)
        m_timer->stop();

      return false;
  }

  return Widget::onProcessMessage(msg);
}

void TooltipManager::onTick()
{
  if (!m_tipWindow) {
    gfx::Rect bounds = m_target.widget->getBounds();

    m_tipWindow.reset(new TipWindow(m_target.tipInfo.text.c_str(), bounds));
    int x = get_mouse_position().x+12*guiscale();
    int y = get_mouse_position().y+12*guiscale();

    m_tipWindow->remapWindow();
    int w = m_tipWindow->getBounds().w;
    int h = m_tipWindow->getBounds().h;

    int arrowAlign = m_target.tipInfo.arrowAlign;
    int trycount = 0;
    for (; trycount < 4; ++trycount) {
      switch (arrowAlign) {
        case JI_TOP | JI_LEFT:
          x = bounds.x + bounds.w;
          y = bounds.y + bounds.h;
          break;
        case JI_TOP | JI_RIGHT:
          x = bounds.x - w;
          y = bounds.y + bounds.h;
          break;
        case JI_BOTTOM | JI_LEFT:
          x = bounds.x + bounds.w;
          y = bounds.y - h;
          break;
        case JI_BOTTOM | JI_RIGHT:
          x = bounds.x - w;
          y = bounds.y - h;
          break;
        case JI_TOP:
          x = bounds.x + bounds.w/2 - w/2;
          y = bounds.y + bounds.h;
          break;
        case JI_BOTTOM:
          x = bounds.x + bounds.w/2 - w/2;
          y = bounds.y - h;
          break;
        case JI_LEFT:
          x = bounds.x + bounds.w;
          y = bounds.y + bounds.h/2 - h/2;
          break;
        case JI_RIGHT:
          x = bounds.x - w;
          y = bounds.y + bounds.h/2 - h/2;
          break;
      }

      x = MID(0, x, ui::display_w()-w);
      y = MID(0, y, ui::display_h()-h);

      if (bounds.intersects(gfx::Rect(x, y, w, h))) {
        switch (trycount) {
          case 0:
          case 2:
            // Switch position
            if (arrowAlign & (JI_TOP | JI_BOTTOM)) arrowAlign ^= JI_TOP | JI_BOTTOM;
            if (arrowAlign & (JI_LEFT | JI_RIGHT)) arrowAlign ^= JI_LEFT | JI_RIGHT;
            break;
          case 1:
            // Rotate positions
            if (arrowAlign & (JI_TOP | JI_LEFT)) arrowAlign ^= JI_TOP | JI_LEFT;
            if (arrowAlign & (JI_BOTTOM | JI_RIGHT)) arrowAlign ^= JI_BOTTOM | JI_RIGHT;
            break;
        }
      }
      else {
        m_tipWindow->setArrowAlign(arrowAlign);
        m_tipWindow->positionWindow(x, y);
        m_tipWindow->openWindow();
        break;
      }
    }

    // No enough room for the tooltip
    if (trycount == 4) {
      m_tipWindow.reset();
      m_timer->stop();
    }
  }
  m_timer->stop();
}

// TipWindow

TipWindow::TipWindow(const char* text, const gfx::Rect& target)
  : PopupWindow(text, kCloseOnClickInOtherWindow)
  , m_arrowAlign(0)
  , m_target(target)
{
  setTransparent(true);

  makeFixed();
  initTheme();
}

TipWindow::~TipWindow()
{
}

int TipWindow::getArrowAlign() const
{
  return m_arrowAlign;
}

void TipWindow::setArrowAlign(int arrowAlign)
{
  m_arrowAlign = arrowAlign;
}

bool TipWindow::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kKeyDownMessage:
      if (static_cast<KeyMessage*>(msg)->scancode() < kKeyFirstModifierScancode)
        closeWindow(NULL);
      break;

  }

  return PopupWindow::onProcessMessage(msg);
}

void TipWindow::onPreferredSize(PreferredSizeEvent& ev)
{
  ScreenGraphics g;
  g.setFont(getFont());
  Size resultSize = g.fitString(getText(),
                                (getClientBounds() - getBorder()).w,
                                getAlign());

  resultSize.w += this->border_width.l + this->border_width.r;
  resultSize.h += this->border_width.t + this->border_width.b;

  if (!getChildren().empty()) {
    Size maxSize(0, 0);
    Size reqSize;

    UI_FOREACH_WIDGET(getChildren(), it) {
      Widget* child = *it;

      reqSize = child->getPreferredSize();

      maxSize.w = MAX(maxSize.w, reqSize.w);
      maxSize.h = MAX(maxSize.h, reqSize.h);
    }

    resultSize.w = MAX(resultSize.w, border_width.l + maxSize.w + border_width.r);
    resultSize.h += maxSize.h;
  }

  ev.setPreferredSize(resultSize);
}

void TipWindow::onInitTheme(InitThemeEvent& ev)
{
  Window::onInitTheme(ev);

  this->border_width.l = 6 * guiscale();
  this->border_width.t = 6 * guiscale();
  this->border_width.r = 6 * guiscale();
  this->border_width.b = 7 * guiscale();

  // Setup the background color.
  setBgColor(gfx::rgba(255, 255, 200));
}

void TipWindow::onPaint(PaintEvent& ev)
{
  getTheme()->paintTooltip(ev);
}

} // namespace ui
