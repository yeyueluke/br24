/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Navico BR24 Radar Plugin
 * Authors:  David Register
 *           Dave Cowell
 *           Kees Verruijt
 *           Douwe Fokkema
 *           Sean D'Epagnier
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register              bdbcat@yahoo.com *
 *   Copyright (C) 2012-2013 by Dave Cowell                                *
 *   Copyright (C) 2012-2016 by Kees Verruijt         canboat@verruijt.net *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#include "RadarCanvas.h"
#include "drawutil.h"
#include "TextureFont.h"

PLUGIN_BEGIN_NAMESPACE

BEGIN_EVENT_TABLE(RadarCanvas, wxGLCanvas)
//    EVT_CLOSE(RadarCanvas::close)
EVT_MOVE(RadarCanvas::OnMove)
EVT_SIZE(RadarCanvas::OnSize)
EVT_PAINT(RadarCanvas::Render)
EVT_LEFT_DOWN(RadarCanvas::OnMouseClick)
END_EVENT_TABLE()

static int attribs[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, WX_GL_STENCIL_SIZE, 8, 0};

RadarCanvas::RadarCanvas(br24radar_pi *pi, RadarInfo *ri, wxWindow *parent, wxSize size)
    : wxGLCanvas(parent, wxID_ANY, attribs, wxDefaultPosition, size, wxFULL_REPAINT_ON_RESIZE | wxBG_STYLE_CUSTOM, _T("")) {
  m_parent = parent;
  m_pi = pi;
  m_ri = ri;
  m_context = new wxGLContext(this);
  m_zero_context = new wxGLContext(this);
  LOG_DIALOG(wxT("BR24radar_pi: %s create OpenGL canvas"), m_ri->name.c_str());
}

RadarCanvas::~RadarCanvas() {
  LOG_DIALOG(wxT("BR24radar_pi: %s destroy OpenGL canvas"), m_ri->name.c_str());
  delete m_context;
  delete m_zero_context;
}

void RadarCanvas::OnSize(wxSizeEvent &evt) {
  wxSize parentSize = m_parent->GetSize();
  LOG_DIALOG(wxT("BR24radar_pi: %s resize OpenGL canvas to %d, %d"), m_ri->name.c_str(), parentSize.x, parentSize.y);
  Refresh(false);
  if (GetSize() != parentSize) {
    SetSize(parentSize);
  }
}

void RadarCanvas::OnMove(wxMoveEvent &evt) {
  wxPoint pos = m_parent->GetPosition();
  LOG_DIALOG(wxT("BR24radar_pi: %s move OpenGL canvas to %d, %d"), m_ri->name.c_str(), pos.x, pos.y);
}

void RadarCanvas::RenderTexts(int w, int h) {
  int x, y;

  wxString s;

  s = m_ri->GetCanvasTextTopLeft();
  m_FontBig.RenderString(s, 0, 0);

  s = m_ri->GetCanvasTextBottomLeft();
  if (s.length()) {
    m_FontBig.GetTextExtent(s, &x, &y);
    m_FontBig.RenderString(s, 0, h - y);
  }

  s = m_ri->GetCanvasTextCenter();
  if (s.length()) {
    m_FontBig.GetTextExtent(s, &x, &y);
    m_FontBig.RenderString(s, (w - x) / 2, (h - y) / 2);
  }
}

void RadarCanvas::RenderRangeRingsAndHeading(int w, int h) {
  // Max range ringe
  float r = wxMax(w, h) / 2.0;

  // Position of the range texts
  float x = sinf((float)(0.25 * PI)) * r * 0.25;
  float y = cosf((float)(0.25 * PI)) * r * 0.25;
  float center_x = w / 2.0;
  float center_y = h / 2.0;

  // Size of rendered string in pixels
  int px;
  int py;

  glColor3ub(0, 126, 29); // same color as HDS
  glLineWidth(1.0);

  for (int i = 1; i <= 4; i++) {
    DrawArc(w / 2.0, h / 2.0, r * i * 0.25, 0.0, 2.0 * (float)PI, 360);
    const char *s = m_ri->GetDisplayRangeStr(i - 1);
    if (s) {
      m_FontNormal.RenderString(wxString::Format(wxT("%s"), s), center_x + x * (float)i, center_y + y * (float)i);
    }
  }

  LOG_DIALOG(wxT("BR24radar_pi: m_hdt=%f rot=%d"), m_pi->m_hdt, m_ri->rotation.value);
  double rot = (m_ri->rotation.value && m_pi->m_heading_source != HEADING_NONE) ? m_pi->m_hdt : 0.0;

  for (int i = 0; i < 360; i += 5) {
    x = -sinf(deg2rad(i + rot)) * (r * 1.00 - 1);
    y = cosf(deg2rad(i + rot)) * (r * 1.00 - 1);

    wxString s;
    if (i % 90 == 0) {
      static char nesw[4] = {'N', 'E', 'S', 'W'};
      s = wxString::Format(wxT("%c"), nesw[i / 90]);
    } else if (i % 15 == 0) {
      s = wxString::Format(wxT("%u"), i);
    }
    m_FontNormal.GetTextExtent(s, &px, &py);
    if (x > 0) {
      x -= px;
    }
    if (y > 0) {
      y -= py;
    }
    m_FontNormal.RenderString(s, center_x + x, center_y + y);
  }
}

void RadarCanvas::RenderLollipop(int w, int h) {
  static const double LOLLIPOP_SIZE = 20.0;

  if ((m_ri->m_mouse_lat == 0.0 && m_ri->m_mouse_lon == 0.0) || !m_pi->m_bpos_set) {
    return;
  }
  // Can't compute this upfront, ownship may move...
  double distance = local_distance(m_pi->m_ownship_lat, m_pi->m_ownship_lon, m_ri->m_mouse_lat, m_ri->m_mouse_lon) * 1852.;
  double bearing = local_bearing(m_pi->m_ownship_lat, m_pi->m_ownship_lon, m_ri->m_mouse_lat, m_ri->m_mouse_lon);
  double full_range = wxMax(w, h) / 2.0;

  double rot = (m_ri->rotation.value && m_pi->m_heading_source != HEADING_NONE) ? m_pi->m_hdt : 0.0;
  int display_range = m_ri->GetDisplayRange();
  double scale = distance * full_range / display_range;

  double center_x = w / 2.0;
  double center_y = h / 2.0;
  double angle = deg2rad(bearing - rot);
  double x = center_x - sin(angle) * scale;
  double y = center_y + cos(angle) * scale;
  double l_x = x + sin(angle) * LOLLIPOP_SIZE;
  double l_y = y - cos(angle) * LOLLIPOP_SIZE;

  glColor3ub(150, 150, 150);
  glLineWidth(1.0);

  glBegin(GL_LINES);
  glVertex2f(center_x, center_y);
  glVertex2f(l_x, l_y);
  glEnd();

  DrawArc(x, y, LOLLIPOP_SIZE, 0.0, 2.0 * (float)PI, 36);
}

void RadarCanvas::Render_EBL_VRM(int w, int h) {

  static const uint8_t rgb[BEARING_LINES][3] = {
    { 22, 129, 154 }
    , { 45, 255, 254 }
  };

  double full_range = wxMax(w, h) / 2.0;
  double center_x = w / 2.0;
  double center_y = h / 2.0;

  double rot = (m_ri->rotation.value && m_pi->m_heading_source != HEADING_NONE) ? m_pi->m_hdt : 0.0;
  int display_range = m_ri->GetDisplayRange();

  for (int b = 0; b < BEARING_LINES; b++)
  {
    if (m_ri->m_vrm[b] != 0.0) {
      double scale = m_ri->m_vrm[b] * 1852.0 * full_range / display_range;
      double angle = deg2rad(m_ri->m_ebl[b] - rot);
      double x = center_x - sin(angle) * full_range;
      double y = center_y + cos(angle) * full_range;

      glColor3ub(rgb[b][0], rgb[b][1], rgb[b][2]);
      glLineWidth(1.0);

      glBegin(GL_LINES);
      glVertex2f(center_x, center_y);
      glVertex2f(x, y);
      glEnd();

      DrawArc(center_x, center_y, scale, 0.0, 2.0 * (float)PI, 360);
    }
  }
}

void RadarCanvas::Render(wxPaintEvent &evt) {
  int w, h;

  if (!IsShown() || !m_pi->m_initialized) {
    return;
  }

  GetClientSize(&w, &h);
  wxPaintDC(this);  // only to be used in paint events. use wxClientDC to paint
                    // outside the paint event

  if (!m_pi->m_opengl_mode) {
    LOG_DIALOG(wxT("BR24radar_pi: %s cannot render non-OpenGL mode"), m_ri->name.c_str());
    return;
  }
  if (!m_pi->m_opencpn_gl_context && !m_pi->m_opencpn_gl_context_broken) {
    LOG_DIALOG(wxT("BR24radar_pi: %s skip render as no context known yet"), m_ri->name.c_str());
    return;
  }
  LOG_DIALOG(wxT("BR24radar_pi: %s render OpenGL canvas %d by %d "), m_ri->name.c_str(), w, h);

  SetCurrent(*m_context);

  glPushMatrix();
  glPushAttrib(GL_ALL_ATTRIB_BITS);

  wxFont font(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
  m_FontNormal.Build(font);
  wxFont bigFont(24, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
  m_FontBig.Build(bigFont);

  glClearColor(0.0f, 0.1f, 0.0f, 1.0f);                // Black Background
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // Clear the canvas
  glEnable(GL_TEXTURE_2D);                             // Enable textures
  glEnable(GL_COLOR_MATERIAL);
  glEnable(GL_BLEND);
  // glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glViewport(0, 0, w, h);

  glMatrixMode(GL_PROJECTION);  // Next two operations on the project matrix stack
  glLoadIdentity();             // Reset projection matrix stack
  glOrtho(0, w, h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);  // Reset matrick stack target back to GL_MODELVIEW

  glEnable(GL_TEXTURE_2D);

  RenderRangeRingsAndHeading(w, h);
  RenderLollipop(w, h);
  Render_EBL_VRM(w, h);

  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);  // Next two operations on the project matrix stack
  glLoadIdentity();             // Reset projection matrix stack
  if (w >= h) {
    glScaled(1.0, (float)-w / h, 1.0);
  } else {
    glScaled((float)h / w, -1.0, 1.0);
  }
  glMatrixMode(GL_MODELVIEW);  // Reset matrick stack target back to GL_MODELVIEW

  // CheckOpenGLError(wxT("range circles"));

  m_ri->RenderRadarImage(wxPoint(0, 0), 1.0, 0.0, false);

  glViewport(0, 0, w, h);

  glMatrixMode(GL_PROJECTION);  // Next two operations on the project matrix stack
  glLoadIdentity();             // Reset projection matrix stack
  glOrtho(0, w, h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);  // Reset matrick stack target back to GL_MODELVIEW

  glColor3ub(200, 255, 200);
  glEnable(GL_TEXTURE_2D);

  RenderTexts(w, h);

  glDisable(GL_TEXTURE_2D);

  glMatrixMode(GL_PROJECTION);  // Next two operations on the project matrix stack
  glLoadIdentity();             // Reset projection matrix stack
  glMatrixMode(GL_MODELVIEW);   // Reset matrick stack target back to GL_MODELVIEW

  glPopAttrib();
  glPopMatrix();
  glFlush();
  glFinish();
  SwapBuffers();

  if (m_pi->m_opencpn_gl_context) {
    SetCurrent(*m_pi->m_opencpn_gl_context);
  } else {
    SetCurrent(*m_zero_context);  // Make sure OpenCPN -at least- doesn't overwrite our context info
  }
}

void RadarCanvas::OnMouseClick(wxMouseEvent &event) {
  LOG_DIALOG(wxT("BR24radar_pi: %s Mouse clicked"), m_ri->name.c_str());
  m_pi->ShowRadarControl(m_ri->radar, true);
  event.Skip();
}

PLUGIN_END_NAMESPACE
