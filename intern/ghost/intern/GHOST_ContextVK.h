/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 */

#ifndef __GHOST_CONTEXTVK_H__
#define __GHOST_CONTEXTVK_H__

#include "GHOST_Context.h"

#ifdef _WIN32
#  include "GHOST_SystemWin32.h"
#else
#  include "GHOST_SystemX11.h"
#endif

#include <vector>
#include <vulkan/vulkan.h>

#ifndef GHOST_OPENGL_VK_CONTEXT_FLAGS
/* leave as convenience define for the future */
#  define GHOST_OPENGL_VK_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_VK_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_VK_RESET_NOTIFICATION_STRATEGY 0
#endif

class GHOST_ContextVK : public GHOST_Context {
  /* XR code needs low level graphics data to send to OpenXR. */
  // friend class GHOST_XrGraphicsBindingOpenGL;

 public:
  /**
   * Constructor.
   */
  GHOST_ContextVK(bool stereoVisual,
#ifdef _WIN32
                  HWND hwnd,
#else
                  Window window,
                  Display *display,
#endif
                  int contextMajorVersion,
                  int contextMinorVersion,
                  int useValidationLayers);

  /**
   * Destructor.
   */
  ~GHOST_ContextVK();

  /**
   * Swaps front and back buffers of a window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers();

  /**
   * Activates the drawing context of this window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext();

  /**
   * Release the drawing context of the calling thread.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess releaseDrawingContext();

  /**
   * Call immediately after new to initialize.  If this fails then immediately delete the object.
   * \return Indication as to whether initialization has succeeded.
   */
  GHOST_TSuccess initializeDrawingContext();

  /**
   * Removes references to native handles from this context and then returns
   * \return GHOST_kSuccess if it is OK for the parent to release the handles and
   * GHOST_kFailure if releasing the handles will interfere with sharing
   */
  GHOST_TSuccess releaseNativeHandles();

  /**
   * Sets the swap interval for swapBuffers.
   * \param interval The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int interval);

  /**
   * Gets the current swap interval for swapBuffers.
   * \param intervalOut Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &intervalOut);

 private:
#ifdef _WIN32
  HWND hwnd;
#else
  Display *m_display;
  Window m_window;
#endif

  const int m_contextMajorVersion;
  const int m_contextMinorVersion;
  const int m_useValidationLayers;

  VkInstance m_instance;
  VkPhysicalDevice m_physical_device;
  VkDevice m_device;

  uint32_t m_queue_family_graphic;
  uint32_t m_queue_family_present;

  VkQueue m_graphic_queue;
  VkQueue m_present_queue;

  /* For display only. */
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain;
  std::vector<VkImage> m_swapChainImages;
  VkFormat m_swapChainImageFormat;
  VkExtent2D m_swapChainExtent;

  GHOST_TSuccess createSwapChain(void);
};

#endif  // __GHOST_CONTEXTVK_H__