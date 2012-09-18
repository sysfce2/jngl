/*
Copyright 2007-2012 Jan Niklas Hasse <jhasse@gmail.com>
For conditions of distribution and use, see copyright notice in LICENSE.txt
*/

#pragma once

#include "jngl.hpp"
#include "freetype.hpp"
#include "opengl.hpp"

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/type_traits.hpp>
#include <boost/array.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/noncopyable.hpp>
#include <map>
#include <stack>

#ifdef __linux
	#ifdef WIZ

	#else
		#include <GL/glx.h>
		#include <GL/glu.h>
		#include <X11/extensions/xf86vmode.h>
		#include <X11/keysym.h>
	#endif
#else
    #ifdef _WIN32
        #include <windows.h>
    #endif
#endif

namespace jngl
{
	class WindowImpl;

	bool Init(const int width, const int height); // Definied in main.cpp

	class Window : boost::noncopyable {
	public:
		Window(const std::string& title, const int width, const int height, const bool fullscreen);
		~Window();
		bool Running();
		void Quit();
		void Continue();
		void UpdateInput();
		void UpdateKeyStates();
		void SwapBuffers();
		void SetRelativeMouseMode(bool relative);
		bool GetRelativeMouseMode() const;
		void SetMouseVisible(bool visible);
		bool GetMouseVisible() const;
		int MouseX();
		int MouseY();
		int GetWidth() const;
		int GetHeight() const;
		double GetTextWidth(const std::string&);
		int getLineHeight();
		void setLineHeight(int);
		bool GetFullscreen() const;
		bool getKeyDown(const key::KeyType key);
		bool getKeyPressed(const key::KeyType key);
		void setKeyPressed(const key::KeyType key, bool);
		bool getKeyDown(const std::string& key);
		bool getKeyPressed(const std::string& key);
		void setKeyPressed(const std::string& key, bool);
		bool MouseDown(mouse::Button button);
		bool getMousePressed(mouse::Button button);
		void setMousePressed(mouse::Button button, bool);
		void SetMouse(const int xposition, const int yposition);
		void SetTitle(const std::string& title);
		void Print(const std::string& text, const int xposition, const int yposition);
		int GetFontSize() const;
		void SetFontSize(const int size);
		void SetFont(const std::string&);
		void SetFontByName(const std::string&);
		bool IsMultisampleSupported() const;
		void SetIcon(const std::string&);
		double GetMouseWheel() const;
		std::string GetFont() const;
		void SetWork(boost::shared_ptr<Work>);
		void MainLoop();
		bool stepIfNeeded();
		void draw() const;
		boost::shared_ptr<Work> getWork();
		void setConfigPath(const std::string&);
		std::string getConfigPath() const;
#ifndef __APPLE__
	#ifdef __linux
		boost::shared_ptr<Display> pDisplay_;
		static void ReleaseXData(void*);
	#else
		static void ReleaseDC(HWND, HDC);
		static void ReleaseRC(HGLRC);
	#endif
#else
		WindowImpl* getImpl() const;
#endif
	private:
		int GetKeyCode(jngl::key::KeyType key);
		std::string GetFontFileByName(const std::string& fontname);

		bool fullscreen_, running_, isMouseVisible_, relativeMouseMode, isMultisampleSupported_, anyKeyPressed_;
		boost::array<bool, 3> mouseDown_;
		boost::array<bool, 3> mousePressed_;
		std::map<unsigned int, bool> keyDown_;
		std::map<unsigned int, bool> keyPressed_;
		std::map<std::string, bool> characterDown_;
		std::map<std::string, bool> characterPressed_;
		std::stack<bool*> needToBeSetFalse_;
		int mousex_, mousey_, fontSize_, width_, height_;
		double mouseWheel_;
		std::string fontName_;
		const static unsigned int PNG_BYTES_TO_CHECK = 4;
		double oldTime_;
		bool needDraw_;
		boost::shared_ptr<Work> currentWork_;
		bool changeWork_;
		boost::shared_ptr<Work> newWork_;
		std::string configPath;

		// <fontSize, <fontName, Font> >
		boost::ptr_map<int, boost::ptr_map<std::string, Font> > fonts_;
#ifndef __APPLE__
#ifdef WIZ
		NativeWindowType nativeWindow_;
		EGLDisplay display_;
		EGLContext context_;
		EGLSurface surface_;
		std::map<unsigned int, double> keyTimeout_;
#else
	#ifdef __linux
		::Window window_;
		GLXContext context_;
		int screen_;
		XF86VidModeModeInfo oldMode_;
	#else
		boost::shared_ptr<boost::remove_pointer<HGLRC>::type> pRenderingContext_;
		boost::shared_ptr<boost::remove_pointer<HWND>::type> pWindowHandle_;
		boost::shared_ptr<boost::remove_pointer<HDC>::type> pDeviceContext_;
		int arbMultisampleFormat_;

		bool InitMultisample(HINSTANCE, PIXELFORMATDESCRIPTOR);
		void Init(const std::string& title, bool multisample);
		void DistinguishLeftRight();
	#endif
#endif
#else
		WindowImpl* const impl;
		
		friend class WindowImpl;
#endif
	};
}
