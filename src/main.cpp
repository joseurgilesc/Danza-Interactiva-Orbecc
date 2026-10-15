#include "ofMain.h"
#include "ofApp.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cstring>

//========================================================================
int main( ){

	// En Ubuntu 26.04 (sesión Wayland + XWayland), la auto-detección de GLFW
	// 3.4 a veces falla con "X11: Platform not initialized" y segfault.
	// Forzamos el backend. Por defecto X11 (xdpyinfo confirma que :0 responde);
	// se puede forzar wayland con:  OFX_GLFW_PLATFORM=wayland
	const char *p = std::getenv("OFX_GLFW_PLATFORM");
	if (p && std::strcmp(p, "wayland") == 0) {
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
	} else if (p && std::strcmp(p, "x11") == 0) {
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
	} else {
		glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
	}

	//Use ofGLFWWindowSettings for more options like multi-monitor fullscreen
	ofGLWindowSettings settings;
	settings.setSize(1280, 720);
	settings.windowMode = OF_WINDOW; //can also be OF_FULLSCREEN

	auto window = ofCreateWindow(settings);

	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();

}
