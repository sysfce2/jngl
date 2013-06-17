/*
Copyright 2007-2012 Jan Niklas Hasse <jhasse@gmail.com>
For conditions of distribution and use, see copyright notice in LICENSE.txt
*/

#include "../window.hpp"
#include "../jngl/debug.hpp"
#include "../opengl.hpp"
#include "../finally.hpp"

#include <GL/glx.h>
#include <X11/extensions/xf86vmode.h>
#include <X11/keysym.h>
#include <stdexcept>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/assign/ptr_map_inserter.hpp>
#include <fontconfig/fontconfig.h>

namespace jngl
{
	// attributes for a double buffered visual in RGBA format with at least
	// 8 bits per color and a 16 bit depth buffer
	static int attrListDbl[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
	    GLX_RED_SIZE, 8,
	    GLX_GREEN_SIZE, 8,
	    GLX_BLUE_SIZE, 8,
	    GLX_ALPHA_SIZE, 8,
	    GLX_DEPTH_SIZE, 16,
	    None };

	// attributes are the same as above except that this list also requests
	// Anti-Aliasing.
	static int attrListAntiAliasing[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
	    GLX_RED_SIZE, 8,
	    GLX_GREEN_SIZE, 8,
	    GLX_BLUE_SIZE, 8,
	    GLX_ALPHA_SIZE, 8,
	    GLX_DEPTH_SIZE, 16,
	    GLX_SAMPLE_BUFFERS_ARB, 1,
	    GLX_SAMPLES_ARB, 4,
	    None };

	void Window::ReleaseXData(void* data)
	{
		if(data)
			XFree(data);
	}

	Window::Window(const std::string& title, const int width, const int height, const bool fullscreen)
		: fullscreen_(fullscreen), running_(false), isMouseVisible_(true), relativeMouseMode(false),
		  isMultisampleSupported_(true), anyKeyPressed_(false), mousex_(0), mousey_(0), fontSize_(12),
		  width_(width), height_(height), mouseWheel_(0), fontName_(""), oldTime(0), changeWork_(false)
	{
		mouseDown_.assign(false);
		mousePressed_.assign(false);
	    Colormap cmap;
	    int dpyWidth, dpyHeight;
	    XF86VidModeModeInfo **modes;
	    int modeNum;
	    XSetWindowAttributes attributes;

		pDisplay_.reset(XOpenDisplay(0), XCloseDisplay);
		if(!pDisplay_ )
			throw std::runtime_error("Could not open display.");
	    screen_ = DefaultScreen(pDisplay_.get());

		std::shared_ptr<XVisualInfo> pVisual(glXChooseVisual(pDisplay_.get(), screen_, attrListAntiAliasing), ReleaseXData);
		if(!pVisual)
		{
			isMultisampleSupported_ = false;
			pVisual.reset(glXChooseVisual(pDisplay_.get(), screen_, attrListDbl), ReleaseXData);
			if(!pVisual)
			{
				throw std::runtime_error("Couldn't create GLX visual.");
			}
		}

	    /* create a GLX context */
	    context_ = glXCreateContext(pDisplay_.get(), pVisual.get(), 0, GL_TRUE);
	    /* create a color map */
	    cmap = XCreateColormap(pDisplay_.get(), RootWindow(pDisplay_.get(), pVisual->screen),
	        pVisual->visual, AllocNone);
	    attributes.colormap = cmap;
	    attributes.border_pixel = 0;

	    if(fullscreen_)
	    {
		    XF86VidModeGetAllModeLines(pDisplay_.get(), screen_, &modeNum, &modes);
		    /* save desktop-resolution before switching modes */
			oldMode_ = *modes[0];
			int bestMode = -1;
		    /* look for mode with requested resolution */
		    for(int i = 0; i < modeNum; ++i)
		    {
		        if ((modes[i]->hdisplay == width) && (modes[i]->vdisplay == height))
		        {
		            bestMode = i;
		        }
		    }
			if(bestMode == -1)
				throw std::runtime_error("The requested fullscreen mode is not supported by your video card.");
	        XF86VidModeSwitchToMode(pDisplay_.get(), screen_, modes[bestMode]);
	        XF86VidModeSetViewPort(pDisplay_.get(), screen_, 0, 0);
	        dpyWidth = modes[bestMode]->hdisplay;
	        dpyHeight = modes[bestMode]->vdisplay;
	        XFree(modes);

	        attributes.override_redirect = True;
	        attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask;
	        window_ = XCreateWindow(pDisplay_.get(), RootWindow(pDisplay_.get(), pVisual->screen),
	            0, 0, dpyWidth, dpyHeight, 0, pVisual->depth, InputOutput, pVisual->visual,
	            CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect,
	            &attributes);
			XMapRaised(pDisplay_.get()	, window_);
	        XGrabKeyboard(pDisplay_.get(), window_, true, GrabModeAsync,
	            GrabModeAsync, CurrentTime);
	        XGrabPointer(pDisplay_.get(), window_, true, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
	            GrabModeAsync, GrabModeAsync, window_, None, CurrentTime);
	    }
	    else
	    {
	        /* create a window in window mode*/
			attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
	        window_ = (XCreateWindow(pDisplay_.get(), RootWindow(pDisplay_.get(), pVisual->screen),
	            0, 0, width, height, 0, pVisual->depth, InputOutput, pVisual->visual,
	            CWBorderPixel | CWColormap | CWEventMask, &attributes));

			/* pointer to the size hints structure. */
			XSizeHints* win_size_hints = XAllocSizeHints();
			if (!win_size_hints)
				throw std::runtime_error("XAllocSizeHints out of memory");

			/* initialize the structure appropriately. */
			/* first, specify which size hints we want to fill in. */
			win_size_hints->flags = PMinSize | PMaxSize;
			win_size_hints->min_width = width;
			win_size_hints->min_height = height;
			win_size_hints->base_width = width;
			win_size_hints->base_height = height;

			/* pass the size hints to the window manager. */
			XSetWMNormalHints(pDisplay_.get(), window_, win_size_hints);

			/* finally, we can free the size hints structure. */
			XFree(win_size_hints);

			XMapRaised(pDisplay_.get(), window_);
		}

		Atom wmDelete = XInternAtom(pDisplay_.get(), "WM_DELETE_WINDOW", true);
		XSetWMProtocols(pDisplay_.get(), window_, &wmDelete, 1);

		SetTitle(title);

		glXMakeCurrent(pDisplay_.get(), window_, context_);

		SetFontByName("Arial"); // Default font
		SetFontSize(fontSize_); // Load a font the first time

		Init(width, height);

		running_ = true;
	}

	std::string Window::GetFontFileByName(const std::string& fontname)
	{
		const double size = 12;
		FcFontSet* fontSet = FcFontSetCreate();

		FcPattern* pattern = FcPatternBuild(NULL, FC_FAMILY, FcTypeString, fontname.c_str(), FC_SIZE, FcTypeDouble, size, NULL);
		FcConfigSubstitute(NULL,pattern, FcMatchPattern);
		FcDefaultSubstitute(pattern);

		FcResult result;
		FcPattern* match = FcFontMatch(NULL, pattern, &result);
		FcPatternDestroy(pattern);

		if(!match)
		{
			FcFontSetDestroy(fontSet);
			throw std::runtime_error(std::string("Couldn't load " + fontname));
		}
		FcFontSetAdd(fontSet, match);

		if(fontSet->nfont == 0)
		{
			throw std::runtime_error(std::string("Couldn't load " + fontname));
		}

		FcChar8* filename = NULL;
		FcPatternGetString(fontSet->fonts[0], FC_FILE, 0, &filename);
		return (const char*)filename;
	}

	Window::~Window()
	{
	    if(context_)
	    {
	        if(!glXMakeCurrent(pDisplay_.get(), None, NULL))
	        {
	            printf("Could not release drawing context.\n");
	        }
	        glXDestroyContext(pDisplay_.get(), context_);
	    }
	    if(fullscreen_)
	    {
	        XF86VidModeSwitchToMode(pDisplay_.get(), screen_, &oldMode_);
	        XF86VidModeSetViewPort(pDisplay_.get(), screen_, 0, 0);
	    }
	}

	int GetKeysym(key::KeyType key)
	{
		switch(key)
		{
			case key::Left: return 0xff51;
			case key::Up: return 0xff52;
			case key::Right: return 0xff53;
			case key::Down: return 0xff54;
			case key::PageUp: return 0xff55;
			case key::PageDown: return 0xff56;
			case key::Home: return 0xff50;
			case key::End: return 0xff57;
			case key::BackSpace: return 0xff08;
			case key::Tab: return 0xff09;
			case key::Clear: return 0xff0b;
			case key::Return: return 0xff0d;
			case key::Pause: return 0xff13;
			case key::Escape: return 0xff1b;
			case key::Delete: return 0xffff;
			case key::ControlL: return 0xffe3;
			case key::ControlR: return 0xffe4;
			case key::CapsLock: return 0xffe5;
			case key::AltL: return 0xffe9;
			case key::AltR: return 0xffea;
			case key::SuperL: return 0xffeb;
			case key::SuperR: return 0xffec;
			case key::Space: return 0x0020;
			case key::ShiftL: return 0xffe1;
			case key::ShiftR: return 0xffe2;
			case key::F1: return 0xffbe;
			case key::F2: return 0xffbf;
			case key::F3: return 0xffc0;
			case key::F4: return 0xffc1;
			case key::F5: return 0xffc2;
			case key::F6: return 0xffc3;
			case key::F7: return 0xffc4;
			case key::F8: return 0xffc5;
			case key::F9: return 0xffc6;
			case key::F10: return 0xffc7;
			case key::F11: return 0xffc8;
			case key::F12: return 0xffc9;
			default:
				return 0;
		};
	}

	int Window::GetKeyCode(key::KeyType key)
	{
		return XKeysymToKeycode(pDisplay_.get(), GetKeysym(key));
	}

	int CharToSym(char c)
	{
		static struct {char from; KeySym to;} CtoKSTable[]={
		   {' ', XK_space},
		   {'!', XK_exclam},
		   {'"', XK_quotedbl},
		   {'#', XK_numbersign},
		   {'$', XK_dollar},
		   {'%', XK_percent},
		   {'&', XK_ampersand},
		   {'\'',XK_apostrophe},
		   {'(', XK_parenleft},
		   {')', XK_parenright},
		   {'*', XK_asterisk},
		   {'+', XK_plus},
		   {',', XK_comma},
		   {'-', XK_minus},
		   {'.', XK_period},
		   {'/', XK_slash},
		   {':', XK_colon},
		   {';', XK_semicolon},
		   {'<', XK_less},
		   {'=', XK_equal},
		   {'>', XK_greater},
		   {'?', XK_question},
		   {'@', XK_at},
		   {'[', XK_bracketleft},
		   {'\\',XK_backslash},
		   {']', XK_bracketright},
		   {'^', XK_asciicircum},
		   {'_', XK_underscore},
		   {'`', XK_grave},
		   {'{', XK_braceleft},
		   {'|', XK_bar},
		   {'}', XK_braceright},
		   {'~', XK_asciitilde},
		   {'\n',XK_Return},
		   {0,0}
		};
		if('A' <= c && c <= 'Z' && !(keyDown(key::ShiftL) || keyDown(key::ShiftR)))
		{
			return NoSymbol;
		}
		if('a' <= c && c <= 'z' && (keyDown(key::ShiftL) || keyDown(key::ShiftR)))
		{
			return NoSymbol;
		}
		char buf[2];
		buf[0]=c;
		buf[1]=0;
		KeySym ksym = XStringToKeysym(buf);
		if(ksym == NoSymbol)
		{
			for(int i = 0; CtoKSTable[i].from != 0; ++i)
			{
				if(CtoKSTable[i].from == c)
					return CtoKSTable[i].to;
			}
			return NoSymbol;
		}
		return ksym;
	}

	bool Window::getKeyDown(const std::string& key)
	{
		if(key.size() > 1) // No UTF-8 support yet
		{
			return false;
		}
		return keyDown_[XKeysymToKeycode(pDisplay_.get(), CharToSym(key[0]))];
	}

	bool Window::getKeyPressed(const std::string& key)
	{
		if(key.size() > 1) // No UTF-8 support yet
		{
			return false;
		}
		if(keyPressed_[XKeysymToKeycode(pDisplay_.get(), CharToSym(key[0]))])
		{
			keyPressed_[XKeysymToKeycode(pDisplay_.get(), CharToSym(key[0]))] = false;
			return true;
		}
		return false;
	}

	void Window::UpdateInput()
	{
		XEvent event;
		while (XPending(pDisplay_.get()) > 0)
		{
			XNextEvent(pDisplay_.get(), &event);
			switch(event.type)
			{
				case MotionNotify:
					mousex_ = event.xmotion.x;
					mousey_ = event.xmotion.y;
					if (relativeMouseMode) {
						SetMouse(width_ / 2, height_ / 2);
						mousex_ -= width_ / 2;
						mousey_ -= height_ / 2;
					}
				break;
				case KeyPress:
					keyDown_[event.xkey.keycode] = true;
					keyPressed_[event.xkey.keycode] = true;
					anyKeyPressed_ = true;
				break;
				case KeyRelease:
					keyDown_[event.xkey.keycode] = false;
					keyPressed_[event.xkey.keycode] = false;
				break;
				case ButtonPress:
					switch(event.xbutton.button)
					{
						case 1:
						case 2:
						case 3:
						{
							const unsigned int button = event.xbutton.button - 1;
							mouseDown_.at(button) = true;
							mousePressed_.at(button) = true;
						}
						break;
						case 4:
							mouseWheel_ += 1;
						break;
						case 5:
							mouseWheel_ -= 1;
						break;
					}
				break;
				case ButtonRelease:
					if(1 <= event.xbutton.button && event.xbutton.button <= 3)
					{
						const unsigned int button = event.xbutton.button - 1;
						mouseDown_.at(button) = false;
						mousePressed_.at(button) = false;
					}
				break;
				case ClientMessage:
					if(strcmp(XGetAtomName(pDisplay_.get(), event.xclient.message_type), "WM_PROTOCOLS") == 0)
						running_ = false;
				break;
			}
		}
	}

	void Window::SetRelativeMouseMode(bool relative) {
		relativeMouseMode = relative;
		SetMouseVisible(!relative);
		if (relative) {
			SetMouse(width_ / 2, height_ / 2);
			mousex_ = mousey_ = 0;
		}
	}

	void Window::SwapBuffers()
	{
		glXSwapBuffers(pDisplay_.get(), window_);
	}

	void Window::SetMouseVisible(const bool visible)
	{
		isMouseVisible_ = visible;
		if(visible)
		{
			XDefineCursor(pDisplay_.get(), window_, None);
		}
		else
		{
			// http://www.linuxforums.org/forum/linux-programming-scripting/59012-xlib-hide-mouse-pointer.html
			Pixmap bm_no;
			Colormap cmap;
			Cursor no_ptr;
			XColor black, dummy;
			static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
			cmap = DefaultColormap(pDisplay_.get(), screen_);
			XAllocNamedColor(pDisplay_.get(), cmap, "black", &black, &dummy);
			bm_no = XCreateBitmapFromData(pDisplay_.get(), window_, bm_no_data, 8, 8);
			no_ptr = XCreatePixmapCursor(pDisplay_.get(), bm_no, bm_no, &black, &black, 0, 0);
			XDefineCursor(pDisplay_.get(), window_, no_ptr);
			XFreeCursor(pDisplay_.get(), no_ptr);
			if (bm_no != None)
				XFreePixmap(pDisplay_.get(), bm_no);
			XFreeColors(pDisplay_.get(), cmap, &black.pixel, 1, 0);
		}
	}

	void Window::SetTitle(const std::string& windowTitle)
	{
		XTextProperty textProperty;
		char* cStr = const_cast<char*>(windowTitle.c_str());
		XStringListToTextProperty(&cStr, 1, &textProperty);
		XSetWMName(pDisplay_.get(), window_, &textProperty);
	}

	int Window::MouseX()
	{
		return mousex_;
	}

	int Window::MouseY()
	{
		return mousey_;
	}

	void Window::SetMouse(const int xposition, const int yposition)
	{
		XWarpPointer(pDisplay_.get(), None, window_, 0, 0, 0, 0, xposition, yposition);
	}

	void Window::SetIcon(const std::string&)
	{
		return; // TODO: Not implemented yet
	}

	int getDesktopWidth()
	{
		Display* display = XOpenDisplay(NULL);
		Finally finally(boost::bind(XCloseDisplay, display));
		return XDisplayWidth(display, XDefaultScreen(display));
	}

	int getDesktopHeight()
	{
		Display* display = XOpenDisplay(NULL);
		Finally finally(boost::bind(XCloseDisplay, display));
		return XDisplayHeight(display, XDefaultScreen(display));
	}
}