#include <stdint.h>
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

const int width = 1920;
const int height = 1080;
const int stride = width * 4;
const int shm_pool_size = height * stride;


static struct wl_compositor *g_compositor = NULL;
static struct xdg_wm_base *g_xdg_wm_base = NULL;

struct App {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	int configured;
	int running;
};


static struct wl_shm *g_shm = NULL;

struct wl_buffer *buffer;

uint32_t *pixels;


static void require(int condition, const char *msg){
	if(!condition) {
		fprintf(stderr, "fatal error: %s\n",msg);
		exit(1);
	} 
}

static void on_wm_base_ping(void *data, struct xdg_wm_base *wm, uint32_t serial) {
	xdg_wm_base_pong(wm, serial);
}
static const struct xdg_wm_base_listener WM_BASE_LISTENER = {
	.ping = on_wm_base_ping,
};

static void on_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *iface, uint32_t version) {
	if (strcmp(iface, "wl_compositor") == 0) {
		uint32_t v = version < 4 ? version : 4;
		g_compositor = wl_registry_bind(registry, name, &wl_compositor_interface, v);
	}
	else if (strcmp(iface, "xdg_wm_base") == 0) {
		g_xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(g_xdg_wm_base, &WM_BASE_LISTENER, NULL);
	}
	else if (strcmp(iface, "wl_shm")==0){
		g_shm = wl_registry_bind(registry,name,&wl_shm_interface,1);
	}
}
static void on_registry_remove(void *data, struct wl_registry *registry, uint32_t name) {
	(void)data; (void)registry; (void)name;
}
static const struct wl_registry_listener REGISTRY_LISTENER = {
	.global = on_registry_global,
	.global_remove = on_registry_remove,
};


static void on_toplevel_configure(void *data, struct xdg_toplevel *toplevel,int32_t width, int32_t height,struct wl_array *states) {
	(void)data; (void)toplevel; (void)width; (void)height; (void)states;
}
static void on_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
	struct App *app = data;
	app->running = 0;
	(void)toplevel;
}
static const struct xdg_toplevel_listener XDG_TOPLEVEL_LISTENER = {
	.configure = on_toplevel_configure,
	.close     = on_toplevel_close,
	.configure_bounds = NULL,
	.wm_capabilities  = NULL,
};

static const struct wl_callback_listener wl_surface_frame_listener;

static void wl_surface_frame_done(void* data, struct wl_callback *cb, uint32_t time){

	wl_callback_destroy(cb);
	struct App *app = data;
	cb = wl_surface_frame(app->surface);
	wl_callback_add_listener(cb,&wl_surface_frame_listener,app);


	for (int y = 0; y < height; ++y) {
	  for (int x = 0; x < width; ++x) {
	    if (((x + (time%8)) + (y+ (time%8)) / 8 * 8) % 16 < 8) {
	      pixels[y * width + x] = 0xFF666666;
	    } else {
	      pixels[y * width + x] = 0xFFEEEEEE;
	    }
	  }
	}

	wl_surface_attach(app->surface,buffer,0,0);
	wl_surface_damage(app->surface,0,0,UINT32_MAX,UINT32_MAX);
	wl_surface_commit(app->surface);

}

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial){
	struct App* app= data;
	xdg_surface_ack_configure(xdg_surface, serial);

	wl_surface_attach(app->surface, buffer, 0, 0);
	wl_surface_commit(app->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

int main(){
	struct App app;
	memset(&app,0,sizeof(app));

	app.running = 1;

	app.display = wl_display_connect(NULL);
	require((int)app.display, "wl_display_connect fail");

	app.registry = wl_display_get_registry(app.display);
	wl_registry_add_listener(app.registry, &REGISTRY_LISTENER,&app);

	wl_display_roundtrip(app.display);
	require((int)g_compositor, "wl_compositor not available");
	require((int)g_xdg_wm_base, "xdg-shell not available");

	app.surface = wl_compositor_create_surface(g_compositor);
	require((int)app.surface, "surface not created");

	app.xdg_surface = xdg_wm_base_get_xdg_surface(g_xdg_wm_base, app.surface);
	require((int)app.xdg_surface, "xdg surface retrieval failed");
	xdg_surface_add_listener(app.xdg_surface, &xdg_surface_listener, &app);

	app.toplevel = xdg_surface_get_toplevel(app.xdg_surface);
	require((int)app.toplevel, "requesting toplevel failed");
	xdg_toplevel_add_listener(app.toplevel, &XDG_TOPLEVEL_LISTENER, &app);


	xdg_toplevel_set_title(app.toplevel, "wayland window");
	xdg_toplevel_set_app_id(app.toplevel, "app.app");



	shm_unlink("wl_shm_temp");
	int fd = shm_open("wl_shm_temp", O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd>=0) {
		shm_unlink("wl_shm_temp");
	}
	if (fd<0) {
		printf("IM GONNA FUCKING KILL MYSELF\n");
	}

	ftruncate(fd,shm_pool_size);

	uint8_t* poolData = mmap(NULL, shm_pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	struct wl_shm_pool* pool = wl_shm_create_pool(g_shm,fd,shm_pool_size);

	buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);

	wl_surface_attach(app.surface,buffer,0,0);
	wl_surface_damage(app.surface,0,0,UINT32_MAX,UINT32_MAX);
	wl_surface_commit(app.surface);



	struct wl_callback *cb = wl_surface_frame(app.surface);
	wl_callback_add_listener(cb,&wl_surface_frame_listener,&app);	

	pixels = (uint32_t *)&poolData[0];

	while(app.running){

		if (wl_display_dispatch(app.display)==-1) {
			break;
		}

		if(app.configured){





		}
	}

	return -1;
}
