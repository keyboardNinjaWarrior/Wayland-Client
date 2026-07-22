/* 					*
 * Name: 	Nadeem Anwar           	*
 * Email: 	ibnul.aftab@proton.me 	*
 * 					*/

// TODO(Nadeem Anwar): plan error handling
// TODO(Nadeem Anwar): replace exit with proper function that deallocates memory
// XXX: DON'T TRY TO MAKE ERROR HANDLING IF STATEMENTS INTO A MACRO. EXTREMELY BAD IDEA. PLEASE BEAR WITH IT.

#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>

#include "xdg-shell-client-protocol.h"
#include "errors.h"
#include "debug.h"
#include "render.h"

#define WIDTH 	500
#define HEIGHT 	600

#define VAR(x)	#x

enum FLAGS {DEFAULT, CREATE, RESIZE};

static struct frame {	
	struct wl_buffer * buffer;
	struct wl_callback * callback;
	uint32_t * pixels;
	uint32_t width;
	uint32_t height;
	uint32_t size;	
	uint32_t stride; 
	int fd;
	bool free;
} frame = {0};

static struct state {	
	struct wl_display * display;		
	struct wl_event_queue * display_queue;
	struct wl_event_queue * render_queue;
	struct wl_compositor * compositor;
	struct wl_shm * shared_memory;
	struct xdg_wm_base * window_manager_base;
	struct wl_surface * wl_surface;
	struct xdg_surface * xdg_surface;
	struct xdg_toplevel * xdg_toplevel;
	struct frame * frame;
	enum FLAGS flag;
	bool running;
} state = {
	.display = NULL,
	.display_queue = NULL,
	.compositor = NULL,
	.shared_memory = NULL,
	.window_manager_base = NULL,
	.wl_surface = NULL,
	.xdg_surface = NULL,
	.xdg_toplevel = NULL,
	.running = false,
	.flag = DEFAULT,

	.frame = &frame
};

static void registery_global(void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version);
static void registery_global_remove(void * data, struct wl_registry * registery, uint32_t name);
static void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
static void xdg_surface_configure (void *data, struct xdg_surface *xdg_surface, uint32_t serial);
static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel);
static void xdg_toplevel_configure (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states);
static void xdg_toplevel_configure_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height);
static void xdg_toplevel_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities);
static void wl_callback_frame_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data);
static void wl_buffer_release (void *data, struct wl_buffer *wl_buffer);

static const struct xdg_wm_base_listener window_manager_base_listener = {
	.ping = &ping
};

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = &xdg_surface_configure 
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = &xdg_toplevel_configure,
	.close = &xdg_toplevel_close,
	.configure_bounds = xdg_toplevel_configure_bounds,
	.wm_capabilities = xdg_toplevel_wm_capabilities
};

static const struct wl_callback_listener wl_callback_listener = {
	.done = &wl_callback_frame_done
};

int main (void)
{
	START_BENCHMARK(1);
	
	state.display = wl_display_connect(NULL);
	
	if(! state.display)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.display" RESET " from " BOLD "wl_display_connect()" RESET);
		exit(ERR_DISPLAY);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.display" RESET " from " BOLD "wl_display_connect()" RESET);
	
	state.display_queue = wl_display_create_queue(state.display);

    	const struct wl_registry_listener registery_listener = {
		.global = &registery_global,
		.global_remove = &registery_global_remove
	};
	
	struct wl_registry * registry = wl_display_get_registry(state.display);
	struct wl_event_queue * registry_queue = wl_display_create_queue(state.display);
	wl_proxy_set_queue((struct wl_proxy *) registry, registry_queue);

	wl_registry_add_listener(registry, &registery_listener, NULL);
	
	START_BENCHMARK(2);
	
	// Fetches available global objects
	wl_display_roundtrip_queue(state.display, registry_queue);
	
	END_BENCHMARK(2, BOLD "wl_display_roundtrip_queue()" RESET);

	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.compositor, state.shared_memory, state.window_manager_base" RESET " in " BOLD "wl_display_roundtrip()" RESET);

	wl_registry_destroy(registry);
	wl_event_queue_destroy(registry_queue);

	xdg_wm_base_add_listener(state.window_manager_base, &window_manager_base_listener, NULL);

	state.wl_surface = wl_compositor_create_surface(state.compositor);

	if(! state.wl_surface)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.wl_surface" RESET " from " BOLD "wl_compositor_create_surface()" RESET);
		exit(ERR_WL_SURFACE);
	}

	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.wl_surface" RESET " from " BOLD "wl_display_connect()" RESET);

	state.xdg_surface = xdg_wm_base_get_xdg_surface(state.window_manager_base, state.wl_surface);
	
	if(! state.xdg_surface)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.wl_surface" RESET " from " BOLD "xdg_wm_base_get_xdg_surface()" RESET);
		exit(ERR_XDG_SURFACE);
	}

	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.xdg_surface" RESET " from " BOLD "xdg_wm_base_get_xdg_surface()" RESET);

	state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
	
	xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, NULL);
	
	if(! state.xdg_toplevel)
	{
		PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.xdg_toplevel" RESET " from " BOLD "xdg_surface_get_toplevel()" RESET);
		exit(ERR_XDG_SURFACE);
	}
	
	PRINT_LOG(SUCCESS, "Initialized " BOLD "state.xdg_toplevel" RESET " from " BOLD "xdg_surface_get_toplevel()" RESET);
	
	xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, NULL);
	
	xdg_toplevel_set_title(state.xdg_toplevel, "Wayland Client");

	wl_surface_commit(state.wl_surface);
	
	END_BENCHMARK(1, "before " BOLD "wl_display_dispatch_queue()" RESET);
	
	state.running = true;
	while(state.running && wl_display_dispatch_queue(state.display, state.display_queue) != -1);

	wl_buffer_destroy(frame.buffer);
	wl_callback_destroy(frame.callback);
	
	xdg_toplevel_destroy(state.xdg_toplevel);
	xdg_surface_destroy(state.xdg_surface);
	wl_surface_destroy(state.wl_surface);

	xdg_wm_base_destroy(state.window_manager_base);
	wl_shm_destroy(state.shared_memory);
	wl_compositor_destroy(state.compositor);

	wl_display_disconnect(state.display);

	if(munmap(frame.pixels, frame.size) < 0)
	{
		PRINT_LOG(FAIL, "Unable to free memory: " BOLD VAR(frame) RESET " having size = " BOLD "%d" RESET, frame.size);
		exit(ERR_MEM);
	}

	close(frame.fd);

	END_BENCHMARK(1, BOLD "%s()" RESET, __func__);
	
	return 0;
}

static void registery_global(void * data, struct wl_registry * registery, uint32_t name, const char * interface, uint32_t version)
{
	START_BENCHMARK(1);
	
	PRINT_LOG(LOG, "Found: " BOLD "%s" RESET, interface);
	
	if (! strcmp(interface, wl_compositor_interface.name))
	{
		state.compositor = wl_registry_bind(registery, name, &wl_compositor_interface, 4);
		
		if(! state.compositor)
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.compositor" RESET " from " BOLD "wl_registery_bind()" RESET);
			exit(ERR_COMPOSITOR);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD "state.compositor" RESET " from " BOLD "wl_registery_bind()" RESET);
		wl_proxy_set_queue((struct wl_proxy *) state.compositor, state.display_queue);

		END_BENCHMARK(1, BOLD "compositor:%s()" RESET, __func__);
		
		return;
	}

	if (! strcmp(interface, wl_shm_interface.name))
	{

		state.shared_memory = wl_registry_bind(registery, name, &wl_shm_interface, 1);
	
		if(! state.shared_memory)
		{
			PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.shared_memory" RESET " from " BOLD "wl_registery_bind()" RESET);
			exit(ERR_SHM);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD "state.shared_memory" RESET " from " BOLD "wl_registery_bind()" RESET);

		wl_proxy_set_queue((struct wl_proxy *) state.shared_memory, state.display_queue);
		
		END_BENCHMARK(1, BOLD "shared_memory:%s()" RESET, __func__);
		
		return;
	}

	if (! strcmp(interface, xdg_wm_base_interface.name))
	{

		state.window_manager_base = wl_registry_bind(registery, name, &xdg_wm_base_interface, 6);

		if(! state.window_manager_base)
		{ 
			PRINT_LOG(FAIL, "Unable to initialize " BOLD "state.window_manager_base" RESET " from " BOLD "wl_registery_bind()" RESET);
			exit(ERR_XDG_WM_BASE);
		}
	
		PRINT_LOG(SUCCESS, "Initialized " BOLD "state.window_manager_base" RESET " from " BOLD "wl_registery_bind()" RESET);	
		wl_proxy_set_queue((struct wl_proxy *) state.window_manager_base, state.display_queue);
		
		
		END_BENCHMARK(1, BOLD "window_manager:%s()" RESET, __func__);
		
		return;
	}
}

static void registery_global_remove(void * data, struct wl_registry * registery, uint32_t name) 
{
	PRINT_LOG(LOG, "Event dispatched...");	
}

void ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	PRINT_LOG(LOG, "Event dispatched...");	
	
	xdg_wm_base_pong(xdg_wm_base, serial);
}

int allocate_shm_file(size_t size)
{
	int fd = memfd_create("main", MFD_CLOEXEC);
	
	if (fd < 0)
	{
		PRINT_LOG(FAIL, "Unable to create anonymous file: " BOLD "main" RESET);
		
		exit(ERR_FILE);
	}
	
	PRINT_LOG(SUCCESS, "Created anonymous file: " BOLD "main" RESET);
	
	if(ftruncate(fd, size) != 0)
	{
		PRINT_LOG(FAIL, "Unable to truncate file " BOLD  "main" RESET " of size " BOLD "%d" RESET, size);
		close(fd);
		
		exit(ERR_FILE);
	}

	return fd;
}

// TODO(Nadeem Anwar): xdg_toplevel_configure: width == 0 and heingth == 0 not necisserily when the window is created 
void xdg_surface_configure (void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	START_BENCHMARK(1);

	PRINT_LOG(LOG, "Event dispatched...");
	
	xdg_surface_ack_configure(state.xdg_surface, serial);
	
	switch (state.flag)	
	{
		case CREATE:
			{

				PRINT_LOG(LOG, "Creating frame buffer...");
			
				frame.stride = frame.width * 4;
				frame.size = frame.stride * frame.height;

				PRINT_LOG(LOG, "Buffer size: " BOLD "%d" RESET " bytes", frame.size);

				frame.fd = allocate_shm_file(frame.size);
				frame.pixels = mmap(NULL, frame.size, PROT_READ | PROT_WRITE, MAP_SHARED, frame.fd, 0);
				
				if(frame.pixels == MAP_FAILED)
				{
					PRINT_LOG(FAIL, "Unable to map memory from the annonymous file to frame");
					close(frame.fd);

					exit(ERR_MEM);
				}

				PRINT_LOG(SUCCESS, "Mapped memory from the annonymous file to the frame");
			
				memset(frame.pixels, 0x00, frame.size);

				struct wl_shm_pool * pool = wl_shm_create_pool(state.shared_memory, frame.fd, frame.size);
				frame.buffer = wl_shm_pool_create_buffer(pool, 0, frame.width, frame.height, frame.stride, WL_SHM_FORMAT_ARGB8888);
				wl_shm_pool_destroy(pool);

				PRINT_LOG(LOG, "Created frame buffer: state.frame");

				static const struct wl_buffer_listener wl_buffer_listener = {
					.release = &wl_buffer_release
			};

				wl_buffer_add_listener(frame.buffer, &wl_buffer_listener, NULL);

				frame.free = true;
				
				PRINT_LOG(LOG, "frame.free = %d", frame.free);
				
				frame.callback = wl_surface_frame(state.wl_surface);
				wl_callback_add_listener(frame.callback, &wl_callback_listener, data);

				wl_surface_attach(state.wl_surface, frame.buffer, 0, 0);
				wl_surface_commit(state.wl_surface);

				state.flag = DEFAULT;
				
				END_BENCHMARK(1, "frame creation : " BOLD "%s()" RESET, __func__);
				
				return;
			}

		default: break;
	}
		
	wl_callback_destroy(frame.callback);
	frame.callback = wl_surface_frame(state.wl_surface);
	wl_callback_add_listener(frame.callback, &wl_callback_listener, data);
	
	wl_surface_attach(state.wl_surface, frame.buffer, 0, 0);
	wl_surface_commit(state.wl_surface);
	
	END_BENCHMARK(1, "%s()", __func__)
}

void xdg_toplevel_configure (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{
	PRINT_LOG(LOG, "Event dispatched...");
	PRINT_LOG(LOG, "Width: %d; Height: %d", width, height);

	if(width == 0 && height == 0) 
	{
		frame.width = WIDTH;
		frame.height = HEIGHT;

		state.flag = CREATE;
	}
	else 
	{
		frame.width = width;
		frame.height = height;
	}
}

void wl_callback_frame_done(void *data, struct wl_callback *wl_callback, uint32_t callback_data)
{
	#ifdef DEBUG 
	static uint32_t time = 0;
	if(time == 0)	time = callback_data;
	
	PRINT_LOG(BENCHMARK, "\u0394callback = %d ms", callback_data - time);
	
	time = callback_data;
	#endif

	wl_callback_destroy(frame.callback);
	frame.callback = wl_surface_frame(state.wl_surface);
	wl_callback_add_listener(frame.callback, &wl_callback_listener, data);
	
	if(! frame.free)
	{	
		PRINT_LOG(LOG, "frame.free = %d", frame.free);	
		return;
	}

	frame.free = false;
	PRINT_LOG(LOG, "frame.free = %d", frame.free);

	draw(frame.pixels, frame.height, frame.width);

	wl_surface_attach(state.wl_surface, frame.buffer, 0, 0);
	wl_surface_damage_buffer(state.wl_surface, 0, 0, frame.width, frame.height);
	wl_surface_commit(state.wl_surface);
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	PRINT_LOG(LOG, "Event dispatched...");

	state.running = false;
}

void xdg_toplevel_configure_bounds (void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height)
{
	PRINT_LOG(LOG, "Event dispatched...");
}

void xdg_toplevel_wm_capabilities (void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities)
{
	PRINT_LOG(LOG, "Event dispatched...");
}

void wl_buffer_release (void *data, struct wl_buffer *wl_buffer)
{
	PRINT_LOG(LOG, "Event dispatched...");

	frame.free = true;
	PRINT_LOG(LOG, "frame.free = %d", frame.free);
}
